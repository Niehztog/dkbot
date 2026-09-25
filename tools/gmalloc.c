/*
 * Guard-page malloc: preloaded before the shim, overruns and uses after free fault.
 *
 *   GMALLOC_ALIGN=1                  end blocks at the guard page, unaligned (box64 only)
 *   GMALLOC_WRITEONLY=1              only writes fault
 *   GMALLOC_EVERY=n GMALLOC_PHASE=k  guard only allocations i with i % n == k
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#define PG     4096UL
#define REGION (96UL << 30)   /* address space reserved for guarded blocks */
#define MAGIC  0x676d616c6c6f6321ULL
#define FREED  (MAGIC ^ 1)

struct hdr {
	unsigned long long magic;
	size_t size, len;
};

static char *region, *next_free;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static int noaccess = PROT_NONE;
static size_t align_min = 16;
static unsigned long every = 1, phase, ncalls;
static void *(*real_malloc)(size_t);
static void *(*real_calloc)(size_t, size_t);
static void *(*real_realloc)(void *, size_t);
static void (*real_free)(void *);
static size_t (*real_usable)(void *);

/* dlsym may allocate while init runs: that is served from here, never freed */
static char boot[1 << 16] __attribute__((aligned(16)));
static size_t boot_used;
static int initializing;
static pthread_t initializer;

static void die(const char *what, const void *ptr)
{
	fprintf(stderr, "gmalloc: %s %p\n", what, ptr);
	abort();
}

static int in_boot(const void *p)
{
	return (const char *)p >= boot && (const char *)p < boot + sizeof boot;
}

static int in_init(void)
{
	return initializing && pthread_equal(initializer, pthread_self());
}

static void *boot_alloc(size_t size)
{
	size_t need = 16 + ((size + 15) & ~(size_t)15);
	char *p;

	if (size > sizeof boot || boot_used + need > sizeof boot)
		die("bootstrap arena exhausted by", NULL);
	p = boot + boot_used;
	boot_used += need;
	*(size_t *)p = size;
	return p + 16;
}

static void init(void)
{
	const char *v;
	char *r;

	pthread_mutex_lock(&lock);
	if (region) {
		pthread_mutex_unlock(&lock);
		return;
	}
	initializer = pthread_self();
	initializing = 1;
	noaccess = getenv("GMALLOC_WRITEONLY") ? PROT_READ : PROT_NONE;
	if ((v = getenv("GMALLOC_ALIGN")) && atoi(v) >= 1 && atoi(v) <= (int)PG
	    && !(atoi(v) & (atoi(v) - 1)))
		align_min = (size_t)atoi(v);
	if ((v = getenv("GMALLOC_EVERY")) && atoi(v) > 1) {
		every = (unsigned long)atoi(v);
		phase = (v = getenv("GMALLOC_PHASE")) ? (unsigned long)atoi(v) % every : 0;
	}
	real_malloc = dlsym(RTLD_NEXT, "malloc");
	real_calloc = dlsym(RTLD_NEXT, "calloc");
	real_realloc = dlsym(RTLD_NEXT, "realloc");
	real_free = dlsym(RTLD_NEXT, "free");
	real_usable = dlsym(RTLD_NEXT, "malloc_usable_size");
	r = mmap(NULL, REGION, noaccess, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
	if (r == MAP_FAILED)
		die("cannot reserve address space", NULL);
	next_free = r;
	initializing = 0;
	region = r;
	pthread_mutex_unlock(&lock);
}

static int ours(const void *p)
{
	return region && (const char *)p >= region && (const char *)p < region + REGION;
}

static int unguarded(void)
{
	return every > 1 && __atomic_fetch_add(&ncalls, 1, __ATOMIC_RELAXED) % every != phase;
}

/* [header page][data pages, the block at their end][guard page] */
static void *g_alloc(size_t size, size_t align)
{
	size_t data, len;
	struct hdr *h;
	char *p;

	if (align > PG)
		die("no alignment above a page, asked", (void *)align);
	if (size > (1UL << 34)) {
		errno = ENOMEM;
		return NULL;
	}
	data = (size + PG - 1) & ~(PG - 1);
	len = PG + data + PG;
	pthread_mutex_lock(&lock);
	p = next_free;
	next_free += len;
	pthread_mutex_unlock(&lock);
	if (p + len > region + REGION)
		die("reserved address space exhausted at", p);
	if (mprotect(p, PG + data, PROT_READ | PROT_WRITE))
		die("out of memory mappings (vm.max_map_count; GMALLOC_EVERY) at", p);
	h = (struct hdr *)p;
	h->magic = MAGIC;
	h->size = size;
	h->len = len;
	return p + PG + ((data - size) & ~(align - 1));
}

static struct hdr *hdr_of(void *ptr)
{
	struct hdr *h = (struct hdr *)(((uintptr_t)ptr & ~(uintptr_t)(PG - 1)) - PG);

	if (h->magic == FREED)
		die("double free of", ptr);
	if (h->magic != MAGIC)
		die("free of a pointer that starts no block:", ptr);
	return h;
}

static size_t size_of(void *ptr)
{
	return in_boot(ptr) ? *(size_t *)((char *)ptr - 16) : hdr_of(ptr)->size;
}

void *malloc(size_t size)
{
	if (!region) {
		if (in_init())
			return boot_alloc(size);
		init();
	}
	return unguarded() ? real_malloc(size) : g_alloc(size, align_min);
}

void *calloc(size_t n, size_t s)
{
	if (s && n > (size_t)-1 / s) {
		errno = ENOMEM;
		return NULL;
	}
	if (!region) {
		if (in_init())
			return boot_alloc(n * s);
		init();
	}
	/* fresh pages, never reused: already zero */
	return unguarded() ? real_calloc(n, s) : g_alloc(n * s, align_min);
}

void free(void *ptr)
{
	struct hdr *h;
	size_t len;

	if (!ptr || in_boot(ptr))
		return;
	if (!region) {
		if (in_init())
			return;
		init();
	}
	if (!ours(ptr)) {
		real_free(ptr);
		return;
	}
	h = hdr_of(ptr);   /* without GMALLOC_WRITEONLY a double free faults right here */
	len = h->len;
	h->magic = FREED;
	madvise((char *)h + PG, len - 2 * PG, MADV_DONTNEED);
	mprotect(h, len, noaccess);
}

void *realloc(void *ptr, size_t size)
{
	size_t old;
	void *q;

	if (!ptr)
		return malloc(size);
	if (!region && !in_init())
		init();
	if (!ours(ptr) && !in_boot(ptr))
		return real_realloc(ptr, size);
	old = size_of(ptr);
	q = malloc(size);
	if (q)
		memcpy(q, ptr, old < size ? old : size);
	free(ptr);
	return q;
}

void *reallocarray(void *ptr, size_t n, size_t s)
{
	if (s && n > (size_t)-1 / s) {
		errno = ENOMEM;
		return NULL;
	}
	return realloc(ptr, n * s);
}

void *memalign(size_t align, size_t size)
{
	if (!region) {
		if (in_init())
			return boot_alloc(size);
		init();
	}
	return g_alloc(size, align > align_min ? align : align_min);
}

void *aligned_alloc(size_t align, size_t size)
{
	return memalign(align, size);
}

int posix_memalign(void **out, size_t align, size_t size)
{
	void *p = memalign(align, size);

	if (!p)
		return ENOMEM;
	*out = p;
	return 0;
}

void *valloc(size_t size)
{
	return memalign(PG, size);
}

void *pvalloc(size_t size)
{
	return memalign(PG, (size + PG - 1) & ~(PG - 1));
}

size_t malloc_usable_size(void *ptr)
{
	if (!ptr)
		return 0;
	if (!region && !in_init())
		init();
	if (!ours(ptr) && !in_boot(ptr))
		return real_usable ? real_usable(ptr) : 0;
	return size_of(ptr);
}
