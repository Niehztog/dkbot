#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

/* PE timestamp of the v12-21-2025 x64 daikatana.exe; tools/check-pdb.py compares it. */
#define SUPPORTED_BUILD 0x6947fa9eu

/* Terminated on failure, so a game held suspended is never left behind. */
static HANDLE g_child;

/* A dialog too: started from Explorer, the console closes as the launcher exits. */
static void fail(const wchar_t *fmt, ...)
{
	wchar_t msg[1024];
	va_list ap;

	va_start(ap, fmt);
	_vsnwprintf(msg, 1023, fmt, ap);
	va_end(ap);
	msg[1023] = 0;
	fwprintf(stderr, L"[launcher] %ls\n", msg);
	fflush(stderr);
	if (g_child)
		TerminateProcess(g_child, 1);
	MessageBoxW(NULL, msg, L"dkbot", MB_OK | MB_ICONERROR);
	exit(1);
}

static void diew(const wchar_t *what)
{
	DWORD e = GetLastError();
	wchar_t msg[256];

	msg[0] = 0;
	FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
	               NULL, e, 0, msg, 256, NULL);
	fail(L"%ls failed (%lu): %ls", what, (unsigned long)e, msg);
}

static void self_dir(wchar_t *out, DWORD n)
{
	wchar_t *slash;

	if (!GetModuleFileNameW(NULL, out, n))
		diew(L"GetModuleFileName");
	slash = wcsrchr(out, L'\\');
	if (slash)
		slash[1] = 0;
}

static int file_exists(const wchar_t *p)
{
	DWORD a = GetFileAttributesW(p);

	return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

static int dir_exists(const wchar_t *p)
{
	DWORD a = GetFileAttributesW(p);

	return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

static int pe_header(const wchar_t *path, IMAGE_FILE_HEADER *fh)
{
	IMAGE_DOS_HEADER dos;
	DWORD sig = 0;
	FILE *f = _wfopen(path, L"rb");
	int ok;

	if (!f)
		return 0;
	ok = fread(&dos, sizeof dos, 1, f) == 1 && dos.e_magic == IMAGE_DOS_SIGNATURE &&
	     !fseek(f, dos.e_lfanew, SEEK_SET) && fread(&sig, sizeof sig, 1, f) == 1 &&
	     sig == IMAGE_NT_SIGNATURE && fread(fh, sizeof *fh, 1, f) == 1;
	fclose(f);
	return ok;
}

/* SYMSRV_INDEX_INFOW, which MinGW's dbghelp.h lacks */
typedef struct {
	DWORD sizeofstruct;
	WCHAR file[MAX_PATH + 1];
	BOOL  stripped;
	DWORD timestamp;
	DWORD size;
	WCHAR dbgfile[MAX_PATH + 1];
	WCHAR pdbfile[MAX_PATH + 1];
	GUID  guid;
	DWORD sig;
	DWORD age;
} index_info_t;
typedef BOOL (WINAPI *index_fn)(PCWSTR, index_info_t *, DWORD);

/* An image and its PDB share the GUID and age of dbghelp's symbol-server index. */
static void check_pdb(const wchar_t *game)
{
	wchar_t pdb[MAX_PATH + 8], *dot, *slash;
	index_info_t exe_ix, pdb_ix;
	HMODULE dbghelp;
	index_fn get_index;

	wcscpy(pdb, game);
	dot = wcsrchr(pdb, L'.');
	slash = wcsrchr(pdb, L'\\');
	if (dot && (!slash || dot > slash))
		*dot = 0;
	wcscat(pdb, L".pdb");
	if (!file_exists(pdb))
		fail(L"%ls is missing.\n\nThe Daikatana 1.3 x64 installer puts it beside the game; "
		     L"the release also has it alone, as DK_PDB_<date>_x64.7z.", pdb);
	dbghelp = LoadLibraryW(L"dbghelp.dll");
	if (!dbghelp)
		return;
	get_index = (index_fn)(void *)GetProcAddress(dbghelp, "SymSrvGetFileIndexInfoW");
	memset(&exe_ix, 0, sizeof exe_ix);
	memset(&pdb_ix, 0, sizeof pdb_ix);
	exe_ix.sizeofstruct = pdb_ix.sizeofstruct = sizeof(index_info_t);
	if (get_index && get_index(game, &exe_ix, 0) && get_index(pdb, &pdb_ix, 0) &&
	    (memcmp(&exe_ix.guid, &pdb_ix.guid, sizeof(GUID)) || exe_ix.age != pdb_ix.age))
		fail(L"%ls does not belong to %ls.\n\nTake both from the same Daikatana 1.3 release.",
		     pdb, game);
}

/* botlib cuts paths at 144 characters, including <gamedir>/maps/<map>.aas. */
static void check_botdata(void)
{
	wchar_t base[MAX_PATH], game[64], chr[MAX_PATH], probe[MAX_PATH * 2];

	if (!GetEnvironmentVariableW(L"DK_BOTLIB_BASEDIR", base, MAX_PATH)
	    || !GetEnvironmentVariableW(L"DK_BOTLIB_GAMEDIR", game, 64)
	    || !GetEnvironmentVariableW(L"DK_BOT_CHARFILE", chr, MAX_PATH))
		return;
	_snwprintf(probe, MAX_PATH * 2, L"%ls\\%ls\\%ls", base, game, chr);
	if (!file_exists(probe))
		fwprintf(stderr, L"[launcher] WARNING: no %ls -- the bots will have no AI.\n"
		         L"  Extract the whole dkbot package into the game folder, or set\n"
		         L"  DK_BOTLIB_BASEDIR.\n", probe);
	else if (wcslen(base) + wcslen(game) + 32 > 143)
		fwprintf(stderr, L"[launcher] WARNING: %ls is too long for the bot library,\n"
		         L"  which cuts its paths at 144 characters; move botdata\\ higher up.\n",
		         base);
	fflush(stderr);
}

static void default_env(const wchar_t *name, const wchar_t *value)
{
	if (!GetEnvironmentVariableW(name, NULL, 0) &&
	    GetLastError() == ERROR_ENVVAR_NOT_FOUND)
		SetEnvironmentVariableW(name, value);
}

static const wchar_t *arg_tail(const wchar_t *cl)
{
	if (*cl == L'"') {
		for (cl++; *cl && *cl != L'"'; cl++)
			;
		if (*cl == L'"')
			cl++;
	} else {
		for (; *cl && *cl != L' ' && *cl != L'\t'; cl++)
			;
	}
	while (*cl == L' ' || *cl == L'\t')
		cl++;
	return cl;
}

int main(void)
{
	wchar_t here[MAX_PATH], game[MAX_PATH], mod[MAX_PATH];
	wchar_t gamedir[MAX_PATH], *slash;
	wchar_t cmdline[8192], *remote;
	const wchar_t *env, *tail;
	IMAGE_FILE_HEADER fh;
	STARTUPINFOW si;
	PROCESS_INFORMATION pi;
	HMODULE k32;
	LPTHREAD_START_ROUTINE loadlib;
	HANDLE thread;
	SIZE_T bytes;
	DWORD loaded = 0;

	self_dir(here, MAX_PATH);

	env = _wgetenv(L"DK_GAME");
	if (env && *env) {
		wcsncpy(game, env, MAX_PATH - 1);
		game[MAX_PATH - 1] = 0;
	} else {
		_snwprintf(game, MAX_PATH, L"%lsdaikatana.exe", here);
	}
	if (!file_exists(game))
		fail(L"daikatana.exe not found (%ls).\n\nExtract dkbot into the Daikatana 1.3 folder, "
		     L"or set DK_GAME.", game);
	if (!pe_header(game, &fh) || fh.Machine != IMAGE_FILE_MACHINE_AMD64)
		fail(L"%ls is not a 64-bit program; dkbot needs the x64 build of Daikatana 1.3.",
		     game);

	env = _wgetenv(L"DK_MOD");
	if (env && *env) {
		wcsncpy(mod, env, MAX_PATH - 1);
		mod[MAX_PATH - 1] = 0;
	} else {
		_snwprintf(mod, MAX_PATH, L"%lsdkbot.dll", here);
	}
	if (!file_exists(mod))
		fail(L"dkbot.dll not found (%ls).\n\nKeep it beside the launcher, or set DK_MOD.", mod);

	/* Run the game in its own directory so it finds data/. */
	wcscpy(gamedir, game);
	slash = wcsrchr(gamedir, L'\\');
	if (slash)
		*slash = 0;

	check_pdb(game);
	if (fh.TimeDateStamp != SUPPORTED_BUILD) {
		fwprintf(stderr, L"[launcher] WARNING: %ls is not the v12-21-2025 build.\n", game);
		fflush(stderr);
		if (MessageBoxW(NULL, L"This daikatana.exe is not the v12-21-2025 build dkbot "
		                L"supports. Bots may not work, or may crash the game.\n\n"
		                L"Start anyway?", L"dkbot", MB_YESNO | MB_ICONWARNING) == IDNO)
			return 1;
	}

	{
		wchar_t botdata[MAX_PATH], up[MAX_PATH];

		_snwprintf(botdata, MAX_PATH, L"%lsbotdata", here);
		_snwprintf(up, MAX_PATH, L"%ls..\\botdata", here);
		if (!dir_exists(botdata) && dir_exists(up))
			GetFullPathNameW(up, MAX_PATH, botdata, NULL);
		default_env(L"DK_BOTLIB_BASEDIR", botdata);
		default_env(L"DK_BOTLIB_GAMEDIR", L"daikatana");
		default_env(L"DK_BOT_CHARFILE", L"bots/dkbot_c.c");
		default_env(L"DK_BOT_CHARNAME", L"dkbot");
		check_botdata();
	}

	tail = arg_tail(GetCommandLineW());
	_snwprintf(cmdline, 8192, L"\"%ls\" %ls", game, tail);

	ZeroMemory(&si, sizeof si);
	si.cb = sizeof si;
	ZeroMemory(&pi, sizeof pi);
	if (!CreateProcessW(game, cmdline, NULL, NULL, FALSE, CREATE_SUSPENDED,
	                    NULL, gamedir, &si, &pi))
		diew(L"CreateProcess");

	g_child = pi.hProcess;
	wprintf(L"[launcher] started %ls (pid %lu, suspended)\n", game,
	        (unsigned long)pi.dwProcessId);
	fflush(stdout);

	/* The hook must precede DLL_LoadDLLs; LoadLibraryW has the same address in the child. */
	bytes = (wcslen(mod) + 1) * sizeof(wchar_t);
	remote = VirtualAllocEx(pi.hProcess, NULL, bytes, MEM_COMMIT | MEM_RESERVE,
	                        PAGE_READWRITE);
	if (!remote)
		diew(L"VirtualAllocEx");
	if (!WriteProcessMemory(pi.hProcess, remote, mod, bytes, NULL))
		diew(L"WriteProcessMemory");

	k32 = GetModuleHandleW(L"kernel32.dll");
	loadlib = (LPTHREAD_START_ROUTINE)(void *)GetProcAddress(k32, "LoadLibraryW");
	if (!loadlib)
		diew(L"GetProcAddress(LoadLibraryW)");
	thread = CreateRemoteThread(pi.hProcess, NULL, 0, loadlib, remote, 0, NULL);
	if (!thread)
		diew(L"CreateRemoteThread");
	WaitForSingleObject(thread, INFINITE);
	GetExitCodeThread(thread, &loaded);   /* low half of the HMODULE; 0 = fail */
	CloseHandle(thread);
	VirtualFreeEx(pi.hProcess, remote, 0, MEM_RELEASE);

	if (!loaded)
		fail(L"dkbot.dll did not load into the game; security software may have blocked it.");
	wprintf(L"[launcher] injected %ls; resuming.\n", mod);
	fflush(stdout);

	ResumeThread(pi.hThread);
	CloseHandle(pi.hThread);

	WaitForSingleObject(pi.hProcess, INFINITE);
	CloseHandle(pi.hProcess);
	return 0;
}
