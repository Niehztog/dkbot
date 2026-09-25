/* LD_PRELOAD shim: hands DK_MOD to the engine as its slot-0 (world) module. */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define DK_LOADER_SYM "_Z12DLL_LoadDLLsPc"

static void *(*real_dlopen)(const char *, int);
static const void *world_site;
static void *mod_handle;
static const char *mod_path;
static int mod_flags = RTLD_NOW | RTLD_LOCAL;
static int verbose;

static void trace(const char *fmt, ...)
{
	va_list ap;

	if (!verbose)
		return;
	va_start(ap, fmt);
	fputs("[dk_preload] ", stderr);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

__attribute__((constructor)) static void dk_preload_init(void)
{
	/* Under box64, stdbuf -oL misses the emulated libc: line-buffer here. */
	setvbuf(stdout, NULL, _IOLBF, 0);
	real_dlopen = dlsym(RTLD_NEXT, "dlopen");
	mod_path = getenv("DK_MOD");
	verbose = getenv("DK_SHIM_VERBOSE") != NULL;
	if (getenv("DK_MOD_GLOBAL"))
		mod_flags = RTLD_NOW | RTLD_GLOBAL;

	if (!mod_path || !*mod_path)
		fputs("[dk_preload] DK_MOD is unset -- shim is inert\n", stderr);
	else
		trace("armed, module=%s\n", mod_path);
}

/* DK_SEED: time(), which seeds rand(), becomes a per-seed base plus sv.time (ms, sv + 0xc). */
time_t time(time_t *t)
{
	static time_t (*real_time)(time_t *);
	static const unsigned *sv_time;
	static long base = -1;
	time_t now;

	if (base < 0) {
		const char *s = getenv("DK_SEED");

		base = s && *s ? 1700000000L + atol(s) * 100000L : 0;
		real_time = dlsym(RTLD_NEXT, "time");
	}
	if (!base)
		return real_time ? real_time(t) : (time_t)-1;
	if (!sv_time) {
		const char *sv = dlsym(RTLD_DEFAULT, "sv");

		sv_time = sv ? (const unsigned *)(sv + 0xc) : NULL;
	}
	now = base + (sv_time ? (time_t)(*sv_time / 1000) : 0);
	if (t)
		*t = now;
	return now;
}

static int from_module_loader(const void *ra)
{
	Dl_info info;

	if (!dladdr((void *)ra, &info) || !info.dli_sname)
		return 0;
	return strcmp(info.dli_sname, DK_LOADER_SYM) == 0;
}

void *dlopen(const char *file, int mode)
{
	const void *ra = __builtin_return_address(0);

	if (!real_dlopen)
		real_dlopen = dlsym(RTLD_NEXT, "dlopen");

	if (file != NULL || !mod_path || !*mod_path)
		return real_dlopen(file, mode);

	/* Pin the call site, not a count: dedicated servers skip GCE, and the loader can run again. */
	if (!world_site && from_module_loader(ra)) {
		world_site = ra;
		trace("pinned world slot at return address %p\n", ra);
	}

	if (ra != world_site)
		return real_dlopen(file, mode);

	if (!mod_handle) {
		mod_handle = real_dlopen(mod_path, mod_flags);
		if (!mod_handle) {
			fprintf(stderr, "[dk_preload] failed to load %s: %s\n"
			                "[dk_preload] falling back to the stock world module\n",
			        mod_path, dlerror());
			return real_dlopen(file, mode);
		}
		trace("loaded %s as slot 0 (handle %p)\n", mod_path, mod_handle);
	}

	return mod_handle;
}
