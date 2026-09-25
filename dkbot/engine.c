#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "dkbot.h"
#include "dk_platform.h"

struct dk_originals dk_orig;
struct dk_globals   dk_glob;

static int verbose;

void dk_log(const char *fmt, ...)
{
	va_list ap;

	if (!verbose)
		return;
	va_start(ap, fmt);
	fputs("[dkbot] ", stderr);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

void *dk_sym(const char *name)
{
	return dk_plat_resolve(name);
}

int dk_engine_init(void)
{
	static int initialised;
	static const char *mangled[] = { DK_SYM_GET_PLAYER_HOOK,
	                                 DK_SYM_WEAPON_SELECT, DK_SYM_CVAR_STRING,
	                                 DK_SYM_CMD_ADDCOMMAND, NULL };
	int i;

	if (initialised)
		return dk_orig.dll_Entry ? 0 : 1;
	initialised = 1;
	dk_plat_init();
	verbose = getenv("DK_BOT_VERBOSE") != NULL;

	dk_orig.dll_Entry                  = dk_sym("dll_Entry");
	dk_orig.dll_ClientConnect          = dk_sym("dll_ClientConnect");
	dk_orig.dll_ClientBeginServerFrame = dk_sym("dll_ClientBeginServerFrame");
	dk_orig.Client_Think = dk_sym(DK_SYM_CLIENT_THINK);
	dk_orig.P_RunFrame   = dk_sym(DK_SYM_RUN_FRAME);
	dk_glob.gi          = dk_sym("gi");
	dk_glob.ge          = dk_sym("globals");
	dk_glob.maxclients  = dk_sym("maxclients");
	dk_glob.sv          = dk_sym("sv");
	dk_glob.svs         = dk_sym("svs");
	dk_glob.com         = dk_sym("com");
	for (i = 0; mangled[i]; i++)
		if (!dk_sym(mangled[i]))
			dk_log("WARNING: cannot resolve %s -- check the mangled name "
			       "against nm -D\n", mangled[i]);
	if (!dk_orig.dll_Entry) {
		fputs("[dkbot] FATAL: cannot resolve the engine's dll_Entry -- "
		      "wrong binary, or symbols were stripped\n", stderr);
		return 1;
	}
	dk_log("bound: dll_Entry=%p gi=%p globals=%p\n", (void *)dk_orig.dll_Entry,
	       dk_glob.gi, (void *)dk_glob.ge);
	return 0;
}

void dk_con_printf(const char *fmt, ...)
{
	dk_fn_conprintf cp = dk_glob.gi
	                     ? DK_AT(dk_glob.gi, DK_GI_OFF_CON_PRINTF, dk_fn_conprintf)
	                     : NULL;
	char buf[1024];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(buf, sizeof buf, fmt, ap);
	va_end(ap);
	if (cp)
		cp("%s", buf);
	else
		fputs(buf, stderr);
}
