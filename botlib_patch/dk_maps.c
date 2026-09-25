/* dkbot: the engine's map in Quake II's layout; must match tools/dkbsp.py convert byte for byte */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
__declspec(dllimport) unsigned long __stdcall GetTempPathA(unsigned long n, char *buf);
__declspec(dllimport) unsigned int  __stdcall GetTempFileNameA(const char *dir, const char *prefix,
                                                               unsigned int unique, char *name);
__declspec(dllimport) int           __stdcall DeleteFileA(const char *name);
#endif

#define DK_PREFIX      "engine:"
#define DK_VERSION     41
#define Q2_VERSION     38
#define DK_LUMPS       21
#define Q2_LUMPS       19
#define LUMP_ENTITIES  0
#define LUMP_LIGHTING  7
#define LUMP_LEAFS     8
#define LUMP_BRUSHES   14
#define DK_LEAF_SIZE   32
#define Q2_LEAF_SIZE   28
#define BRUSH_SIZE     12
#define MAX_LIGHTING   3276800

#define CONTENTS_SOLID    0x1u
#define CONTENTS_LAVA     0x8u
#define CONTENTS_WATER    0x20u
#define CONTENTS_LADDER   0x20000000u
#define DK_HARMFUL_WATER  0x800u
#define DK_PLAYERSOLID    (0x80u | 0x200u)

static int  (*fs_load)(const char *, void **);
static void (*fs_free)(void *);
static unsigned char *image;
static size_t image_size;

void DK_SetFileSystem(int (*load)(const char *, void **), void (*release)(void *))
{
	fs_load = load;
	fs_free = release;
}

static unsigned rd32(const unsigned char *p)
{
	return p[0] | p[1] << 8 | p[2] << 16 | (unsigned)p[3] << 24;
}

static void wr32(unsigned char *p, unsigned v)
{
	p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24;
}

/* Python's str.splitlines() and \s, for text decoded as latin-1 */
static int line_break(unsigned char c)
{
	return c == '\n' || c == '\r' || c == 0x0b || c == 0x0c || (c >= 0x1c && c <= 0x1e) || c == 0x85;
}

static int py_space(unsigned char c)
{
	return (c >= 0x09 && c <= 0x0d) || (c >= 0x1c && c <= 0x20) || c == 0x85 || c == 0xa0;
}

/* A "model" key naming an external model, which BSPC cannot parse. */
static int named_model(const unsigned char *s, size_t n)
{
	size_t i = 0, j;

	while (i < n && py_space(s[i]))
		i++;
	if (n - i < 7 || memcmp(s + i, "\"model\"", 7))
		return 0;
	i += 7;
	j = i;
	while (i < n && py_space(s[i]))
		i++;
	if (i == j || i >= n || s[i] != '"')
		return 0;
	for (j = ++i; j < n && s[j] != '"'; j++)
		;
	if (j >= n)
		return 0;
	return !(j > i && s[i] == '*');
}

static unsigned char *fix_entities(const unsigned char *src, size_t n, size_t *outn)
{
	unsigned char *out = malloc(n + 2), *p = out;
	size_t len = 0, start = 0, pos;
	int first = 1;

	if (!out)
		return NULL;
	while (len < n && src[len])
		len++;
	for (pos = 0; pos <= len; pos++) {
		size_t end = pos;

		if (pos < len && !line_break(src[pos]))
			continue;
		if (pos == len && start == len)
			break;
		if (pos < len && src[pos] == '\r' && pos + 1 < len && src[pos + 1] == '\n')
			pos++;
		if (!named_model(src + start, end - start)) {
			if (!first)
				*p++ = '\n';
			memcpy(p, src + start, end - start);
			p += end - start;
			first = 0;
		}
		start = pos + 1;
	}
	*p++ = '\n';
	*p++ = 0;
	*outn = p - out;
	return out;
}

static void fix_contents(unsigned char *blob, size_t n, size_t size, size_t offset)
{
	size_t i;

	for (i = 0; i < n / size; i++) {
		unsigned char *q = blob + i * size + offset;
		unsigned c = rd32(q);

		if ((c & CONTENTS_WATER) && (c & DK_HARMFUL_WATER))
			c |= CONTENTS_LAVA;
		if ((c & DK_PLAYERSOLID) && !(c & (CONTENTS_SOLID | CONTENTS_LADDER)))
			c |= CONTENTS_SOLID;
		wr32(q, c);
	}
}

static unsigned char *convert(const unsigned char *src, size_t n, size_t *outn)
{
	const unsigned char *piece[Q2_LUMPS];
	unsigned char *own[Q2_LUMPS] = { 0 }, *out = NULL, *p;
	size_t len[Q2_LUMPS], total = 8 + 8 * Q2_LUMPS;
	int i;

	if (n < 8 + 8 * DK_LUMPS || memcmp(src, "IBSP", 4))
		return NULL;
	if (rd32(src + 4) == Q2_VERSION) {
		if ((out = malloc(n)))
			memcpy(out, src, *outn = n);
		return out;
	}
	if (rd32(src + 4) != DK_VERSION)
		return NULL;
	for (i = 0; i < Q2_LUMPS; i++) {
		size_t ofs = rd32(src + 8 + 8 * i), ln = rd32(src + 12 + 8 * i), k;

		if (ofs > n || ln > n - ofs)
			goto fail;
		piece[i] = src + ofs;
		len[i] = ln;
		if (i == LUMP_ENTITIES) {
			if (!(own[i] = fix_entities(src + ofs, ln, &len[i])))
				goto fail;
		} else if (i == LUMP_LIGHTING && ln > MAX_LIGHTING) {
			len[i] = 0;
		} else if (i == LUMP_LEAFS) {
			if (ln % DK_LEAF_SIZE || !(own[i] = malloc(ln / DK_LEAF_SIZE * Q2_LEAF_SIZE + 1)))
				goto fail;
			for (k = 0; k < ln / DK_LEAF_SIZE; k++)
				memcpy(own[i] + k * Q2_LEAF_SIZE, src + ofs + k * DK_LEAF_SIZE, Q2_LEAF_SIZE);
			len[i] = ln / DK_LEAF_SIZE * Q2_LEAF_SIZE;
			fix_contents(own[i], len[i], Q2_LEAF_SIZE, 0);
		} else if (i == LUMP_BRUSHES) {
			if (!(own[i] = malloc(ln + 1)))
				goto fail;
			memcpy(own[i], src + ofs, ln);
			fix_contents(own[i], ln, BRUSH_SIZE, 8);
		}
		if (own[i])
			piece[i] = own[i];
		total += (len[i] + 3) & ~(size_t)3;
	}
	if (!(out = calloc(1, total)))
		goto fail;
	memcpy(out, "IBSP", 4);
	wr32(out + 4, Q2_VERSION);
	p = out + 8 + 8 * Q2_LUMPS;
	for (i = 0; i < Q2_LUMPS; i++) {
		wr32(out + 8 + 8 * i, (unsigned)(p - out));
		wr32(out + 12 + 8 * i, (unsigned)len[i]);
		memcpy(p, piece[i], len[i]);
		p += (len[i] + 3) & ~(size_t)3;
	}
	*outn = total;
fail:
	for (i = 0; i < Q2_LUMPS; i++)
		free(own[i]);
	return out;
}

/* The engine's copy of maps/<mapname>.bsp; path gets a name only DK_OpenBSP opens. */
int DK_FindBSP(const char *mapname, char *path, int size)
{
	char name[160];
	void *raw = NULL;
	int n;

	if (!fs_load || !fs_free || strlen(mapname) > 128)
		return 0;
	snprintf(name, sizeof name, "maps/%s.bsp", mapname);
	n = fs_load(name, &raw);
	if (n <= 0 || !raw)
		return 0;
	free(image);
	image = convert(raw, (size_t)n, &image_size);
	fs_free(raw);
	if (!image)
		return 0;
	snprintf(path, size, DK_PREFIX "%s", name);
	return 1;
}

FILE *DK_OpenBSP(const char *name)
{
	if (strncmp(name, DK_PREFIX, sizeof DK_PREFIX - 1))
		return fopen(name, "rb");
	if (!image)
		return NULL;
#ifdef _WIN32
	{
		/* msvcrt has no fmemopen; "D" deletes the file when it is closed */
		char dir[260], tmp[260];
		FILE *f;

		if (!GetTempPathA(sizeof dir, dir) || !GetTempFileNameA(dir, "dkb", 0, tmp))
			return NULL;
		if (!(f = fopen(tmp, "w+bD"))) {
			DeleteFileA(tmp);
			return NULL;
		}
		if (fwrite(image, 1, image_size, f) != image_size) {
			fclose(f);
			return NULL;
		}
		rewind(f);
		return f;
	}
#else
	return fmemopen(image, image_size, "r");
#endif
}
