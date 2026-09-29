#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dkbot.h"

#define DK_BOT_PREFIX "[Bot]"
/* One bot per second: bots spawned in one frame all take one spawn point. */
#define DK_SPAWN_STAGGER 10
/* A weapon switch needs a moment; retrying every frame just fights it. */
#define DK_RESELECT_EVERY 5

struct bot {
	edict_t *ent;
	int      slot;             /* edict number: botlib's client + 1 */
	char     name[32];
	int      botlib_ok;
	int      firing;
	int      was_dead;
	unsigned long deaths;      /* the engine never counts them */
	char     want_weapon[32];
	float    delta_seen[3];    /* delta_angles the library has been told of */
	float    status_pos[3];
};

static struct bot bots[DK_MAX_BOTS];
static int nbots;
static int pending;
static int carried = -1;     /* bots at the last level exit, -1 before one */
static int spawn_at_frame;
static int map_pending;      /* gstate->mapName is only valid once frames run */
static int adding_bot;

static int frame_now(void)
{
	return (int)dkbot_frame_count();
}

static const float *bot_origin(const struct bot *b)
{
	return (const float *)((const unsigned char *)b->ent + DK_EDICT_OFF_ORIGIN);
}

static float bot_health(const struct bot *b)
{
	return DK_AT((const unsigned char *)b->ent, DK_UENT_OFF_HEALTH, float);
}

void dkbot_request(int n)
{
	if (n <= 0)
		return;
	pending += n;
	if (spawn_at_frame < frame_now())
		spawn_at_frame = frame_now();
}

int dkbot_count(void)
{
	return nbots;
}

int dkbot_bot_slot(int i)
{
	return (i >= 0 && i < nbots) ? bots[i].slot : -1;
}

const edict_t *dkbot_bot_edict(int i)
{
	return (i >= 0 && i < nbots) ? bots[i].ent : NULL;
}

const char *dkbot_bot_name(int i)
{
	return (i >= 0 && i < nbots) ? bots[i].name : "";
}

int dkbot_adding_bot(void)
{
	return adding_bot;
}

static struct bot *bot_of(const edict_t *ent)
{
	int i;

	for (i = 0; i < nbots; i++)
		if (bots[i].ent == ent)
			return &bots[i];
	return NULL;
}

int dkbot_is_bot(const edict_t *ent)
{
	return bot_of(ent) != NULL;
}

edict_t *dkbot_client_edict(int client)
{
	int i;

	for (i = 0; i < nbots; i++)
		if (bots[i].slot == client + 1)
			return bots[i].ent;
	return NULL;
}

int dkbot_slot_is_firing(int entnum)
{
	int i;

	for (i = 0; i < nbots; i++)
		if (bots[i].slot == entnum)
			return bots[i].firing;
	return 0;
}

/* botlib asks for a weapon only once, so the request is kept and re-applied. */
void dkbot_note_weapon_request(const edict_t *ent, const char *classname)
{
	struct bot *b = bot_of(ent);

	if (b && classname && !strncmp(classname, "weapon_", 7))
		snprintf(b->want_weapon, sizeof b->want_weapon, "%s", classname);
}

static unsigned char *edict_at(int i)
{
	dk_game_export_t *ge = dk_glob.ge;

	if (!ge || !ge->edicts || ge->edict_size <= 0 || i < 0 || i >= ge->max_edicts)
		return NULL;
	return (unsigned char *)ge->edicts + (size_t)i * (size_t)ge->edict_size;
}

static int max_clients(void)
{
	return (dk_glob.maxclients && *dk_glob.maxclients)
	       ? (*dk_glob.maxclients)->intValue : 0;
}

static const struct {
	const char *name;
	int character;
	const char *model;
	const char *skin;
	int color;
} roster[] = {
	{ "Kage",      0, "models/global/m_hiro.dkm",     "skins/hiro_bod_1.wal", 0 },
	{ "Nharre",    2, "models/global/m_superfly.dkm", "skins/sfly_bod_4.wal", 5 },
	{ "Medusa",    1, "models/global/m_mikiko.dkm",   "skins/miko_bod_4.wal", 3 },
	{ "Garroth",   0, "models/global/m_hiro.dkm",     "skins/hiro_bod_6.wal", 6 },
	{ "Stavros",   2, "models/global/m_superfly.dkm", "skins/sfly_bod_1.wal", 2 },
	{ "Wyndrax",   1, "models/global/m_mikiko.dkm",   "skins/miko_bod_1.wal", 4 },
	{ "Lycanthir", 0, "models/global/m_hiro.dkm",     "skins/hiro_bod_4.wal", 1 },
	{ "Cerberus",  2, "models/global/m_superfly.dkm", "skins/sfly_bod_6.wal", 7 },
};
#define DK_ROSTER ((int)(sizeof roster / sizeof roster[0]))

static int roster_pick(void)
{
	char name[32];
	int k, i;

	for (k = 0; k < DK_ROSTER; k++) {
		snprintf(name, sizeof name, "%s%s", DK_BOT_PREFIX, roster[k].name);
		for (i = 0; i < nbots && strcmp(bots[i].name, name); i++)
			;
		if (i == nbots)
			return k;
	}
	return nbots % DK_ROSTER;
}

/* Top down: the engine hands real clients the lowest free slots. */
static int find_free_slot(void)
{
	int i;

	for (i = max_clients(); i >= 1; i--) {
		unsigned char *e = edict_at(i);

		if (e && !DK_AT(e, DK_EDICT_OFF_INUSE, qboolean))
			return i;
	}
	return -1;
}

/* ge->ClientDisconnect is our dll_ClientDisconnect: our own calls are not the engine's drops. */
static int disconnecting;

static void disconnect(edict_t *ent)
{
	dk_game_export_t *ge = dk_glob.ge;

	if (!ge || !ge->ClientDisconnect || !ent)
		return;
	disconnecting = 1;
	ge->ClientDisconnect(ent);
	disconnecting = 0;
}

static void forget(int idx, const char *why)
{
	dk_log("dropping %s (slot %d): %s\n", bots[idx].name, bots[idx].slot, why);
	if (bots[idx].botlib_ok && dkbot_botlib_active())
		dkbot_botlib_shutdown_client(bots[idx].slot - 1);
	memmove(&bots[idx], &bots[idx + 1], sizeof bots[0] * (size_t)(nbots - idx - 1));
	nbots--;
}

static void drop_bot(int idx, const char *why)
{
	if (idx < 0 || idx >= nbots)
		return;
	disconnect(bots[idx].ent);
	dkbot_release_client_slot(bots[idx].ent);
	forget(idx, why);
}

/* A kick drops the bot through the engine, which then frees its slot itself. */
void dkbot_forget_edict(const edict_t *ent)
{
	struct bot *b = disconnecting ? NULL : bot_of(ent);

	if (b)
		forget((int)(b - bots), "the engine disconnected it");
}

/* By name, else the newest; returns the removed name (static) or NULL. */
const char *dkbot_remove(const char *name)
{
	static char removed[32];
	int i = -1, j;

	if (name && *name && strcasecmp(name, "all")) {
		for (j = 0; j < nbots; j++)
			if (!strcasecmp(bots[j].name, name))
				i = j;
	} else if (nbots > 0) {
		i = nbots - 1;
	}
	if (i < 0)
		return NULL;
	snprintf(removed, sizeof removed, "%s", bots[i].name);
	drop_bot(i, "removed from the console");
	return removed;
}

int dkbot_release_edict(const edict_t *ent)
{
	struct bot *b = bot_of(ent);

	if (!b)
		return 0;
	drop_bot((int)(b - bots), "a real client is taking that edict");
	return 1;
}

static int dkbot_add(int pick)
{
	const char *name = roster[pick].name;
	dk_game_export_t *ge = dk_glob.ge;
	char userinfo[512];
	struct bot *b;
	unsigned char *e;
	int slot;

	if (!ge || !ge->ClientConnect || !ge->ClientBegin || nbots >= DK_MAX_BOTS)
		return 1;
	slot = find_free_slot();
	if (slot < 0) {
		dk_log("bot add: no free client slot (maxclients=%d)\n", max_clients());
		return 1;
	}
	e = edict_at(slot);
	b = &bots[nbots];
	memset(b, 0, sizeof *b);
	b->ent = (edict_t *)e;
	b->slot = slot;
	snprintf(b->name, sizeof b->name, "%s%s", DK_BOT_PREFIX, name);
	/* Daikatana's keys: a Quake II `skin` in place of modelname/skinname crashes clients. */
	snprintf(userinfo, sizeof userinfo,
	         "\\name\\%s\\character\\%d\\modelname\\%s\\skinname\\%s"
	         "\\skincolor\\%d\\team\\0\\spectator\\0"
	         "\\rate\\25000\\msg\\1\\fov\\90",
	         b->name, roster[pick].character, roster[pick].model,
	         roster[pick].skin, roster[pick].color);
	/* Claim and arm the slot first: ClientConnect already sends to it. */
	nbots++;
	dkbot_arm_client_buffers();
	adding_bot = 1;
	if (!ge->ClientConnect((edict_t *)e, userinfo, 0)) {
		adding_bot = 0;
		nbots--;
		dk_log("bot add: ClientConnect refused %s\n", b->name);
		return 1;
	}
	adding_bot = 0;
	if (ge->ClientUserinfoChanged)
		ge->ClientUserinfoChanged((edict_t *)e, userinfo);
	ge->ClientBegin((edict_t *)e, 0);
	dk_log("bot add: %s on edict %d\n", b->name, slot);
	if (dkbot_botlib_active()) {
		b->botlib_ok = dkbot_botlib_setup_client(slot - 1) == 0;
		if (!b->botlib_ok) {
			dk_con_printf("bot: %s could not load its character (%s) -- check "
			              "DK_BOTLIB_BASEDIR and DK_BOT_CHARFILE\n", b->name,
			              getenv("DK_BOT_CHARFILE") ? getenv("DK_BOT_CHARFILE")
			                                        : "bots/dkbot_c.c");
			drop_bot(nbots - 1, "BotSetupClient failed");
			return 1;
		}
	}
	{
		const char *give = getenv("DK_BOT_GIVE");

		if (give && *give) {
			const char *argv[1] = { give };

			dkbot_client_command_argv((edict_t *)e, argv, 1);
		}
	}
	return 0;
}

/* From bl_main.c field for field; angles are relative to delta_angles. */
static void make_cmd(struct bot *b, const dk_bot_input_t *in,
                     const float *delta, usercmd_t *cmd)
{
	float angles[3], forward[3], right[3];
	int swimming = DK_AT((const unsigned char *)b->ent, DK_UENT_OFF_WATERLEVEL, int) >= 2;
	int k;

	cmd->msec = (byte)(1000.0f * in->thinktime);
	if (!cmd->msec)
		cmd->msec = 100;
	for (k = 0; k < 3; k++)
		cmd->angles[k] = DK_ANGLE2SHORT(in->viewangles[k] - delta[k]);
	angles[0] = swimming ? in->viewangles[0] : 0.0f;
	angles[1] = in->viewangles[1];
	angles[2] = 0.0f;
	dk_angle_vectors(angles, forward, right, NULL);
	cmd->forwardmove = (short)(dk_dot(forward, in->dir) * in->speed);
	cmd->sidemove = (short)(dk_dot(right, in->dir) * in->speed);
	cmd->upmove = (short)(fabsf(forward[2]) * in->dir[2] * in->speed);
	if (in->actionflags & DK_ACTION_MOVEFORWARD)
		cmd->forwardmove += 400;
	if (in->actionflags & DK_ACTION_MOVEBACK)
		cmd->forwardmove -= 400;
	if (in->actionflags & DK_ACTION_MOVELEFT)
		cmd->sidemove -= 400;
	if (in->actionflags & DK_ACTION_MOVERIGHT)
		cmd->sidemove += 400;
	if (in->actionflags & DK_ACTION_JUMP)
		cmd->upmove += 400;
	if (in->actionflags & DK_ACTION_CROUCH)
		cmd->upmove -= 400;
	cmd->lightlevel = 64;
	b->firing = (in->actionflags & DK_ACTION_ATTACK) != 0;
	if (b->firing)
		cmd->buttons |= DK_BUTTON_ATTACK;
	if (in->actionflags & DK_ACTION_USE)
		cmd->buttons |= DK_BUTTON_USE;
}

static void think(struct bot *b, int i)
{
	dk_game_export_t *ge = dk_glob.ge;
	const dk_bot_input_t *in = NULL;
	usercmd_t cmd;
	float delta[3] = { 0.0f, 0.0f, 0.0f };
	const void *cl = DK_AT((const unsigned char *)b->ent, DK_EDICT_OFF_CLIENT, void *);
	int dead = DK_AT((const unsigned char *)b->ent, DK_UENT_OFF_DEADFLAG, int)
	           || bot_health(b) <= 0.0f;
	int k, respawn = 0, delayed_jump = 0;

	if (dead && !b->was_dead)
		b->deaths++;
	b->was_dead = dead;
	if (b->botlib_ok) {
		dk_bot_updateclient_t buc;

		dkbot_fill_client(b->ent, &buc);
		/* The library adds delta_angles to its view: pass each change once, as bl_main.c does. */
		for (k = 0; k < 3; k++) {
			float seen = buc.delta_angles[k];

			buc.delta_angles[k] = seen - b->delta_seen[k];
			b->delta_seen[k] = seen;
		}
		dkbot_botlib_update_client(b->slot - 1, &buc);
		dkbot_botlib_ai(b->slot - 1, 0.1f);
		in = dkbot_botlib_input(b->slot - 1);
		if (!dead && b->want_weapon[0] && frame_now() % DK_RESELECT_EVERY == 0) {
			const char *now = dkbot_current_weapon(b->ent);

			if ((!now || strcmp(now, b->want_weapon))
			    && dkbot_select_weapon(b->ent, b->want_weapon)
			       == DK_SELECT_NOT_CARRIED)
				b->want_weapon[0] = 0;
		}
	}
	if (cl) {
		const short *da = (const short *)((const unsigned char *)cl
		                  + DK_CLIENT_OFF_PS + DK_PS_OFF_PMOVE
		                  + DK_PM_OFF_DELTA_ANGLES);

		for (k = 0; k < 3; k++)
			delta[k] = DK_SHORT2ANGLE(da[k]);
	}
	memset(&cmd, 0, sizeof cmd);
	cmd.msec = 100;                  /* the server runs at 10 Hz */
	if (in) {
		make_cmd(b, in, delta, &cmd);
		delayed_jump = (in->actionflags & DK_ACTION_DELAYEDJUMP) != 0;
		respawn = (in->actionflags & DK_ACTION_RESPAWN) != 0;
		dkbot_set_ping(i, (int)(1000.0f * in->thinktime) + (rand() % 41) - 20);
	}
	/* Respawn takes a fresh press; a held button never revives. */
	if (dead || respawn) {
		if (frame_now() & 1)
			cmd.buttons |= DK_BUTTON_ATTACK;
		else
			cmd.buttons &= (byte)~DK_BUTTON_ATTACK;
	}
	/* The engine runs this for real clients only; respawn lives in it. */
	if (dk_orig.dll_ClientBeginServerFrame)
		dk_orig.dll_ClientBeginServerFrame((userEntity_t *)b->ent);
	/* Twice at half msec: 10 Hz pmove fails steps, stairs and water jumps. */
	cmd.msec = (byte)(cmd.msec / 2);
	if (!cmd.msec)
		cmd.msec = 1;
	ge->ClientThink(b->ent, &cmd);
	if (delayed_jump)
		cmd.upmove += 400;
	ge->ClientThink(b->ent, &cmd);
}

void dkbot_think_all(void)
{
	dk_game_export_t *ge = dk_glob.ge;
	int i;

	dkbot_arm_client_buffers();
	if (pending && !dkbot_botlib_problem() && ge && ge->ClientConnect
	    && frame_now() >= spawn_at_frame) {
		if (dkbot_add(roster_pick()) == 0)
			pending--;
		else
			pending = 0;
		spawn_at_frame = frame_now() + DK_SPAWN_STAGGER;
	}
	if (!ge || !ge->ClientThink)
		return;
	if (dkbot_botlib_active()) {
		if (map_pending && dkbot_map_name() && *dkbot_map_name()) {
			map_pending = 0;
			dkbot_botlib_load_map(dkbot_map_name());
		}
		dkbot_botlib_start_frame(dkbot_server_time());
		dkbot_push_entities();
	}
	for (i = 0; i < nbots; i++)
		if (bots[i].ent)
			think(&bots[i], i);
}

void dkbot_status(char *buf, unsigned long n)
{
	unsigned long used = 0;
	int i;

	buf[0] = 0;
	for (i = 0; i < nbots && used + 200 < n; i++) {
		struct bot *b = &bots[i];
		const float *o = bot_origin(b);
		const char *w = dkbot_current_weapon(b->ent);
		float moved = (float)sqrt((o[0] - b->status_pos[0]) * (o[0] - b->status_pos[0])
		                        + (o[1] - b->status_pos[1]) * (o[1] - b->status_pos[1])
		                        + (o[2] - b->status_pos[2]) * (o[2] - b->status_pos[2]));
		int area = dkbot_aas_area(o);

		used += (unsigned long)snprintf(buf + used, n - used,
		        "%s%s@(%.0f %.0f %.0f) %d/%lu hp=%.0f mv=%.0f area=%d/%d "
		        "w=%s", i ? ", " : "", b->name, o[0], o[1], o[2],
		        DK_AT((const unsigned char *)b->ent + DK_UENT_OFF_RECORD,
		              DK_REC_OFF_FRAGS, int),
		        b->deaths, bot_health(b), moved, area,
		        dkbot_aas_area_reachable(area), w ? w : "-");
		memcpy(b->status_pos, o, sizeof b->status_pos);
	}
}

void dkbot_server_init(void)
{
	dkbot_install_hooks();
	dkbot_record_commands();
}

void dkbot_level_load(void)
{
	const char *want = getenv("DK_BOT_SPAWN");
	const char *bl = getenv("DK_BOTLIB");

	dkbot_install_hooks();
	nbots = 0;
	pending = carried >= 0 ? carried : want ? atoi(want) : 0;
	if (!bl || (strcmp(bl, "none") && strcmp(bl, "0") && *bl))
		dkbot_botlib_init();
	dkbot_dump_gamemode();
	dkbot_register_console();
	map_pending = 1;
	spawn_at_frame = frame_now() + 20;          /* let the level settle */
}

void dkbot_level_exit(void)
{
	int i;

	/* Forgotten only at the next level load: the game still messages them as it shuts down. */
	for (i = 0; i < nbots; i++) {
		if (!bots[i].ent)
			continue;
		disconnect(bots[i].ent);
		dkbot_release_client_slot(bots[i].ent);
	}
	carried = nbots + pending;
	pending = 0;
	dkbot_botlib_shutdown();
}
