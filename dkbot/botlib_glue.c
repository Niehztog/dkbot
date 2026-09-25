#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dkbot.h"
#include "dk_platform.h"
#include "dk/dk_inventory.h"

static void *lib;
static dk_bot_export_t *be;
static dk_bot_import_t  bi;
static dk_bot_input_t   inputs[DK_MAX_CLIENTS];
static int              have_input[DK_MAX_CLIENTS];

static int  (*aas_point_area)(const float *);
static int  (*aas_area_reachability)(int);

static char problem[512];

static void set_problem(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(problem, sizeof problem, fmt, ap);
	va_end(ap);
	dk_con_printf("dkbot: bots unavailable: %s\n", problem);
}

const char *dkbot_botlib_problem(void)
{
	return problem[0] ? problem : NULL;
}

int dkbot_botlib_active(void)
{
	return be != NULL;
}

const dk_bot_input_t *dkbot_botlib_input(int client)
{
	if (client < 0 || client >= DK_MAX_CLIENTS || !have_input[client])
		return NULL;
	return &inputs[client];
}

int dkbot_aas_area(const float *origin)
{
	return aas_point_area ? aas_point_area(origin) : -1;
}

int dkbot_aas_area_reachable(int area)
{
	return aas_area_reachability && area > 0 ? aas_area_reachability(area) : -1;
}

float dkbot_server_time(void)
{
	return dk_glob.ss ? DK_AT(dk_glob.ss, DK_SS_OFF_TIME, float) : 0.0f;
}

const char *dkbot_map_name(void)
{
	return dk_glob.ss ? DK_AT(dk_glob.ss, DK_SS_OFF_MAPNAME, const char *) : NULL;
}

/* The engine's own table: world and inline models included, rebuilt per map. */
static const char *model_name(int index)
{
	return dk_glob.sv && index >= 0 && index < DK_MAX_MODELS
	       ? (const char *)dk_glob.sv + DK_SV_OFF_CONFIGSTRINGS
	         + (size_t)(DK_CS_MODELS + index) * DK_CONFIGSTRING_SIZE
	       : "";
}

static void bl_BotInput(int client, dk_bot_input_t *in)
{
	if (client < 0 || client >= DK_MAX_CLIENTS || !in)
		return;
	inputs[client] = *in;
	have_input[client] = 1;
}

/* A NULL-terminated argument list, not a format string. */
static void bl_BotClientCommand(int client, char *str, ...)
{
	edict_t *ent = dkbot_client_edict(client);
	const char *argv[8];
	int argc = 0;
	va_list ap;

	argv[argc++] = str ? str : "";
	va_start(ap, str);
	while (argc < (int)(sizeof argv / sizeof argv[0])) {
		const char *a = va_arg(ap, char *);

		if (!a)
			break;
		argv[argc++] = a;
	}
	va_end(ap);
	if (!ent)
		return;
	/* Daikatana has no generic "use": drive its weapon selector. */
	if (!strcasecmp(argv[0], "use") && argc >= 2) {
		dkbot_note_weapon_request(ent, argv[1]);
		dkbot_select_weapon(ent, argv[1]);
		return;
	}
	dkbot_client_command_argv(ent, argv, argc);
}

static void bl_Print(int type, char *fmt, ...)
{
	static int quiet = -1;
	char buf[1024];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(buf, sizeof buf, fmt, ap);
	va_end(ap);
	/* the reachability pass prints a percentage every few areas */
	if (quiet < 0) {
		const char *v = getenv("DK_BOT_VERBOSE");

		quiet = !(v && atoi(v) >= 2);
	}
	if (quiet && strchr(buf, '%') && type == DK_PRT_MESSAGE)
		return;
	dk_log("botlib[%d]: %s", type, buf);
}

#define DK_CONTENTS_SOLID      0x00001
#define DK_CONTENTS_PLAYERCLIP 0x10000
#define DK_CONTENTS_SEETHROUGH (0x80 | 0x200)
static const float bl_vec3_origin[3] = { 0.0f, 0.0f, 0.0f };
static dk_fn_tracebox      tracebox;
static dk_fn_pointcontents pointcontents;

/* passent 0 must be NULL: skipping edicts[0], the world, passes every wall. */
static void *bl_edict_of(int entnum)
{
	dk_game_export_t *ge = dk_glob.ge;

	if (!ge || !ge->edicts || entnum <= 0 || entnum >= ge->max_edicts)
		return NULL;
	return (unsigned char *)ge->edicts + (size_t)entnum * (size_t)ge->edict_size;
}

static dk_bsp_trace_t bl_Trace(dk_vec3_t start, dk_vec3_t mins, dk_vec3_t maxs,
                               dk_vec3_t end, int passent, int contentmask)
{
	dk_bsp_trace_t tr;
	dk_trace_t et;
	int num;

	memset(&tr, 0, sizeof tr);
	tr.fraction = 1.0f;
	if (!end)
		return tr;
	tr.endpos[0] = end[0];
	tr.endpos[1] = end[1];
	tr.endpos[2] = end[2];
	if (!tracebox || !start)
		return tr;
	/* a player's mask: Daikatana's players also stop at see-through solids */
	if ((contentmask & (DK_CONTENTS_SOLID | DK_CONTENTS_PLAYERCLIP))
	    == (DK_CONTENTS_SOLID | DK_CONTENTS_PLAYERCLIP))
		contentmask |= DK_CONTENTS_SEETHROUGH;
	/* mins/maxs are NULL for a line trace, and the engine dereferences them */
	et = tracebox(start, mins ? mins : bl_vec3_origin,
	              maxs ? maxs : bl_vec3_origin, end,
	              bl_edict_of(passent), contentmask);
	tr.allsolid   = et.allsolid;
	tr.startsolid = et.startsolid;
	tr.fraction   = et.fraction;
	memcpy(tr.endpos, et.endpos, sizeof tr.endpos);
	memcpy(tr.plane.normal, et.plane.normal, sizeof tr.plane.normal);
	tr.plane.dist     = et.plane.dist;
	tr.plane.type     = et.plane.type;
	tr.plane.signbits = et.plane.signbits;
	tr.contents = et.contents;
	if (et.surface) {
		memcpy(tr.surface.name, et.surface->name, sizeof tr.surface.name);
		tr.surface.name[sizeof tr.surface.name - 1] = 0;
		tr.surface.flags = et.surface->flags;
		tr.surface.value = et.surface->value;
	}
	/* 0 is the library's "nothing hit"; never -1 */
	num = et.ent ? dkbot_entity_number((const edict_t *)et.ent) : 0;
	tr.ent = num > 0 ? num : 0;
	return tr;
}

static int bl_PointContents(dk_vec3_t point)
{
	return pointcontents && point ? pointcontents(point) : 0;
}

static void *bl_GetMemory(int size)
{
	return calloc(1, (size_t)size);
}

static void bl_FreeMemory(void *ptr)
{
	free(ptr);
}

/* Never 0: AAS_DebugLine takes 0 for "unused" and re-creates it every call. */
static int bl_DebugLineCreate(void)
{
	static int next = 1;

	return next < 256 ? next++ : 255;
}

static void bl_DebugLineDelete(int line) { }
static void bl_DebugLineShow(int line, dk_vec3_t start, dk_vec3_t end, int color) { }

static const char *botlib_path(void)
{
	static char buf[1024];
	const char *p = getenv("DK_BOTLIB"), *mod = getenv("DK_MOD");
	const char *slash;
	size_t n;

	if (p && *p)
		return p;
	if (dk_plat_self_dir(buf, sizeof buf) == 0) {
		n = strlen(buf);
		snprintf(buf + n, sizeof buf - n, "%s%s", DK_PATH_SEP, DK_BOTLIB_NAME);
		return buf;
	}
	if (mod && (slash = strrchr(mod, '/')) != NULL
	    && (size_t)(slash - mod) + sizeof("/" DK_BOTLIB_NAME) <= sizeof buf) {
		snprintf(buf, sizeof buf, "%.*s/%s", (int)(slash - mod), mod,
		         DK_BOTLIB_NAME);
		return buf;
	}
	return DK_BOTLIB_NAME;
}

static void libvar(const char *name, const char *value)
{
	be->BotLibVarSet((char *)name, (char *)value);
}

typedef void (*dk_fn_set_fs)(dk_fn_fs_loadfile, dk_fn_fs_freefile);
static dk_fn_set_fs set_fs;

static const struct { const char *name; int reach; } reaches[] = {
#define X(name, reach) { name, reach },
	DK_WEAPON_REACH(X)
#undef X
};

int dkbot_botlib_init(void)
{
	const char *path = botlib_path();
	const char *basedir = getenv("DK_BOTLIB_BASEDIR");
	const char *gamedir = getenv("DK_BOTLIB_GAMEDIR");
	dk_fn_GetBotAPI get;
	char buf[96], val[32];
	int i, rc;

	if (be)
		return 0;
	problem[0] = 0;
	lib = dk_plat_dlopen(path);
	if (!lib) {
		set_problem("cannot load %s: %s", path, dk_plat_dlerror());
		return 1;
	}
	get = (dk_fn_GetBotAPI)dk_plat_dlsym(lib, "GetBotAPI");
	aas_point_area        = dk_plat_dlsym(lib, "AAS_PointAreaNum");
	aas_area_reachability = dk_plat_dlsym(lib, "AAS_AreaReachability");
	set_fs = (dk_fn_set_fs)dk_plat_dlsym(lib, "DK_SetFileSystem");
	if (dk_glob.gi) {
		tracebox = DK_AT(dk_glob.gi, DK_GI_OFF_TRACEBOX, dk_fn_tracebox);
		pointcontents = DK_AT(dk_glob.gi, DK_GI_OFF_POINTCONTENTS,
		                      dk_fn_pointcontents);
	}
	memset(&bi, 0, sizeof bi);
	bi.BotInput         = bl_BotInput;
	bi.BotClientCommand = bl_BotClientCommand;
	bi.Print            = bl_Print;
	bi.Trace            = bl_Trace;
	bi.PointContents    = bl_PointContents;
	bi.GetMemory        = bl_GetMemory;
	bi.FreeMemory       = bl_FreeMemory;
	bi.DebugLineCreate  = bl_DebugLineCreate;
	bi.DebugLineDelete  = bl_DebugLineDelete;
	bi.DebugLineShow    = bl_DebugLineShow;
	be = get ? get(&bi) : NULL;
	if (!be || !be->BotLibVarSet || !be->BotSetupLibrary) {
		set_problem("%s has no usable GetBotAPI", path);
		be = NULL;
		dk_plat_dlclose(lib);
		lib = NULL;
		return 1;
	}

	if (basedir)
		libvar("basedir", basedir);
	if (gamedir)
		libvar("gamedir", gamedir);
	snprintf(val, sizeof val, "%d", (dk_glob.maxclients && *dk_glob.maxclients)
	                                ? (*dk_glob.maxclients)->intValue : 8);
	libvar("maxclients", val);
	snprintf(val, sizeof val, "%d", dk_glob.ge ? dk_glob.ge->max_edicts : 1024);
	libvar("maxentities", val);
	/* 0x8000: Daikatana's not-in-deathmatch flag; Quake II's is 0x800 */
	libvar("notspawnflags", "32768");
	/* jumpvel: a Hiro's 66-unit rise, sqrt(2 * 800 * 66); maxbarrier: Quake II's 33/45.6 of it */
	libvar("sv_gravity", "800");
	libvar("sv_jumpvel", "325");
	libvar("sv_step", "18");
	libvar("sv_maxbarrier", "48");
	libvar("sv_maxvelocity", "320");
	libvar("sv_maxwalkvelocity", "320");
	libvar("sv_maxswimvelocity", "160");   /* PM_WaterMove halves pm_maxspeed */
	libvar("sv_airaccelerate", "3");
	libvar("sv_friction", "6");
	libvar("sv_stopspeed", "100");
	libvar("sv_waterfriction", "1");
	libvar("sv_watergravity", "400");
	for (i = 0; i < (int)(sizeof reaches / sizeof reaches[0]); i++) {
		if (reaches[i].reach <= 0)
			continue;
		snprintf(buf, sizeof buf, "reach_%s", reaches[i].name);
		snprintf(val, sizeof val, "%d", reaches[i].reach);
		libvar(buf, val);
	}

	dkbot_redirect_args();
	rc = be->BotSetupLibrary();
	dk_log("botlib: loaded %s, BotSetupLibrary -> %d\n", path, rc);
	if (rc != DK_BLERR_NOERROR) {
		set_problem("the bot library found no configs in %s%s%s (error %d) -- "
		            "generate botdata/ (docs/BUILD.md)",
		            basedir ? basedir : "", DK_PATH_SEP, gamedir ? gamedir : "", rc);
		dkbot_botlib_shutdown();
		return 1;
	}
	return 0;
}

void dkbot_botlib_shutdown_client(int client)
{
	if (be && be->BotShutdownClient)
		be->BotShutdownClient(client);
	if (client >= 0 && client < DK_MAX_CLIENTS)
		have_input[client] = 0;
}

void dkbot_botlib_shutdown(void)
{
	if (be && be->BotShutdownLibrary)
		be->BotShutdownLibrary();
	be = NULL;
	memset(have_input, 0, sizeof have_input);
	if (lib) {
		dk_plat_dlclose(lib);
		lib = NULL;
	}
	aas_point_area = NULL;
	aas_area_reachability = NULL;
	set_fs = NULL;
}

void dkbot_dump_gamemode(void)
{
	static const char *names[] = { "deathmatch", "coop", "teamplay", "ctf",
	                               "dmflags", "maxclients", "dm_same_map",
	                               "maps_dm", "sv_auto_rotate_map",
	                               "sv_random_map", "timelimit", "fraglimit",
	                               "dm_spawn_farthest", NULL };
	const char *(*cvar_string)(const char *) = dk_sym(DK_SYM_CVAR_STRING);
	int i;

	for (i = 0; cvar_string && names[i]; i++) {
		const char *v = cvar_string(names[i]);

		dk_log("cvar %-18s = \"%s\"\n", names[i], v ? v : "");
	}
}

int dkbot_botlib_load_map(const char *mapname)
{
	static char *models[DK_MAX_MODELS];
	int nmodels = 0, i, rc;

	if (!be || !be->BotLoadMap)
		return 1;
	/* The library copies the names; it indexes them by modelindex. */
	for (i = 0; i < DK_MAX_MODELS; i++)
		if (*(models[i] = (char *)model_name(i)))
			nmodels = i + 1;
	/* The library reads the map the engine loaded, through the engine's file system. */
	if (set_fs && dk_glob.ss)
		set_fs(DK_AT(dk_glob.ss, DK_SS_OFF_FS_LOADFILE, dk_fn_fs_loadfile),
		       DK_AT(dk_glob.ss, DK_SS_OFF_FS_FREEFILE, dk_fn_fs_freefile));
	rc = be->BotLoadMap((char *)mapname, nmodels, models, 0, NULL, 0, NULL);
	dk_log("botlib: BotLoadMap(\"%s\") -> %d\n", mapname, rc);
	if (rc == DK_BLERR_NOAASFILE)
		set_problem("no navigation data for %s -- botdata/<gamedir>/maps/ "
		            "needs %s.aas (tools/gen-aas.sh %s)", mapname, mapname, mapname);
	else if (rc != DK_BLERR_NOERROR)
		set_problem("the bot library could not load %s (error %d)", mapname, rc);
	return rc;
}

int dkbot_botlib_setup_client(int client)
{
	dk_bot_settings_t settings;
	const char *cfile = getenv("DK_BOT_CHARFILE");
	const char *cname = getenv("DK_BOT_CHARNAME");
	const char *gamedir = getenv("DK_BOTLIB_GAMEDIR");
	int dk = !gamedir || !*gamedir || !strncmp(gamedir, "daikatana", 9);
	int rc;

	if (!be || !be->BotSetupClient)
		return 1;
	memset(&settings, 0, sizeof settings);
	snprintf(settings.characterfile, sizeof settings.characterfile, "%s",
	         cfile && *cfile ? cfile : dk ? "bots/dkbot_c.c" : "bots/hunk_c.c");
	snprintf(settings.charactername, sizeof settings.charactername, "%s",
	         cname && *cname ? cname : dk ? "dkbot" : "hunk");
	rc = be->BotSetupClient(client, &settings);   /* true on success */
	dk_log("botlib: BotSetupClient(%d, \"%s\", \"%s\") -> %d (%s)\n", client,
	       settings.characterfile, settings.charactername, rc,
	       rc ? "ok" : "failed");
	return rc ? 0 : 1;
}

void dkbot_botlib_start_frame(float time)
{
	if (!be || !be->BotStartFrame)
		return;
	memset(have_input, 0, sizeof have_input);
	be->BotStartFrame(time);
}

void dkbot_botlib_update_client(int client, dk_bot_updateclient_t *buc)
{
	if (be && be->BotUpdateClient)
		be->BotUpdateClient(client, buc);
}

void dkbot_botlib_ai(int client, float thinktime)
{
	if (be && be->BotAI)
		be->BotAI(client, thinktime);
}

void dkbot_push_entities(void)
{
	static int dump_items = -1;
	dk_game_export_t *ge = dk_glob.ge;
	int i;

	if (!be || !be->BotUpdateEntity || !ge || !ge->edicts || ge->edict_size <= 0)
		return;
	if (dump_items < 0)
		dump_items = getenv("DK_BOT_DUMP_ITEMS") != NULL;
	for (i = 0; i < ge->num_edicts; i++) {
		unsigned char *e = (unsigned char *)ge->edicts + (size_t)i * ge->edict_size;
		const char *cn = DK_AT(e, DK_EDICT_OFF_CLASSNAME, const char *);
		dk_bot_updateentity_t bue;

		if (!DK_AT(e, DK_EDICT_OFF_INUSE, qboolean))
			continue;
		if (dump_items > 0 && cn && (!strncmp(cn, "item_", 5)
		    || !strncmp(cn, "weapon_", 7) || !strncmp(cn, "ammo_", 5)))
			dk_log("ITEM %-28s modelindex=%-4d %s\n", cn,
			       DK_AT(e, DK_ES_OFF_MODELINDEX, int),
			       model_name(DK_AT(e, DK_ES_OFF_MODELINDEX, int)));
		dkbot_fill_entity((const edict_t *)e, &bue);
		be->BotUpdateEntity(i, &bue);
	}
	if (dump_items > 0)
		dump_items = 0;
}
