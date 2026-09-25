#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dbghelp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dk_platform.h"
#include "dk/dk_symnames.h"

/* From entry.c, declared loosely (dk_boundary.h clashes with <windows.h>): only addresses taken. */
int  dll_Entry(void *hParent, unsigned int msg, void *pvData);
int  dll_ClientConnect(void *self, void *userinfo, int loadgame);

static const struct { const char *name; void *fn; } g_overrides[] = {
	{ "dll_Entry",         (void *)(uintptr_t)dll_Entry },
	{ "dll_ClientConnect", (void *)(uintptr_t)dll_ClientConnect },
};

static HMODULE g_self;
static HMODULE g_exe;
static FARPROC (WINAPI *g_real_gpa)(HMODULE, LPCSTR);

static FARPROC WINAPI hook_GetProcAddress(HMODULE mod, LPCSTR name)
{
	/* a name <= 0xffff is an ordinal, not a string */
	if (mod == g_exe && (ULONG_PTR)name > 0xffff) {
		size_t i;

		for (i = 0; i < sizeof g_overrides / sizeof g_overrides[0]; i++)
			if (!strcmp(name, g_overrides[i].name))
				return (FARPROC)g_overrides[i].fn;
	}
	return g_real_gpa(mod, name);
}

static int patch_iat(HMODULE base, const char *dll, const char *func,
                     void *newfn, void **oldfn)
{
	IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)base;
	IMAGE_NT_HEADERS *nt;
	IMAGE_DATA_DIRECTORY *dir;
	IMAGE_IMPORT_DESCRIPTOR *imp;
	BYTE *b = (BYTE *)base;

	if (dos->e_magic != IMAGE_DOS_SIGNATURE)
		return 0;
	nt = (IMAGE_NT_HEADERS *)(b + dos->e_lfanew);
	if (nt->Signature != IMAGE_NT_SIGNATURE)
		return 0;
	dir = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
	if (!dir->VirtualAddress)
		return 0;
	for (imp = (IMAGE_IMPORT_DESCRIPTOR *)(b + dir->VirtualAddress);
	     imp->Name; imp++) {
		IMAGE_THUNK_DATA *oft, *ft;

		if (_stricmp((const char *)(b + imp->Name), dll))
			continue;
		oft = (IMAGE_THUNK_DATA *)(b + (imp->OriginalFirstThunk
		                               ? imp->OriginalFirstThunk
		                               : imp->FirstThunk));
		ft = (IMAGE_THUNK_DATA *)(b + imp->FirstThunk);
		for (; oft->u1.AddressOfData; oft++, ft++) {
			IMAGE_IMPORT_BY_NAME *ibn;
			DWORD prot;

			if (oft->u1.Ordinal & IMAGE_ORDINAL_FLAG)
				continue;
			ibn = (IMAGE_IMPORT_BY_NAME *)(b + oft->u1.AddressOfData);
			if (strcmp((const char *)ibn->Name, func))
				continue;
			if (!VirtualProtect(&ft->u1.Function, sizeof ft->u1.Function,
			                    PAGE_READWRITE, &prot))
				return 0;
			if (oldfn)
				*oldfn = (void *)(uintptr_t)ft->u1.Function;
			ft->u1.Function = (uintptr_t)newfn;
			VirtualProtect(&ft->u1.Function, sizeof ft->u1.Function, prot, &prot);
			return 1;
		}
	}
	return 0;
}

static const struct { const char *key; const char *pdb; } g_symmap[] = {
	{ "gi",                   "?gi@@3Ugame_import_t@@A" },
	{ "globals",              "?globals@@3Ugame_export_s@@A" },
	{ "maxclients",           "?maxclients@@3PEAUcvar_s@@EA" },
	{ "sv",                   "?sv@@3Userver_t@@A" },
	{ "svs",                  "?svs@@3Userver_static_t@@A" },
	{ "com",                  "?com@@3PEAUcommon_export_s@@EA" },
	{ DK_SYM_CLIENT_THINK,    "?Client_Think@@YAXPEAUedict_s@@PEAUusercmd_s@@@Z" },
	{ DK_SYM_RUN_FRAME,       "?P_RunFrame@@YAXXZ" },
	{ DK_SYM_CVAR_STRING,     "?Cvar_VariableString@@YAPEBDPEBD@Z" },
	{ DK_SYM_CMD_ADDCOMMAND,  "?Cmd_AddCommand@@YAXPEBDP6AXXZ@Z" },
	{ DK_SYM_GET_PLAYER_HOOK, "?P_GetPlayerHook@@YAPEAUplayerHook_s@@PEAUedict_s@@@Z" },
	{ DK_SYM_WEAPON_SELECT,   "?weaponSelect@@YAFPEAUedict_s@@PEBUweaponInfo_s@@@Z" },
};

static int    g_sym_state;
static HANDLE g_proc;

static int ensure_sym(void)
{
	if (g_sym_state)
		return g_sym_state > 0;
	g_proc = GetCurrentProcess();
	/* No SYMOPT_UNDNAME: g_symmap holds decorated names. */
	SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_FAIL_CRITICAL_ERRORS);
	g_sym_state = SymInitialize(g_proc, NULL, TRUE) ? 1 : -1;
	return g_sym_state > 0;
}

static void *sym_from_pdb(const char *decorated)
{
	char buf[sizeof(SYMBOL_INFO) + 512];
	SYMBOL_INFO *si = (SYMBOL_INFO *)buf;

	if (!ensure_sym())
		return NULL;
	memset(buf, 0, sizeof buf);
	si->SizeOfStruct = sizeof(SYMBOL_INFO);
	si->MaxNameLen = 511;
	if (!SymFromName(g_proc, decorated, si))
		return NULL;
	return (void *)(uintptr_t)si->Address;
}

void *dk_plat_resolve(const char *name)
{
	size_t i;

	/* The unhooked GetProcAddress: the exe's originals, never our overrides. */
	if (g_exe) {
		FARPROC p = (g_real_gpa ? g_real_gpa : GetProcAddress)(g_exe, name);

		if (p)
			return (void *)p;
	}
	for (i = 0; i < sizeof g_symmap / sizeof g_symmap[0]; i++)
		if (!strcmp(name, g_symmap[i].key))
			return sym_from_pdb(g_symmap[i].pdb);
	return NULL;
}

void *dk_plat_dlopen(const char *path)
{
	return (void *)LoadLibraryA(path);
}

void *dk_plat_dlsym(void *handle, const char *name)
{
	return (void *)(uintptr_t)GetProcAddress((HMODULE)handle, name);
}

void dk_plat_dlclose(void *handle)
{
	if (handle)
		FreeLibrary((HMODULE)handle);
}

const char *dk_plat_dlerror(void)
{
	static char buf[256];
	DWORD e = GetLastError();

	buf[0] = 0;
	FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
	               NULL, e, 0, buf, sizeof buf, NULL);
	if (!buf[0])
		snprintf(buf, sizeof buf, "error %lu", (unsigned long)e);
	return buf;
}

int dk_plat_self_dir(char *out, size_t n)
{
	char path[MAX_PATH];
	char *slash;
	size_t len;

	if (!GetModuleFileNameA(g_self, path, sizeof path))
		return 1;
	slash = strrchr(path, '\\');
	if (!slash)
		return 1;
	len = (size_t)(slash - path);
	if (len + 1 > n)
		return 1;
	memcpy(out, path, len);
	out[len] = 0;
	return 0;
}

void dk_plat_init(void)
{
	const char *log = getenv("DK_BOT_LOG");

	/* No console on a listen server: open one or a log file, only when asked. */
	if (log && *log) {
		if (freopen(log, "w", stderr))
			setvbuf(stderr, NULL, _IONBF, 0);
		return;
	}
	if (getenv("DK_BOT_VERBOSE") || getenv("DK_BOT_CONSOLE")) {
		if (AllocConsole()) {
			freopen("CONOUT$", "w", stdout);
			freopen("CONOUT$", "w", stderr);
			SetConsoleTitleA("dkbot");
		}
	}
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
	(void)reserved;
	if (reason == DLL_PROCESS_ATTACH) {
		DisableThreadLibraryCalls(inst);
		g_self = inst;
		g_exe = GetModuleHandleW(NULL);
		g_real_gpa = GetProcAddress;
		if (!patch_iat(g_exe, "KERNEL32.dll", "GetProcAddress",
		               (void *)(uintptr_t)hook_GetProcAddress,
		               (void **)&g_real_gpa))
			OutputDebugStringA("[dkbot] could not hook GetProcAddress -- "
			                   "bots unavailable\n");
	}
	return TRUE;
}
