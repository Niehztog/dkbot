/* Stands in for DLL_LoadDLLs, under the mangled name the shim keys on. */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>

int dll_Entry(void *h, unsigned int msg, void *data) { (void)h; (void)msg; (void)data; return 100; }
int weapons_dll_Entry(void *h, unsigned int msg, void *data) { (void)h; (void)msg; (void)data; return 101; }
int gce_dll_Entry(void *h, unsigned int msg, void *data) { (void)h; (void)msg; (void)data; return 102; }
void dll_SetStats(void *ent) { (void)ent; }

static void *slots[3];

static void *find_function(const char *name)
{
	for (int i = 0; i < 3; i++) {
		void *p;

		if (!slots[i])
			continue;
		p = dlsym(slots[i], name);
		if (p)
			return p;
	}
	return NULL;
}

void _Z12DLL_LoadDLLsPc(char *unused);
void _Z12DLL_LoadDLLsPc(char *unused)
{
	(void)unused;
	slots[0] = dlopen(NULL, RTLD_NOW);
	slots[1] = dlopen(NULL, RTLD_NOW);
	slots[2] = dlopen(NULL, RTLD_NOW);
}

int main(void)
{
	int (*entry)(void *, unsigned int, void *);
	int rc = 0, v;

	_Z12DLL_LoadDLLsPc(NULL);

	printf("slot0 %s slot1\n", slots[0] == slots[1] ? "==" : "!=");
	if (slots[0] == slots[1]) { puts("FAIL: slot 0 was not substituted"); rc = 1; }

	entry = find_function("dll_Entry");
	v = entry ? entry(slots[0], 12, NULL) : -1;
	printf("dll_Entry -> %d (expect 42 from the module)\n", v);
	if (v != 42) { puts("FAIL: override did not win"); rc = 1; }

	entry = find_function("weapons_dll_Entry");
	v = entry ? entry(slots[1], 12, NULL) : -1;
	printf("weapons_dll_Entry -> %d (expect 101 from the engine)\n", v);
	if (v != 101) { puts("FAIL: slot 1 lost its self-handle"); rc = 1; }

	printf("dll_SetStats fallback -> %s\n", find_function("dll_SetStats") ? "found" : "MISSING");
	if (!find_function("dll_SetStats")) { puts("FAIL: no fallback for undefined names"); rc = 1; }

	puts(rc ? "shim self-test FAILED" : "shim self-test passed");
	return rc;
}
