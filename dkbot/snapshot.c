#include <math.h>
#include <stddef.h>
#include <string.h>
#include <strings.h>

#include "dkbot.h"
#include "dk/dk_entstate.h"
#include "dk/dk_inventory.h"

static int inventory_index(const char *name)
{
	static const struct { const char *name; int index; } slots[] = {
#define X(name, index) { name, index },
		DK_INVENTORY_ITEMS(X)
#undef X
	};
	unsigned i;

	for (i = 0; name && i < sizeof slots / sizeof slots[0]; i++)
		if (!strcasecmp(slots[i].name, name))
			return slots[i].index;
	return 0;
}

static void vcopy(dk_vec3_t dst, const float *src)
{
	dst[0] = src[0];
	dst[1] = src[1];
	dst[2] = src[2];
}

const unsigned char *dkbot_player_hook(const edict_t *ent)
{
	static edict_t *(*get_hook)(edict_t *);

	if (!get_hook)
		get_hook = dk_sym(DK_SYM_GET_PLAYER_HOOK);
	return get_hook ? (const unsigned char *)get_hook((edict_t *)ent) : NULL;
}

void dkbot_fill_client(const edict_t *ent, dk_bot_updateclient_t *buc)
{
	const unsigned char *e = (const unsigned char *)ent;
	const unsigned char *hook = dkbot_player_hook(ent);
	const unsigned char *ps, *pms;
	void *list, *node, *client;
	int i, guard = 0;

	memset(buc, 0, sizeof *buc);
	vcopy(buc->origin, (const float *)(e + DK_EDICT_OFF_ORIGIN));
	vcopy(buc->velocity, (const float *)(e + DK_UENT_OFF_VELOCITY));
	buc->fov = 90.0f;
	buc->gravity = 800.0f;

	/* stats[1] is Quake II's STAT_HEALTH; left 0, the AI thinks it is dead. */
	buc->stats[1] = (short)DK_AT(e, DK_UENT_OFF_HEALTH, float);
	buc->inventory[DK_INVENTORY_HEALTH] = buc->stats[1];
	buc->stats[DK_STAT_ARMOR] = (short)DK_AT(e, DK_UENT_OFF_ARMOR_VAL, float);

	if (hook) {
		static const struct { int off; int stat; } timed[] = {
			{ DK_HOOK_OFF_INVULN_TIME,  DK_STAT_INVULN_SECS },
			{ DK_HOOK_OFF_ENVIRO_TIME,  DK_STAT_ENVIROSUIT_SECS },
			{ DK_HOOK_OFF_OXYLUNG_TIME, DK_STAT_OXYLUNG_SECS },
		};
		static const struct { int off; const char *name; } levels[] = {
			{ DK_HOOK_OFF_POWER_BOOST,  "item_power_boost" },
			{ DK_HOOK_OFF_ATTACK_BOOST, "item_attack_boost" },
			{ DK_HOOK_OFF_SPEED_BOOST,  "item_speed_boost" },
			{ DK_HOOK_OFF_ACRO_BOOST,   "item_acro_boost" },
			{ DK_HOOK_OFF_VITA_BOOST,   "item_vita_boost" },
		};
		float now = dkbot_server_time();
		unsigned k;

		for (k = 0; k < sizeof timed / sizeof timed[0]; k++) {
			float left = DK_AT(hook, timed[k].off, float) - now;

			buc->stats[timed[k].stat] = left > 0.0f ? (short)left : 0;
		}
		for (k = 0; k < sizeof levels / sizeof levels[0]; k++) {
			int idx = inventory_index(levels[k].name);
			unsigned int lvl = DK_AT(hook, levels[k].off, unsigned int);

			if (idx > 0 && idx < DK_BL_MAX_ITEMS && lvl)
				buc->inventory[idx] = (int)lvl;
		}
	}

	list = DK_AT(e, DK_UENT_OFF_INVENTORY, void *);
	for (node = list ? DK_AT(list, DK_INVLIST_OFF_HEAD, void *) : NULL;
	     node && guard++ < 256; node = DK_AT(node, DK_INVITEM_OFF_NEXT, void *)) {
		void *def = DK_AT(node, DK_INVITEM_OFF_DATA, void *);
		int idx = def ? inventory_index(DK_AT(def, DK_INVDEF_OFF_NAME,
		                                      const char *)) : 0;

		if (idx <= 0 || idx >= DK_BL_MAX_ITEMS)
			continue;
		buc->inventory[idx] = 1;
		/* size_t in the engine; unsigned long is only 32 bits on Windows */
		if (DK_AT(def, DK_INVDEF_OFF_SIZE, uint64_t) >= DK_INVDEF_AMMO_SIZE
		    && (DK_AT(def, DK_INVDEF_OFF_FLAGS, unsigned int) & DK_INVDEF_FLAG_AMMO)) {
			int count = DK_AT(def, DK_INVDEF_OFF_AMMO_COUNT, int);

			buc->inventory[idx] = count > 0 ? count : 0;
		}
	}

	client = DK_AT(e, DK_EDICT_OFF_CLIENT, void *);
	if (!client)
		return;
	ps = (const unsigned char *)client + DK_CLIENT_OFF_PS;
	pms = ps + DK_PS_OFF_PMOVE;

	buc->pm_type  = DK_AT(pms, DK_PM_OFF_TYPE, int);
	buc->pm_flags = (unsigned char)DK_AT(pms, DK_PM_OFF_FLAGS, unsigned short);
	buc->pm_time  = DK_AT(pms, DK_PM_OFF_TIME, unsigned char);
	buc->gravity  = (float)DK_AT(pms, DK_PM_OFF_GRAVITY, short);
	for (i = 0; i < 3; i++)
		buc->delta_angles[i] =
		    DK_SHORT2ANGLE(((const short *)(pms + DK_PM_OFF_DELTA_ANGLES))[i]);
	vcopy(buc->viewangles, (const float *)(ps + DK_PS_OFF_VIEWANGLES));
	vcopy(buc->viewoffset, (const float *)(ps + DK_PS_OFF_VIEWOFFSET));
	vcopy(buc->kick_angles, (const float *)(ps + DK_PS_OFF_KICKANGLES));
	buc->fov     = DK_AT(ps, DK_PS_OFF_FOV, float);
	buc->rdflags = DK_AT(ps, DK_PS_OFF_RDFLAGS, int);
	/* ps.stats is not copied: its slots mean other things than Quake II's. */
}

void dkbot_fill_entity(const edict_t *ent, dk_bot_updateentity_t *bue)
{
	const unsigned char *e = (const unsigned char *)ent;
	const float *origin = (const float *)(e + DK_EDICT_OFF_ORIGIN);
	const float *absmin = (const float *)(e + DK_EDICT_OFF_ABSMIN);
	const float *absmax = (const float *)(e + DK_EDICT_OFF_ABSMAX);
	int i, num, maxclients;

	memset(bue, 0, sizeof *bue);
	vcopy(bue->origin, origin);
	vcopy(bue->angles, (const float *)(e + DK_EDICT_OFF_ANGLES));
	vcopy(bue->old_origin, (const float *)(e + DK_ES_OFF_OLD_ORIGIN));
	/* Deliberately the link box, a unit larger than entity_state_t.mins/maxs. */
	for (i = 0; i < 3; i++) {
		bue->mins[i] = absmin[i] - origin[i];
		bue->maxs[i] = absmax[i] - origin[i];
	}
	bue->solid       = DK_AT(e, DK_EDICT_OFF_SOLID, int);
	bue->modelindex  = DK_AT(e, DK_ES_OFF_MODELINDEX, int);
	bue->modelindex2 = DK_AT(e, DK_ES_OFF_MODELINDEX2, int);
	bue->frame       = DK_AT(e, DK_ES_OFF_FRAME, int);
	bue->skinnum     = DK_AT(e, DK_ES_OFF_SKINNUM, int);
	bue->effects     = DK_AT(e, DK_ES_OFF_EFFECTS, int);
	bue->renderfx    = DK_AT(e, DK_ES_OFF_RENDERFX, int);

	num = dkbot_entity_number(ent);
	maxclients = (dk_glob.maxclients && *dk_glob.maxclients)
	             ? (*dk_glob.maxclients)->intValue : 0;
	if (num >= 1 && num <= maxclients && DK_AT(e, DK_EDICT_OFF_CLIENT, void *)) {
		const unsigned char *hook = dkbot_player_hook(ent);
		int team = DK_AT(e, DK_UENT_OFF_TEAM, int);
		int state = DK_ENT_PLAYER;

		if (!DK_AT(e, DK_UENT_OFF_DEADFLAG, int)
		    && DK_AT(e, DK_UENT_OFF_HEALTH, float) > 0.0f)
			state |= DK_ENT_ALIVE;
		if (dkbot_slot_is_firing(num))
			state |= DK_ENT_SHOOTING;
		if (hook && DK_AT(hook, DK_HOOK_OFF_INVULN_TIME, float)
		            > dkbot_server_time())
			state |= DK_ENT_INVULN;
		if (team >= 0 && team < DK_ENT_TEAM_MASK)
			state |= (team + 1) << DK_ENT_TEAM_SHIFT;
		bue->modelindex2 = state;
	}
}

int dkbot_entity_number(const edict_t *ent)
{
	dk_game_export_t *ge = dk_glob.ge;
	ptrdiff_t off;

	if (!ge || !ge->edicts || ge->edict_size <= 0 || !ent)
		return -1;
	off = (const unsigned char *)ent - (const unsigned char *)ge->edicts;
	if (off < 0 || off % ge->edict_size)
		return -1;
	return (int)(off / ge->edict_size);
}

/* Quake II's AngleVectors. */
void dk_angle_vectors(const float *angles, float *forward, float *right,
                      float *up)
{
	float angle, sr, sp, sy, cr, cp, cy;

	angle = angles[1] * (float)(M_PI * 2 / 360);
	sy = sinf(angle);
	cy = cosf(angle);
	angle = angles[0] * (float)(M_PI * 2 / 360);
	sp = sinf(angle);
	cp = cosf(angle);
	angle = angles[2] * (float)(M_PI * 2 / 360);
	sr = sinf(angle);
	cr = cosf(angle);
	if (forward) {
		forward[0] = cp * cy;
		forward[1] = cp * sy;
		forward[2] = -sp;
	}
	if (right) {
		right[0] = -sr * sp * cy + cr * sy;
		right[1] = -sr * sp * sy - cr * cy;
		right[2] = -sr * cp;
	}
	if (up) {
		up[0] = cr * sp * cy + sr * sy;
		up[1] = cr * sp * sy - sr * cy;
		up[2] = cr * cp;
	}
}

float dk_dot(const float *a, const float *b)
{
	return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}
