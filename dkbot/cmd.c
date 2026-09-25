#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dkbot.h"

#define DK_MAX_CMD_ARGS 8
#define DK_MAX_COMMANDS 256

static char args_buf[256];
static char argv_buf[DK_MAX_CMD_ARGS][64];
static int  argc_now;
static int  active;

static dk_fn_argc orig_gi_argc, orig_ss_argc;
static dk_fn_argv orig_gi_argv, orig_ss_argv;
static dk_fn_args orig_gi_args;

static int bot_argc(void)
{
	return active ? argc_now : orig_gi_argc ? orig_gi_argc() : 0;
}

static const char *bot_argv(int n)
{
	if (active)
		return (n >= 0 && n < argc_now) ? argv_buf[n] : "";
	return orig_gi_argv ? orig_gi_argv(n) : "";
}

static const char *bot_args(void)
{
	return active ? args_buf : orig_gi_args ? orig_gi_args() : "";
}

static int ss_argc(void)
{
	return active ? argc_now : orig_ss_argc ? orig_ss_argc() : 0;
}

static const char *ss_argv(int n)
{
	if (active)
		return (n >= 0 && n < argc_now) ? argv_buf[n] : "";
	return orig_ss_argv ? orig_ss_argv(n) : "";
}

int dkbot_redirect_args(void)
{
	void *gi = dk_glob.gi;
	void *ss = dk_glob.ss;

	if (orig_gi_argv)
		return 0;
	if (!gi || !ss)
		return 1;
	orig_gi_argc = DK_AT(gi, DK_GI_OFF_GETARGC, dk_fn_argc);
	orig_gi_argv = DK_AT(gi, DK_GI_OFF_GETARGV, dk_fn_argv);
	orig_gi_args = DK_AT(gi, DK_GI_OFF_GETARGS, dk_fn_args);
	orig_ss_argc = DK_AT(ss, DK_SS_OFF_GETARGC, dk_fn_argc);
	orig_ss_argv = DK_AT(ss, DK_SS_OFF_GETARGV, dk_fn_argv);
	if (!orig_gi_argv || !orig_ss_argv) {
		dk_log("argument accessors look wrong (gi=%p ss=%p)\n",
		       (void *)orig_gi_argv, (void *)orig_ss_argv);
		orig_gi_argv = NULL;
		return 1;
	}
	DK_AT(gi, DK_GI_OFF_GETARGC, dk_fn_argc) = bot_argc;
	DK_AT(gi, DK_GI_OFF_GETARGV, dk_fn_argv) = bot_argv;
	DK_AT(gi, DK_GI_OFF_GETARGS, dk_fn_args) = bot_args;
	DK_AT(ss, DK_SS_OFF_GETARGC, dk_fn_argc) = ss_argc;
	DK_AT(ss, DK_SS_OFF_GETARGV, dk_fn_argv) = ss_argv;
	return 0;
}

static struct {
	char name[32];
	dk_fn_cmdhandler fn;
} cmds[DK_MAX_COMMANDS];
static int ncmds;
static dk_fn_addcommand orig_addcommand, orig_ss_addcommand;

static dk_fn_cmdhandler find_command(const char *name)
{
	int i;

	for (i = 0; i < ncmds; i++)
		if (!strcasecmp(cmds[i].name, name))
			return cmds[i].fn;
	return NULL;
}

static void remember_cmd(const char *name, dk_fn_cmdhandler fn)
{
	if (!name || !fn || ncmds >= DK_MAX_COMMANDS || find_command(name))
		return;
	snprintf(cmds[ncmds].name, sizeof cmds[ncmds].name, "%s", name);
	cmds[ncmds++].fn = fn;
}

static void gi_addcommand(const char *name, dk_fn_cmdhandler fn)
{
	orig_addcommand(name, fn);
	remember_cmd(name, fn);
}

static void ss_addcommand(const char *name, dk_fn_cmdhandler fn)
{
	orig_ss_addcommand(name, fn);
	remember_cmd(name, fn);
}

void dkbot_record_commands(void)
{
	void *gi = dk_glob.gi;
	void *ss = dk_glob.ss;

	if (gi && !orig_addcommand) {
		orig_addcommand = DK_AT(gi, DK_GI_OFF_ADDCOMMAND, dk_fn_addcommand);
		if (orig_addcommand)
			DK_AT(gi, DK_GI_OFF_ADDCOMMAND, dk_fn_addcommand) = gi_addcommand;
	}
	if (ss && !orig_ss_addcommand) {
		orig_ss_addcommand = DK_AT(ss, DK_SS_OFF_ADDCOMMAND, dk_fn_addcommand);
		if (orig_ss_addcommand)
			DK_AT(ss, DK_SS_OFF_ADDCOMMAND, dk_fn_addcommand) = ss_addcommand;
	}
}

int dkbot_client_command_argv(edict_t *ent, const char **argv, int argc)
{
	dk_fn_cmdhandler handler;
	int i;

	if (!ent || !argv || argc < 1 || !argv[0] || active
	    || dkbot_redirect_args() != 0)
		return 1;
	handler = find_command(argv[0]);
	if (!handler)
		return 1;
	for (argc_now = 0; argc_now < argc && argc_now < DK_MAX_CMD_ARGS; argc_now++)
		snprintf(argv_buf[argc_now], sizeof argv_buf[0], "%s",
		         argv[argc_now] ? argv[argc_now] : "");
	args_buf[0] = 0;
	for (i = 1; i < argc_now; i++) {
		if (i > 1)
			strncat(args_buf, " ", sizeof args_buf - strlen(args_buf) - 1);
		strncat(args_buf, argv_buf[i], sizeof args_buf - strlen(args_buf) - 1);
	}
	active = 1;
	handler(ent);
	active = 0;
	return 0;
}

/* Console and rcon only: no client command reaches Cmd_AddCommand's table. */
static void bot_console_cmd(void)
{
	const char *verb = orig_gi_argv ? orig_gi_argv(1) : "";
	const char *arg = orig_gi_argv ? orig_gi_argv(2) : "";

	if (!strcasecmp(verb, "add")) {
		int n = (*arg >= '0' && *arg <= '9') ? atoi(arg) : 1;

		if (dkbot_botlib_problem()) {
			dk_con_printf("bot: cannot add bots: %s\n", dkbot_botlib_problem());
			return;
		}
		dkbot_request(n);
		dk_con_printf("bot: %d queued (one per second)\n", n);
	} else if (!strcasecmp(verb, "remove")) {
		const char *gone;
		int n = 0;

		if (!strcasecmp(arg, "all")) {
			while (dkbot_remove(NULL))
				n++;
			dk_con_printf("bot: removed %d\n", n);
		} else if ((gone = dkbot_remove(arg)) != NULL) {
			dk_con_printf("bot: removed %s\n", gone);
		} else {
			dk_con_printf("bot: none to remove\n");
		}
	} else if (!strcasecmp(verb, "list")) {
		int i, n = dkbot_count();

		dk_con_printf("bot: %d active\n", n);
		for (i = 0; i < n; i++)
			dk_con_printf("  %-12s edict %d\n", dkbot_bot_name(i),
			              dkbot_bot_slot(i));
	} else {
		dk_con_printf("usage: bot add [n] | bot remove [name|all] | bot list\n");
	}
}

void dkbot_register_console(void)
{
	void (*add_command)(const char *, void (*)(void)) = dk_sym(DK_SYM_CMD_ADDCOMMAND);

	if (add_command)
		add_command("bot", bot_console_cmd);
	else
		dk_log("no %s -- \"bot\" console command unavailable\n",
		       DK_SYM_CMD_ADDCOMMAND);
}

const char *dkbot_current_weapon(const edict_t *ent)
{
	const void *w = DK_AT((const unsigned char *)ent, DK_UENT_OFF_CURWEAPON,
	                      const void *);

	return w ? DK_AT(w, DK_INVDEF_OFF_NAME, const char *) : NULL;
}

int dkbot_select_weapon(edict_t *ent, const char *classname)
{
	void *ss = dk_glob.ss;
	unsigned char *e = (unsigned char *)ent;
	void *list = DK_AT(e, DK_UENT_OFF_INVENTORY, void *);
	static dk_fn_weaponselect weapon_select;
	dk_fn_invfind find_item;
	const unsigned char *hook = dkbot_player_hook(ent);
	const char *cur = dkbot_current_weapon(ent);
	const void *winfo;
	void *item, *com;
	dk_fn_findweapon find;
	dk_fn_select select;

	if (!weapon_select)
		weapon_select = dk_sym(DK_SYM_WEAPON_SELECT);
	if (!ss || !list || !classname || !*classname || !weapon_select
	    || !DK_AT(e, DK_UENT_OFF_CURWEAPON, void *))  /* mid-switch */
		return DK_SELECT_RETRY;
	/* Switching away while the sword's frame function is pending corrupts the heap. */
	if (hook && DK_AT(hook, DK_HOOK_OFF_FX_FRAME_FUNC, void *) && cur
	    && !strcmp(cur, "weapon_daikatana"))
		return DK_SELECT_RETRY;
	find_item = DK_AT(ss, DK_SS_OFF_INVFINDITEM, dk_fn_invfind);
	if (!find_item)
		return DK_SELECT_RETRY;
	item = find_item(list, classname);
	if (!item)
		return DK_SELECT_NOT_CARRIED;
	winfo = DK_AT(item, DK_INVDEF_OFF_WINFO, const void *);
	if (!winfo)
		return DK_SELECT_RETRY;
	/* The Slugger's item holds its cordite weaponInfo_s, whose select_func restores the shells. */
	com = dk_glob.com ? *dk_glob.com : NULL;
	find = com ? DK_AT(com, DK_COM_OFF_FIND_REGISTERED_WEAPON, dk_fn_findweapon) : NULL;
	select = DK_AT(winfo, DK_WINFO_OFF_SELECT_FUNC, dk_fn_select);
	if (find && select && find(classname) != winfo)
		select(ent);
	else
		weapon_select(ent, winfo);
	return DK_SELECT_OK;
}
