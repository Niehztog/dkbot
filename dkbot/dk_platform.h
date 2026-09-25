/* Plain C types only: dk_win.c includes this beside <windows.h>. */
#ifndef DK_PLATFORM_H
#define DK_PLATFORM_H

#include <stddef.h>

/* The engine's definition, never our override; NULL if this build lacks it. */
void *dk_plat_resolve(const char *name);

void       *dk_plat_dlopen(const char *path);
void       *dk_plat_dlsym(void *handle, const char *name);
void        dk_plat_dlclose(void *handle);
const char *dk_plat_dlerror(void);

/* This module's directory, without a trailing separator; 0 on success. */
int dk_plat_self_dir(char *buf, size_t n);

#ifdef _WIN32
#define DK_BOTLIB_NAME "gladiator_x64.dll"
#define DK_PATH_SEP    "\\"
#else
#define DK_BOTLIB_NAME "gladiator_x64.so"
#define DK_PATH_SEP    "/"
#endif

void dk_plat_init(void);

#endif
