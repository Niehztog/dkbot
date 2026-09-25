#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdint.h>
#include <string.h>

#include "dk_platform.h"

void *dk_plat_resolve(const char *name)
{
	Dl_info self, found;
	void *p = dlsym(RTLD_DEFAULT, name);

	/* Our overrides win the default scope; the shim substitutes only slot 0's dlopen(NULL). */
	if (p && dladdr((void *)(uintptr_t)&dk_plat_resolve, &self) && dladdr(p, &found)
	    && self.dli_fbase == found.dli_fbase) {
		void *exe = dlopen(NULL, RTLD_NOW);

		p = exe ? dlsym(exe, name) : NULL;
	}
	return p;
}

void *dk_plat_dlopen(const char *path)
{
	return dlopen(path, RTLD_NOW | RTLD_LOCAL);
}

void *dk_plat_dlsym(void *handle, const char *name)
{
	return dlsym(handle, name);
}

void dk_plat_dlclose(void *handle)
{
	dlclose(handle);
}

const char *dk_plat_dlerror(void)
{
	const char *e = dlerror();

	return e ? e : "";
}

int dk_plat_self_dir(char *buf, size_t n)
{
	Dl_info info;
	const char *slash;
	size_t len;

	if (!dladdr((void *)(uintptr_t)&dk_plat_self_dir, &info) || !info.dli_fname)
		return 1;
	slash = strrchr(info.dli_fname, '/');
	if (!slash)
		return 1;
	len = (size_t)(slash - info.dli_fname);
	if (len + 1 > n)
		return 1;
	memcpy(buf, info.dli_fname, len);
	buf[len] = 0;
	return 0;
}

void dk_plat_init(void)
{
}
