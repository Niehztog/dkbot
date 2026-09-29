/* dkbot: be_ai2_dmq2.c adapted to Daikatana and compiled in its place */
/*
 * be_ai2_dmq2.c — Gladiator Bot v0.96 botlib (Mr. Elusive, 1999), reconstructed
 * from the Windows gladiator.dll.  DLL extent 0x10020ED0..0x10028C30.
 */

#include "botlib_port.h"
#include "l_libvar.h"
#include "dk/dk_entstate.h"  /* dkbot */
#include "dk/dk_inventory.h" /* dkbot */
#undef VectorNegate
#include "be_ea.h"
#include "q2files.h"
#include "aasfile.h"
#include "be_aas_def.h"
#include "l_script.h"
#include "l_precomp.h"
#include "l_struct.h"
#include "l_utils.h"
#include "be_ai_def.h"
#include "be_interface.h"
#include "struct_sizes_asserts.h"
#include "be_ai2_dmq2.h"
#include "be_aas_bspq2.h"
#include "be_aas_debug.h"
#include "be_aas_entity.h"
#include "be_aas_light.h"
#include "be_aas_main.h"
#include "be_aas_reach.h"
#include "be_aas_route.h"  /* dkbot */
#include "be_aas_sample.h"
#include "be_ai2_dmnet.h"
#include "be_ai2_main.h"
#include "be_ai_char.h"
#include "be_ai_chat.h"
#include "be_ai_goal.h"
#include "be_ai_move.h"
#include "be_ai_weap.h"
#include "be_ea.h"
#include "be_interface.h"
#include "l_libvar.h"
#include "l_memory.h"
#include "l_utils.h"
//
#include "chars.h"				//characteristics
#include "inv.h"				//indexes into the inventory
#include "syn.h"				//synonyms
#include "match.h"				//string matching types and vars

/* dkbot: weaponinfo_t has no range, so reach arrives as reach_<classname> libvars; 0 means unknown */
#define DK_MAX_REACH_CACHE 64
static float dk_reach_cache[DK_MAX_REACH_CACHE];
static char  dk_reach_known[DK_MAX_REACH_CACHE];

static float dk_weapon_reach(weaponinfo_t *wi)
{
  char var[128];

  if ( !wi || wi->number < 0 || wi->number >= DK_MAX_REACH_CACHE )
    return 0.0f;
  if ( !dk_reach_known[wi->number] )
  {
    Com_sprintf(var, sizeof(var), "reach_%s", wi->name);
    dk_reach_cache[wi->number] = LibVarGetValue(var);
    dk_reach_known[wi->number] = 1;
  }
  return dk_reach_cache[wi->number];
}

/* Armour points: a slot between our items (80..) and the library's (200..) */
#define DK_INVENTORY_ARMOR      190
#define DK_AGGR_SHOTS 5
#define DK_AGGR_ARMOR 50
#define DK_REACH_SLACK 1.25f
/* How far two bots close in a think; tools/gen-botcfg.py uses the same value */
#define DK_SPLASH_SLACK 64.0f
/* A projectile's clipmask: MASK_SHOT plus 0x80, which the maps put on see-through brushes */
#define DK_MASK_PROJECTILE 0x6000083
#define DK_SLOW_PROJECTILE 1000.0f
#define DK_SNIPER_REACH    1000.0f

/* tools/gen-botcfg.py flags the Zeus: the engine picks its target, and strikes the wielder if none */
#define DK_WFL_ENGINE_TARGET 0x100
#define DK_MASK_OPAQUE       0x19

static int dk_engine_target(bot_state_t *bs, weaponinfo_t *wi)
{
  aas_entityinfo_t ei = AAS_EntityInfo(bs->enemy);
  vec3_t dir, angles, nobox = { 0.0f, 0.0f, 0.0f };
  bsp_trace_t trace;
  float yaw;

  VectorSubtract(ei.origin, bs->origin, dir);
  if ( VectorLength(dir) > 0.75f * dk_weapon_reach(wi) )
    return 0;
  Vector2Angles(dir, angles);
  yaw = anglemod(angles[1] - bs->viewangles[1]);
  if ( yaw > 20.0f && yaw < 340.0f )
    return 0;
  trace = AAS_Trace(bs->origin, nobox, nobox, ei.origin, bs->entitynum, DK_MASK_OPAQUE);
  return trace.fraction >= 1.0f;
}

static int dk_within_reach(bot_state_t *bs, weaponinfo_t *wi)
{
  float reach = dk_weapon_reach(wi);
  aas_entityinfo_t ei;
  vec3_t d;

  if ( reach <= 0.0f )
    return 1;
  ei = AAS_EntityInfo(bs->enemy);
  VectorSubtract(ei.origin, bs->origin, d);
  return VectorLength(d) <= reach * (wi->ammoindex ? 1.0f : DK_REACH_SLACK);
}
int DK_LevelHasWeapons(void);
/* end dkbot */

libvar_t *ctf; /* libvar handle */

bot_goal_t ctf_flag2; /* 0x100643E0 blue flag goal (ai_dmq3.c; was unk_100643E0) */

bot_goal_t ctf_flag1;  /* 0x10064420 red flag goal  (ai_dmq3.c; was unk_10064420) */

libvar_t *usehook; /* libvar handle */

libvar_t *techs; /* libvar handle */

libvar_t *rocketjump; /* libvar handle */

libvar_t *ch; /* libvar handle */
libvar_t *teamplay; /* libvar handle */
libvar_t *ra; /* libvar handle */
int mapchange; // weak
libvar_t *dmflags; /* libvar handle */
libvar_t *nochat; /* libvar handle */
libvar_t *fastchat; /* libvar handle */
libvar_t *assimilation; /* libvar handle */
int modelindex3_flag1; // weak
libvar_t *teamplay_shell; /* libvar handle */
int modelindex3_flag2; // weak
int modelindex_tech4; // weak
int modelindex_tech3; // weak
int modelindex_tech2; // weak
int modelindex_tech1; // weak

// gladiator.dll: 10020ED0..10020F9F
// gladi386.so:   0002BA08..0002BAD3
void __cdecl BotEntityInfo(bot_state_t *bs, _DWORD *info)
{

  unsigned int v3; // edx
  unsigned int v4; // edx
  unsigned int v5; // edx

  *info = (*(int *)&bs->snapshot.origin[0]);
  info[1] = (*(int *)&bs->snapshot.origin[1]);
  info[2] = (*(int *)&bs->snapshot.origin[2]);
  info[3] = (*(int *)&bs->snapshot.velocity[0]);
  info[4] = (*(int *)&bs->snapshot.velocity[1]);
  info[5] = (*(int *)&bs->snapshot.velocity[2]);
  info[6] = (*(int *)&bs->snapshot.viewoffset[0]);
  info[7] = (*(int *)&bs->snapshot.viewoffset[1]);
  info[8] = (*(int *)&bs->snapshot.viewoffset[2]);
  info[9] = bs->entitynum;
  info[10] = bs->client;
  info[11] = *(int *)&bs->thinktime;
  v3 = info[24] & ~MFL_ONGROUND;
  info[24] = v3;
  if ( (bs->snapshot.pm_flags & PMF_ON_GROUND) != 0 )
    info[24] = v3 | MFL_ONGROUND;
  v4 = info[24] & ~MFL_TELEPORTED;
  info[24] = v4;
  if ( (bs->snapshot.pm_flags & PMF_TIME_TELEPORT) != 0 && bs->snapshot.pm_time > 0 )
    info[24] = v4 | MFL_TELEPORTED;
  v5 = info[24] & ~MFL_WATERJUMP;
  info[24] = v5;
  if ( (bs->snapshot.pm_flags & PMF_TIME_WATERJUMP) != 0 && bs->snapshot.pm_time > 0 )
    info[24] = v5 | MFL_WATERJUMP;
  if ( (bs->snapshot.pm_flags & PMF_DUCKED) != 0 )
    info[12] = PRESENCE_CROUCH;
  else
    info[12] = PRESENCE_NORMAL;
  info[13] = *(int *)&bs->viewangles[0];
  info[14] = *(int *)&bs->viewangles[1];
  info[15] = *(int *)&bs->viewangles[2];
}

// gladiator.dll: 10020FE0..10021008
// gladi386.so:   0002BAD4..0002BB0A
char *__cdecl sub_10020FE0(bot_state_t *bs, bot_weaponstate_t *ws)
{

  char *result; // eax

  ws->client = bs->client;
  ws->inventory = bs->inventory;
  result = AAS_ModelFromIndex(bs->snapshot.gunindex);
  ws->modelname = result;
  return result;
}

// gladiator.dll: 10021020..1002120B
// gladi386.so:   0002BB0C..0002BD98
void __cdecl BotUpdateInventory(bot_state_t *bs)
{
  int *inventory;
  short *stats;
  char *name;

  inventory = bs->inventory;
  stats = bs->snapshot.stats;
  inventory[INVENTORY_HEALTH] = stats[STAT_HEALTH];
  if ( stats[STAT_TIMER_ICON] )
  {
    name = AAS_ImageFromIndex(stats[STAT_TIMER_ICON]);
    if ( !_strcmpi(name, "p_quad") )
      bs->quad_endtime = AAS_Time() + (float)stats[STAT_TIMER];
    else if ( !_strcmpi(name, "p_invulnerability") )
      bs->invulnerability_endtime = AAS_Time() + (float)stats[STAT_TIMER];
    else if ( !_strcmpi(name, "p_rebreather") )
      bs->rebreather_endtime = AAS_Time() + (float)stats[STAT_TIMER];
    else if ( !_strcmpi(name, "p_envirosuit") )
      bs->enviro_endtime = AAS_Time() + (float)stats[STAT_TIMER];
  }
  /* dkbot: timed effects arrive as seconds left; STAT_TIMER_ICON is never filled */
  if ( stats[DK_STAT_INVULN_SECS] > 0 )
    bs->invulnerability_endtime = AAS_Time() + (float)stats[DK_STAT_INVULN_SECS];
  if ( stats[DK_STAT_OXYLUNG_SECS] > 0 )
    bs->rebreather_endtime = AAS_Time() + (float)stats[DK_STAT_OXYLUNG_SECS];
  if ( stats[DK_STAT_ENVIROSUIT_SECS] > 0 )
    bs->enviro_endtime = AAS_Time() + (float)stats[DK_STAT_ENVIROSUIT_SECS];
  inventory[DK_INVENTORY_ARMOR] = stats[DK_STAT_ARMOR];  /* dkbot */
  inventory[USING_QUAD] = bs->quad_endtime - AAS_Time();
  if ( inventory[USING_QUAD] <= 0 )
    inventory[USING_QUAD] = 0;
  inventory[USING_INVULNERABILITY] = bs->invulnerability_endtime - AAS_Time();
  if ( inventory[USING_INVULNERABILITY] <= 0 )
    inventory[USING_INVULNERABILITY] = 0;
  inventory[USING_REBREATHER] = bs->rebreather_endtime - AAS_Time();
  if ( inventory[USING_REBREATHER] <= 0 )
    inventory[USING_REBREATHER] = 0;
  inventory[USING_ENVIRONMENTSUIT] = bs->enviro_endtime - AAS_Time();
  if ( inventory[USING_ENVIRONMENTSUIT] <= 0 )
    inventory[USING_ENVIRONMENTSUIT] = 0;
  if ( stats[STAT_ARMOR_ICON] )
  {
    name = AAS_ImageFromIndex(stats[STAT_ARMOR_ICON]);
    if ( !_strcmpi(name, "i_powershield") )
      bs->powerscreen_seen_time = AAS_Time();
    if ( bs->powerscreen_seen_time > AAS_Time() - 0.9 )
    {
      inventory[USING_POWERSCREEN] = inventory[INVENTORY_CELLS];
      inventory[USING_POWERSHIELD] = inventory[INVENTORY_CELLS];
    }
    else
    {
      inventory[USING_POWERSCREEN] = 0;
      inventory[USING_POWERSHIELD] = 0;
    }
  }
}

// gladiator.dll: 10021290..10021427
// gladi386.so:   0002BD98..0002C01B
void __cdecl BotUpdateBattleInventory(bot_state_t *bs, int enemy)
{

  vec3_t dir; // [esp+8h] [ebp-88h] BYREF
  /* Properly-typed struct, not `float entinfo[31]` + a type-punned cast assignment:
   * writing AAS_EntityInfo()'s struct-by-value return through a cast to a
   * differently-declared object defeats gcc's return-slot forwarding, forcing a
   * hidden temp plus an extra 124-byte rep-movs copy. */
  aas_entityinfo_t entinfo; // [esp+14h] [ebp-7Ch] BYREF

  entinfo = AAS_EntityInfo(enemy);
  VectorSubtract(entinfo.origin, bs->origin, dir);
  bs->inventory[ENEMY_HEIGHT] = (int)dir[2];
  dir[2] = 0.0;
  bs->inventory[ENEMY_HORIZONTAL_DIST] = (int)VectorLength(dir);
  /* dkbot: Daikatana's skinnum and effects mean other things */
  bs->inventory[ENEMY_INVULNERABILITY] =
      (entinfo.modelindex2 & DK_ENT_INVULN) != 0;
}

// gladiator.dll: 100214E0..100214EC
// gladi386.so:   0002C01C..0002C028
/* Returns the signed 16-bit snapshot.stats[16].  DEAD in Gladiator, so no call
 * site confirms the parameter type; bot_state_t * matches both neighbours. */
int __cdecl sub_100214E0(bot_state_t *p)
{
  return p->snapshot.stats[STAT_CHASE];
}

// gladiator.dll: 10021500..100215A4
// gladi386.so:   0002C028..0002C0DB
void __cdecl BotBattleUseItems(bot_state_t *bs)
{
  if ( bs->inventory[INVENTORY_SILENCER] > 0 )
    EA_UseItem(bs->client, "Silencer");
  if ( (AAS_PointContents(bs->eye) & (CONTENTS_WATER|CONTENTS_SLIME|CONTENTS_LAVA)) != 0
       && !bs->inventory[USING_REBREATHER]
       && bs->inventory[INVENTORY_REBREATHER] > 0 )
    EA_UseItem(bs->client, "Rebreather");
  if ( !bs->inventory[USING_POWERSHIELD] && bs->inventory[INVENTORY_POWERSHIELD] > 0 )
    EA_UseItem(bs->client, "Power Shield");
  if ( !bs->inventory[USING_POWERSCREEN] && bs->inventory[INVENTORY_POWERSCREEN] > 0 )
    EA_UseItem(bs->client, "Power Screen");
}

// gladiator.dll: 100215E0..10021630
// gladi386.so:   0002C0DC..0002C13B
void __cdecl sub_100215E0(bot_state_t *bs)
{
  if ( !bs->inventory[USING_QUAD] && bs->inventory[INVENTORY_QUAD] > 0 )
  {
    EA_UseItem(bs->client, "Quad Damage");
    return;
  }
  if ( !bs->inventory[USING_INVULNERABILITY] && bs->inventory[INVENTORY_INVULNERABILITY] > 0 )
    EA_UseItem(bs->client, "Invulnerability");
}

// gladiator.dll: 10021650..10021690
// gladi386.so:   0002C13C..0002C18C
int __cdecl BotCTFCarryingFlag(bot_state_t *bs)
{
  if ( ctf->value == 0.0f )
    return 0;
  /* Returns 1 (red flag), 2 (blue flag), or 0.  Four separate `return` statements,
   * not a ternary on the last pair: gcc cross-jumps the trailing `return 0` back
   * onto the first one, where a ternary would pre-zero eax. */
  /* dkbot */
  if ( bs->inventory[DK_INVENTORY_FLAG_TEAM1] > 0 )
    return 1;
  if ( bs->inventory[DK_INVENTORY_FLAG_TEAM2] > 0 )
    return 2;
  return 0;
}

// gladiator.dll: 100216A0..100216BA
// gladi386.so:   0002C18C..0002C1A2
BOOL __cdecl BotIsDead(bot_state_t *bs)
{
  int v1; // eax

  v1 = bs->snapshot.pm_type;
  return v1 == PM_DEAD || v1 == PM_GIB;
}

// gladiator.dll: 100216D0..100216DE
// gladi386.so:   0002C1A4..0002C1B5
BOOL __cdecl BotIsObserver(bot_state_t *bs)
{
  return bs->snapshot.pm_type == PM_SPECTATOR;
}

// gladiator.dll: 100216F0..100216FE
// gladi386.so:   0002C1B8..0002C1C9
BOOL __cdecl BotIntermission(bot_state_t *bs)
{
  return bs->snapshot.pm_type == PM_FREEZE;
}

// gladiator.dll: 10021710..10021752
// gladi386.so:   0002C1CC..0002C223
/* Q3's EntityIsDead: its signature, its slot right before EntityIsShooting, and
 * its four callers, BotFindEnemy's skip test and the three AI nodes' enemy-dead
 * tests.  The body is the Q2 test, where Q3 asks the client's pm_type: a gib or a
 * corpse with flies, a client without the player model, or a client in a death
 * frame (173..197). */
BOOL __cdecl EntityIsDead(aas_entityinfo_t *entinfo)
{
  /* dkbot: player and alive come from modelindex2 */
  if ( entinfo->number < 1 || entinfo->number > botlibglobals.num_clients )
    return 1;
  if ( (entinfo->modelindex2 & DK_ENT_PLAYER) == 0 || (entinfo->modelindex2 & DK_ENT_ALIVE) == 0 )
    return 1;
  return 0;
}

// gladiator.dll: 10021780..100217A3
// gladi386.so:   0002C224..0002C245
BOOL __cdecl EntityIsShooting(intptr_t a1)
{
  aas_entityinfo_t *ent = (aas_entityinfo_t *)a1;

  /* dkbot */
  return (ent->modelindex2 & DK_ENT_SHOOTING) != 0;
}

// gladiator.dll: 100217C0..10021836
// gladi386.so:   0002C248..0002C2DB
char *__cdecl stristr(char *str, char *charset)
{
  int i;

  while ( *str )
  {
    for ( i = 0; charset[i] && str[i]; i++ )
    {
      if ( toupper(charset[i]) != toupper(str[i]) )
        break;
    }
    if ( !charset[i] )
      return str;
    str++;
  }
  return 0;
}

// gladiator.dll: 10021860..10021A12
// gladi386.so:   0002C2DC..0002C4AA
char *__cdecl EasyClientName(int client, char *buf)
{
  int ci; // eax (strength-reduced walk-pointer over Str)
  char *i; // edx
  char *str1; // esi
  char *str2; // eax
  char *ptr; // esi
  char c; // al
  /* One 128-byte stack buffer (the original's `sub esp,0x80`). */
  char Str[128]; // [esp+8h] [ebp-80h] BYREF

  strcpy(Str, (const char *)ClientName(client));
  for ( ci = 0; Str[ci]; ci++ )
    Str[ci] &= 0x7F;
  for ( i = strstr(Str, " "); i; i = strstr(Str, " ") )
    memmove(i, i + 1, strlen(i + 1) + 1);  /* memmove, not strcpy: the ranges overlap */
  str1 = strstr(Str, "[");
  str2 = strstr(Str, "]");
  if ( str1 && str2 )
  {
    /* overlapping shift-left: use memmove (was strcpy — UB on aarch64) */
    if ( str2 > str1 )
      memmove(str1, str2 + 1, strlen(str2 + 1) + 1);
    else
      memmove(str2, str1 + 1, strlen(str1 + 1) + 1);
  }
  if ( (Str[0] == 109 || Str[0] == 77) && (Str[1] == 114 || Str[1] == 82) )
  {
    memmove(Str, &Str[2], strlen(&Str[2]) + 1);  /* was strcpy — UB on aarch64 */
  }
  ptr = Str;
  if ( *ptr )
  {
    do
    {
      c = *ptr;
      if ( c >= 97 && c <= 122 || c >= 48 && c <= 57 || c == 95 )
      {
        ++ptr;
      }
      else if ( c >= 65 && c <= 90 )
      {
        *ptr++ = c + 32;
      }
      else
      {
        memmove(ptr, ptr + 1, strlen(ptr + 1) + 1);
      }
    }
    while ( *ptr );
  }
  strcpy(buf, Str);
  return buf;
}

// gladiator.dll: 10021A90..10021B1A
// gladi386.so:   0002C4AC..0002C57E
bot_waypoint_t *__cdecl BotCreateWayPoint(const char *name, vec3_t origin, int areanum)
{
  /* The original allocates strlen(name)+1+68 with a hand-laid header; a real struct
   * here, with the name buffer still inline right after the node so one FreeMemory
   * releases both. */
  bot_waypoint_t *wp;
  vec3_t mins = { -8.0f, -8.0f, -8.0f };
  vec3_t maxs = {  8.0f,  8.0f,  8.0f };
  size_t namelen = strlen(name);

  wp = (bot_waypoint_t *)GetMemory((unsigned)(sizeof(bot_waypoint_t) + namelen + 1));
  wp->name = (char *)(wp + 1);
  strcpy(wp->name, name);
  VectorCopy(origin, wp->goal.origin);
  VectorCopy(mins, wp->goal.mins);
  VectorCopy(maxs, wp->goal.maxs);
  wp->goal.areanum = areanum;
  wp->next = NULL;
  wp->prev = NULL;
  return wp;
}

// gladiator.dll: 10021B50..10021B7F
// gladi386.so:   0002C580..0002C5C1
bot_waypoint_t *__cdecl BotFindWayPoint(bot_waypoint_t *waypoints, char *name)
{
  bot_waypoint_t *wp;

  for ( wp = waypoints; wp; wp = wp->next )
  {
    if ( !_strcmpi(wp->name, name) )
      return wp;
  }
  return NULL;
}

// gladiator.dll: 10021B90..10021BAD
// gladi386.so:   0002C5C4..0002C5F1
void __cdecl BotFreeWaypoints(bot_waypoint_t *wp)
{
  bot_waypoint_t *next;

  for ( ; wp; wp = next )
  {
    next = wp->next;
    FreeMemory(wp);
  }
}

// gladiator.dll: 10021BC0..10021D13
// gladi386.so:   0002C5F4..0002C79E
BOOL __cdecl BotValidChatPosition(bot_state_t *bs)
{

  char v4; // al
  char v7; // al
  /* Real vec3_t, written with Q3's exact statements
   * (`VectorCopy(bs->origin, X); X[2] += K;`): MSVC6 forwards the [2] copy into the
   * arithmetic and interleaves the two surviving integer copies between the fld and
   * the fadd.  Copying start and end from the SAME source produces the original's
   * register reuse. */
  vec3_t point; // [esp+4h] [ebp-90h] BYREF
  vec3_t start; // [esp+10h] [ebp-84h] BYREF
  vec3_t end;   // [esp+1Ch] [ebp-78h] BYREF
  _DWORD mins[3]; // [esp+34h] [ebp-60h] BYREF
  _DWORD maxs[3]; // [esp+28h] [ebp-6Ch] BYREF
  bsp_trace_t trace; // [esp+40h] [ebp-54h] BYREF

  if ( BotIsDead(bs) )
    return 1;
  if ( (bs->snapshot.pm_flags & PMF_ON_GROUND) == 0 )
    return 0;
  VectorCopy(bs->origin, point);
  point[2] = point[2] - 24.0f;
  v4 = (char)AAS_PointContents(point);
  if ( (v4 & (CONTENTS_LAVA|CONTENTS_SLIME)) != 0 )
    return 0;
  VectorCopy(bs->origin, point);
  point[2] = point[2] + 32.0f;
  v7 = (char)AAS_PointContents(point);
  if ( (v7 & MASK_WATER) != 0 )
    return 0;
  VectorCopy(bs->origin, start);
  VectorCopy(bs->origin, end);
  start[2] = start[2] + 1.0f;
  end[2] = end[2] - 100.0f;
  AAS_PresenceTypeBoundingBox(PRESENCE_CROUCH, (float *)mins, (float *)maxs);
  /* Both images pass 4 as passent and the client number as the content mask
   * (push [esi+4], push 4 @10021ccf; the same pair @2c768), so for an even
   * client the mask lacks CONTENTS_SOLID and the trace can only report the
   * world.  GLAD_SERVERFIX(chat-position-trace-args) builds Q3's call,
   * BotAI_Trace(..., bs->client, MASK_SOLID), in Gladiator's terms: the bot's
   * own entity, which is bs->entitynum here, and Q2's MASK_SOLID. */
#if GLAD_SERVERFIX /* GLAD_SERVERFIX(chat-position-trace-args) */
  trace = AAS_Trace(start, (float*)mins, (float*)maxs, end, bs->entitynum, MASK_SOLID);
#else
  trace = AAS_Trace(start, (float*)mins, (float*)maxs, end, 4, bs->client);
#endif
  if ( trace.ent != 0 )
    return 0;
  return 1;
}

// gladiator.dll: 10021D80..10021E44
// gladi386.so:   0002C7A0..0002C888
BOOL __cdecl BotChat_EnterGame(bot_state_t *bs)
{

  float v1; // st7
  BOOL result; // eax
  float rnd; // [esp+4h] [ebp-24h]
  char name[32]; // [esp+8h] [ebp-20h] BYREF

  v1 = nochat->value;
  if ( v1 != 0.0f )
    return 0;
  rnd = (float)Characteristic_BFloat(BotCharacter(bs), CHARACTERISTIC_CHAT_ENTEREXITGAME, 0.0, 1.0);
  if ( fastchat->value == 0.0f )
  {
    if ( random() > rnd )
      return 0;
  }
  result = BotValidChatPosition(bs);
  if ( !result )
    return result;
  BotInitialChat(&bs->chatstate, "enter_game", EasyClientName(bs->client, name),
                 (char *)0);
  return 1;
}

// gladiator.dll: 10021E90..10021F42
// gladi386.so:   0002C888..0002C958
int __cdecl BotChat_ExitGame(bot_state_t *bs)
{

  float rnd; // [esp+4h] [ebp-24h]
  char name[32]; // [esp+8h] [ebp-20h] BYREF

  if ( nochat->value != 0.0f )
    return 0;
  rnd = (float)Characteristic_BFloat(BotCharacter(bs), CHARACTERISTIC_CHAT_ENTEREXITGAME, 0.0, 1.0);
  if ( fastchat->value == 0.0f )
  {
    if ( random() > rnd )
      return 0;
  }
  BotInitialChat(&bs->chatstate, "exit_game", EasyClientName(bs->client, name),
                 (char *)0);
  return 1;
}

// gladiator.dll: 10021F80..10022032
// gladi386.so:   0002C958..0002CA28
int __cdecl BotChat_StartLevel(bot_state_t *bs)
{

  float rnd; // [esp+4h] [ebp-24h]
  char name[32]; // [esp+8h] [ebp-20h] BYREF

  if ( nochat->value != 0.0f )
    return 0;
  rnd = (float)Characteristic_BFloat(BotCharacter(bs), CHARACTERISTIC_CHAT_STARTENDLEVEL, 0.0, 1.0);
  if ( fastchat->value == 0.0f )
  {
    if ( random() > rnd )
      return 0;
  }
  BotInitialChat(&bs->chatstate, "start_level",
                 EasyClientName(bs->client, name), (char *)0);
  return 1;
}

// gladiator.dll: 10022070..10022122
// gladi386.so:   0002CA28..0002CAF8
int __cdecl BotChat_EndLevel(bot_state_t *bs)
{

  float rnd; // [esp+4h] [ebp-24h]
  char name[32]; // [esp+8h] [ebp-20h] BYREF

  if ( nochat->value != 0.0f )
    return 0;
  rnd = (float)Characteristic_BFloat(BotCharacter(bs), CHARACTERISTIC_CHAT_STARTENDLEVEL, 0.0, 1.0);
  if ( fastchat->value == 0.0f )
  {
    if ( random() > rnd )
      return 0;
  }
  BotInitialChat(&bs->chatstate, "end_level", EasyClientName(bs->client, name),
                 (char *)0);
  return 1;
}

// gladiator.dll: 10022160..1002228C
// gladi386.so:   0002CAF8..0002CC50
int __cdecl BotChat_Death(int *bs)
{
  float v1; // st7
  int v4; // eax
  float v6; // [esp+4h] [ebp-24h]
  float v5; // st7
  _BYTE v7[32]; // [esp+8h] [ebp-20h] BYREF
  float v8; // [esp+2Ch] [ebp+4h]

  v1 = nochat->value;
  if ( v1 != 0.0f )
    return 0;
  v6 = (float)Characteristic_BFloat(BotCharacter((bot_state_t *)bs), CHARACTERISTIC_CHAT_DEATH, 0.0, 1.0);
  if ( fastchat->value == 0.0f )
  {
    if ( random() > v6 )
      return 0;
  }
  v4 = bs[1049];
  if ( v4 )
    EasyClientName(v4 - 1, v7);
  else
    strcpy(v7, "");
  if ( bs[693] == ST_DEATH_BFG )
  {
    BotInitialChat(bs + 995, "death_bfg", v7, (char *)0);
  }
  else
  {
    v5 = random();
    v8 = (float)Characteristic_BFloat(BotCharacter((bot_state_t *)bs), CHARACTERISTIC_CHAT_INSULT, 0.0, 1.0);
    if ( v5 < v8 )
      BotInitialChat(bs + 995, "death_insult", v7, (char *)0);
    else
      BotInitialChat(bs + 995, "death_praise", v7, (char *)0);
  }
  return 1;
}

// gladiator.dll: 100222E0..1002241E
// gladi386.so:   0002CC50..0002CDB8
BOOL __cdecl BotChat_Kill(int *bs)
{
  float v1; // st7
  BOOL result; // eax
  int v4; // eax
  float rnd; // [esp+4h] [ebp-24h]
  float v5; // st7
  _BYTE name[32]; // [esp+8h] [ebp-20h] BYREF
  float v8; // [esp+2Ch] [ebp+4h]

  v1 = nochat->value;
  if ( v1 != 0.0f )
    return 0;
  rnd = (float)Characteristic_BFloat(BotCharacter((bot_state_t *)bs), CHARACTERISTIC_CHAT_KILL, 0.0, 1.0);
  if ( fastchat->value == 0.0f )
  {
    if ( random() > rnd )
      return 0;
  }
  result = BotValidChatPosition((bot_state_t *)bs);
  if ( !result )
    return result;
  v4 = bs[1049];
  if ( v4 )
    EasyClientName(v4 - 1, name);
  else
    strcpy(name, "");
  if ( bs[692] == ST_DEATH_TELEFRAG )
  {
    BotInitialChat(bs + 995, "kill_telefrag", name, (char *)0);
  }
  else
  {
    v5 = random();
    v8 = (float)Characteristic_BFloat(BotCharacter((bot_state_t *)bs), CHARACTERISTIC_CHAT_INSULT, 0.0, 1.0);
    if ( v5 < v8 )
      BotInitialChat(bs + 995, "kill_insult", name, (char *)0);
    else
      BotInitialChat(bs + 995, "kill_praise", name, (char *)0);
  }
  return 1;
}

// gladiator.dll: 10022470..100225EE
// gladi386.so:   0002CDB8..0002CF54
int __cdecl BotChat_Random(bot_state_t *bs)
{

  float v1; // st7
  float rnd; // [esp+4h] [ebp-4h]

  v1 = nochat->value;
  if ( v1 != 0.0f )
    return 0;
  if ( bs->ltgtype == 1 || bs->ltgtype == 2 || bs->ltgtype == 5 )
    return 0;
  rnd = (float)Characteristic_BFloat(BotCharacter(bs), CHARACTERISTIC_CHAT_RANDOM, 0.0f, 1.0f);
  if ( random() > bs->thinktime * 0.1 )
    return 0;
  if ( fastchat->value == 0.0f )
  {
    if ( random() > rnd || random() > 0.25 )
      return 0;
  }
  if ( !BotValidChatPosition(bs) )
    return 0;
  if ( random() < Characteristic_BFloat(BotCharacter(bs), CHARACTERISTIC_CHAT_MISC, 0.0f, 1.0f) )
  {
    BotInitialChat(&bs->chatstate, "random_misc", (char *)0);
    return 1;
  }
  BotInitialChat(&bs->chatstate, "random_insult", (char *)0);
  return 1;
}

// gladiator.dll: 10022650..10022693
// gladi386.so:   0002CF54..0002CFA7
float __cdecl BotChatTime(bot_state_t *bs)
{

  int cpm; // [esp+4h] [ebp-4h]

  cpm = Characteristic_BInteger(BotCharacter(bs), CHARACTERISTIC_CHAT_CPM, 1, 4000);
  return (int)BotChatLength(&bs->chatstate) * 30.0f / cpm;
}

// gladiator.dll: 100226C0..10022849
// gladi386.so:   0002CFA8..0002D0F4
float __cdecl BotAggression(bot_state_t *bs)
{
  int v2; // ecx

  if ( bs->inventory[USING_INVULNERABILITY] )
    return 100.0f;
  if ( bs->inventory[ENEMY_INVULNERABILITY] )
    return 0.0f;
  if ( bs->inventory[ENEMY_QUAD] && !bs->inventory[USING_QUAD] )
    return 0.0f;
  if ( bs->inventory[ENEMY_POWERSCREEN] && (!bs->inventory[USING_POWERSCREEN] || bs->inventory[INVENTORY_CELLS] < 50) )
    return 0.0f;
  if ( bs->inventory[ENEMY_HEIGHT] > 200 )
    return 0.0f;
  v2 = bs->inventory[INVENTORY_HEALTH];
  /* dkbot: Quake II's weapon and armour slots are always zero here */
  if ( v2 < 40 || (v2 < 70 && bs->inventory[DK_INVENTORY_ARMOR] < DK_AGGR_ARMOR) )
    return 0.0f;
  if ( weaponconfig )
  {
    int i;

    for ( i = 0; i < weaponconfig->numweapons; i++ )
    {
      weaponinfo_t *wi = &weaponconfig->weaponinfo[i];

      if ( wi->weaponindex <= 0 || wi->weaponindex >= MAX_ITEMS )
        continue;
      /* on a map with nothing to arm up with, the sword is worth fighting with */
      if ( !strcmp(wi->name, "weapon_daikatana") && bs->inventory[wi->weaponindex] > 0
        && !DK_LevelHasWeapons() )
        return 100.0f;
      if ( wi->ammoindex <= 0 || wi->ammoindex >= MAX_ITEMS || wi->ammoamount <= 0 )
        continue;
      if ( bs->inventory[wi->weaponindex] > 0
        && bs->inventory[wi->ammoindex] / wi->ammoamount >= DK_AGGR_SHOTS )
        return 100.0f;
    }
  }
  return 0.0f;
}

// gladiator.dll: 100228C0..1002290A
// gladi386.so:   0002D0F4..0002D173
BOOL __cdecl BotWantsToRetreat(int *bs)
{
  if ( BotCTFCarryingFlag(bs) )
    return 1;
  if ( bs[1065] == 4 )
    return 1;
  if ( BotAggression((bot_state_t *)bs) < 50.0f )
    return 1;
  return 0;
}

// gladiator.dll: 10022930..10022953
// gladi386.so:   0002D174..0002D1AF
BOOL __cdecl BotWantsToChase(int *bs)
{
  if ( BotAggression((bot_state_t *)bs) > 50.0f )
    return 1;
  return 0;
}

// gladiator.dll: 10022970..10022976
// gladi386.so:   0002D1B0..0002D1B6
/* Always-true predicate between BotWantsToChase and BotCanAndWantsToRocketJump —
 * probably a feature toggle that ended up hard-coded.  DEAD. */
int __cdecl BotWantsToHelp(bot_state_t *bs)
{
  return 1;
}

// gladiator.dll: 10022990..10022A23
// gladi386.so:   0002D1B8..0002D259
BOOL BotCanAndWantsToRocketJump(bot_state_t *bs)
{
  int v3;

  if ( bs->inventory[INVENTORY_ROCKETLAUNCHER] <= 0 )
    return 0;
  if ( bs->inventory[INVENTORY_ROCKETS] < 3 )
    return 0;
  if ( bs->inventory[USING_QUAD] )
    return 0;
  if ( bs->inventory[USING_INVULNERABILITY] )
    return 1;
  v3 = bs->inventory[INVENTORY_HEALTH];
  if ( v3 < 60 )
    return 0;
  if ( v3 < 90 && bs->inventory[INVENTORY_ARMORBODY] < 40 && bs->inventory[INVENTORY_ARMORCOMBAT] < 50 && bs->inventory[INVENTORY_ARMORJACKET] < 60 )
    return 0;
  if ( Characteristic_BFloat(BotCharacter(bs), CHARACTERISTIC_WEAPONJUMPING, 0.0, 1.0) < 0.5 )
    return 0;
  return 1;
}

// gladiator.dll: 10022A60..10022D42
// gladi386.so:   0002D25C..0002D5C1
void __cdecl BotRoamGoal(bot_state_t *bs, vec3_t goal)
{
  int pc;
  float len, rnd, sign, i;
  vec3_t dir, bestorg, belowbestorg;
  bsp_trace_t trace;

  for ( i = 0; i < 10; i++ )
  {
    VectorCopy(bs->origin, bestorg);
    rnd = random();
    if ( rnd < 0.8 )
    {
      if ( random() < 0.5 ) sign = -1;
      else sign = 1;
      bestorg[0] += sign * 700 * random() + 50;
    }
    if ( rnd > 0.2 )
    {
      if ( random() < 0.5 ) sign = -1;
      else sign = 1;
      bestorg[1] += sign * 700 * random() + 50;
    }
    bestorg[2] += random() * 144 - 96 - 1;
    trace = AAS_Trace(bs->origin, NULL, NULL, bestorg, bs->entitynum, MASK_SOLID);
    VectorSubtract(bestorg, bs->origin, dir);
    len = VectorNormalize(dir);
    if ( len > 100 )
    {
      VectorScale(dir, len * trace.fraction - 40, dir);
      VectorAdd(bs->origin, dir, bestorg);
      VectorCopy(bestorg, belowbestorg);
      belowbestorg[2] -= 800;
      trace = AAS_Trace(bestorg, NULL, NULL, belowbestorg, bs->entitynum, MASK_SOLID);
      if ( !trace.startsolid )
      {
        trace.endpos[2]++;
        pc = AAS_PointContents(trace.endpos);
        if ( !(pc & (CONTENTS_LAVA | CONTENTS_SLIME)) )
        {
          VectorCopy(bestorg, goal);
          return;
        }
      }
    }
  }
  VectorCopy(bestorg, goal);
}

// gladiator.dll: 10022E10..100233A0
// gladi386.so:   0002D5C4..0002DE0C
bot_moveresult_t __cdecl BotAttackMove(bot_state_t *bs, int tfl)
{
  int movetype, i;
  float attack_skill, jumper, croucher, dist, strafechange_time;
  vec3_t forward, backward, sideward, hordir, up = {0, 0, 1};
  aas_entityinfo_t entinfo;
  bot_moveresult_t moveresult;
  bot_goal_t goal;
  float near_dist = 100.0f, far_dist = 180.0f, reach;  /* dkbot */

  if ( bs->attackchase_time > AAS_Time() )
  {
    goal.entitynum = bs->enemy;
    goal.areanum = bs->lastenemyareanum;
    VectorCopy(bs->lastenemyorigin, goal.origin);
    VectorSet(goal.mins, -8, -8, -8);
    VectorSet(goal.maxs, 8, 8, 8);
    BotEntityInfo(bs, (_DWORD *)&bs->ms);
    return BotMoveToGoal((bot_movestate_t *)&bs->ms, &goal, tfl);
  }
  memset(&moveresult, 0, sizeof(bot_moveresult_t));
  /* chars.h: "does the bot client eat pizza while playing Q2".  One that does skips
   * its attack movement. */
  if ( random() < Characteristic_BFloat(BotCharacter(bs), CHARACTERISTIC_PIZZAPREFERENCE, 0, 1) )
    return moveresult;
  attack_skill = Characteristic_BFloat(BotCharacter(bs), CHARACTERISTIC_ATTACK_SKILL, 0, 1);
  jumper = Characteristic_BFloat(BotCharacter(bs), CHARACTERISTIC_JUMPER, 0, 1);
  croucher = Characteristic_BFloat(BotCharacter(bs), CHARACTERISTIC_CROUCHER, 0, 1);
  if ( attack_skill < 0.2 )
    return moveresult;
  BotEntityInfo(bs, (_DWORD *)&bs->ms);
  entinfo = AAS_EntityInfo(bs->enemy);
  VectorSubtract(entinfo.origin, bs->origin, forward);
  dist = VectorLength(forward);
  /* dkbot: a short-reach weapon fights from within its reach */
  reach = dk_weapon_reach(sub_100354B0(BotWS(bs)));
  if ( reach > 0.0f && reach < far_dist )
  {
    far_dist = 0.75f * reach;
    near_dist = 0.25f * reach;
  }
  VectorNormalize(forward);
  backward[0] = -forward[0];
  backward[1] = -forward[1];
  backward[2] = -forward[2];
  movetype = MOVE_WALK;
  if ( bs->attackcrouch_time < AAS_Time() - 1 )
  {
    if ( random() < jumper )
      movetype = MOVE_JUMP;
    else if ( bs->attackcrouch_time < AAS_Time() - 1 && random() < croucher )
      bs->attackcrouch_time = AAS_Time() + croucher * 5;
  }
  if ( bs->attackcrouch_time > AAS_Time() )
    movetype = MOVE_CROUCH;
  if ( movetype == MOVE_JUMP )
  {
    if ( bs->flags & BFL_ATTACKJUMPED )
    {
      bs->flags &= ~BFL_ATTACKJUMPED;
      movetype = MOVE_WALK;
    }
    else
      bs->flags |= BFL_ATTACKJUMPED;
  }
  if ( attack_skill <= 0.4 )
  {
    if ( dist > far_dist )  /* dkbot */
    {
      if ( BotMoveInDirection((bot_movestate_t *)&bs->ms, forward, 400, movetype) )
        return moveresult;
    }
    if ( dist < near_dist )  /* dkbot */
    {
      if ( BotMoveInDirection((bot_movestate_t *)&bs->ms, backward, 400, movetype) )
        return moveresult;
    }
    return moveresult;
  }
  bs->attackstrafe_drift += 0.1;
  strafechange_time = 0.4 + (1 - attack_skill) * 0.2;
  if ( attack_skill > 0.7 )
    strafechange_time += crandom() * 0.1;
  if ( bs->attackstrafe_drift > strafechange_time )
  {
    if ( random() > 0.935 )
    {
      bs->flags ^= BFL_STRAFERIGHT;
      bs->attackstrafe_drift = 0;
    }
  }
  for ( i = 0; i < 2; i++ )
  {
    hordir[0] = forward[0];
    hordir[1] = forward[1];
    hordir[2] = 0;
    VectorNormalize(hordir);
    CrossProduct(hordir, up, sideward);
    if ( bs->flags & BFL_STRAFERIGHT )
    {
      sideward[0] = -sideward[0];
      sideward[1] = -sideward[1];
      sideward[2] = -sideward[2];
    }
    if ( random() > 0.9 )
    {
      VectorAdd(sideward, backward, sideward);
    }
    else
    {
      if ( dist > far_dist )  /* dkbot */
        VectorAdd(sideward, forward, sideward);
      else if ( dist < near_dist )  /* dkbot */
        VectorAdd(sideward, backward, sideward);
    }
    if ( BotMoveInDirection((bot_movestate_t *)&bs->ms, sideward, 400, movetype) )
      return moveresult;
    bs->flags ^= BFL_STRAFERIGHT;
    bs->attackstrafe_drift = 0;
  }
  return moveresult;
}

// gladiator.dll: 10023510..10023533
// gladi386.so:   0002DE0C..0002DE4B
int __cdecl BotCTFTeam(bot_state_t *bs)
{
  const char *v1; // eax

  v1 = (const char *)ClientSkin(bs->client);
  if ( strstr(v1, "ctf_r") )
    return 1;
  return 2;
}

// gladiator.dll: 10023550..1002382D
// gladi386.so:   0002DE4C..0002E15D
BOOL __cdecl BotSameTeam(bot_state_t *bs, int entnum)
{
  aas_entityinfo_t entinfo;
  aas_entityinfo_t botinfo;
  char *team1, *team2;
  unsigned int len1, len2;

  entinfo = AAS_EntityInfo(entnum);
  if ( !entinfo.number )
    return 0;
  if ( teamplay_shell->value != 0.0f )
  {
    botinfo = AAS_EntityInfo(bs->entitynum);
    return (botinfo.renderfx & (RF_SHELL_RED|RF_SHELL_GREEN|RF_SHELL_BLUE))
        == (entinfo.renderfx & (RF_SHELL_RED|RF_SHELL_GREEN|RF_SHELL_BLUE));
  }
  if ( ch->value != 0.0f )
  {
    botinfo = AAS_EntityInfo(bs->entitynum);
    if ( botinfo.modelindex3 != entinfo.modelindex3 )
      return 1;
  }
  else if ( teamplay->value != 0.0f )
  {
    if ( !_strcmpi(ClientSkin(bs->client), ClientSkin(entinfo.number - 1)) )
      return 1;
    return 0;
  }
  else if ( ((int)dmflags->value & DF_SKINTEAMS) || ctf->value != 0.0f )
  {
    /* dkbot: the team is in modelindex2; 0 is none */
    int mine, yours;

    botinfo = AAS_EntityInfo(bs->entitynum);
    mine  = (botinfo.modelindex2 >> DK_ENT_TEAM_SHIFT) & DK_ENT_TEAM_MASK;
    yours = (entinfo.modelindex2 >> DK_ENT_TEAM_SHIFT) & DK_ENT_TEAM_MASK;
    if ( mine && mine == yours )
      return 1;
  }
  else if ( (int)dmflags->value & DF_MODELTEAMS )
  {
    team1 = strchr(ClientSkin(bs->client), '/');
    if ( team1 )
      len1 = team1 - ClientSkin(bs->client);
    else
      len1 = strlen(ClientSkin(bs->client));
    team2 = strchr(ClientSkin(entinfo.number - 1), '/');
    if ( team2 )
      len2 = team2 - ClientSkin(entinfo.number - 1);
    else
      len2 = strlen(ClientSkin(entinfo.number - 1));
    if ( len1 == len2 )
    {
      if ( !strncmp(ClientSkin(bs->client), ClientSkin(entinfo.number - 1), len1) )
        return 1;
    }
  }
  return 0;
}

// gladiator.dll: 100238F0..10023949
// gladi386.so:   0002E160..0002E1DD
int __cdecl BotNumTeamMates(bot_state_t *bs)
{
  int numplayers; // ebp
  int i; // esi

  numplayers = 0;
  for ( i = 0; i < botlibglobals.num_clients; i++ )
  {
    if ( strlen(clientsettings[i].netname) )
    {
      if ( BotSameTeam(bs, i + 1) )
        ++numplayers;
    }
  }
  return numplayers;
}

// gladiator.dll: 10023970..10023C30
// gladi386.so:   0002E1E0..0002E558
int __cdecl BotFindEnemy(bot_state_t *bs)
{
  int i, numents, alertness, healthdecrease;
  float dist, fov;
  int ents[16];
  aas_entityinfo_t entinfo;
  vec3_t dir, angles;

  /* chars.h's 3D-accelerator characteristic, an integer 0 or 1: without one the bot
   * sees no enemy beyond 900 units.  Q3 scales that range by CHARACTERISTIC_ALERTNESS
   * instead, whence the local's name. */
  alertness = Characteristic_BInteger(BotCharacter(bs), CHARACTERISTIC_3DACCELERATOR, 0, 1);
  //check if the health decreased
  healthdecrease = bs->lasthealth > bs->inventory[INVENTORY_HEALTH];
  //remember the current health value
  bs->lasthealth = bs->inventory[INVENTORY_HEALTH];
  numents = sub_1000BAA0(bs->entitynum, bs->eye, bs->viewangles, 360, 16, ents);
  for ( i = 0; i < numents; i++ )
  {
    entinfo = AAS_EntityInfo(ents[i]);
    //if the enemy isn't dead and the enemy isn't the bot self
    if ( EntityIsDead(&entinfo) || entinfo.number == bs->entitynum )
      continue;
    //calculate the distance towards the enemy
    VectorSubtract(entinfo.origin, bs->origin, dir);
    dist = VectorLength(dir);
    if ( !alertness && dist > 900 )
      continue;
    //if the bot's health decreased
    if ( healthdecrease )
      fov = 360;
    else
      fov = 360 - (270 - (dist > 810 ? 810 : dist) / 3);
    Vector2Angles(dir, angles);
    if ( !InFieldOfVision(bs->viewangles, fov, angles) )
      continue;
    //if on the same team
    if ( BotSameTeam(bs, ents[i]) )
      continue;
    if ( !healthdecrease || dist > 300 )
    {
      if ( AAS_PointLight(entinfo.origin, 0, 0, 0) < 5 )
        continue;
      if ( dist > 300 && !EntityIsShooting((intptr_t)&entinfo) )
      {
        //check if we can avoid this enemy
        VectorSubtract(bs->origin, entinfo.origin, dir);
        Vector2Angles(dir, angles);
        //if the bot isn't in the fov of the enemy
        if ( !InFieldOfVision(entinfo.angles, 160, angles) )
        {
          //update some stuff for this enemy
          BotUpdateBattleInventory(bs, ents[i]);
          //if the bot doesn't really want to fight
          if ( BotWantsToRetreat((int *)bs) )
            continue;
        }
      }
    }
    //found an enemy
    bs->enemy = entinfo.number;
    bs->enemysight_time = AAS_Time();
    return 1;
  }
  return 0;
}

// gladiator.dll: 10023CE0..100243C4
// gladi386.so:   0002E558..0002EDBF
void BotAimAtEnemy(bot_state_t *bs)
{
  int i;
  float dist, aim_skill, aim_accuracy, speed;
  /* Declaration order of the by-reference locals is Q3's own order in
   * ai_dmq3.c's BotAimAtEnemy -- `vec3_t dir, bestorigin, end, start,
   * groundtarget, ...;` then `mins`/`maxs`, then `entinfo` and `trace`.  It is
   * recoverable because gcc 2.7 lays the address-taken group out top-down in
   * declaration order, and the reference .so's slot order matches Q3's list
   * exactly.  The float spill slots (dist, aim_skill, aim_accuracy) follow the
   * same rule.  MSVC /O2 assigns slots in first-reference order, which makes
   * the DLL blind to this. */
  vec3_t dir, bestorigin, end, start, groundtarget;
  vec3_t mins = {-4, -4, -4}, maxs = {4, 4, 4};
  weaponinfo_t *wi;
  aas_entityinfo_t entinfo;
  bsp_trace_t trace;

  if ( bs->enemy )
  {
    aim_skill = Characteristic_BFloat(BotCharacter(bs), CHARACTERISTIC_AIM_SKILL, 0, 1);
    aim_accuracy = Characteristic_BFloat(BotCharacter(bs), CHARACTERISTIC_AIM_ACCURACY, 0, 1);
    //
    if ( aim_accuracy <= 0 )
      aim_accuracy = 0.0001f;
    //get the weapon information
    wi = sub_100354B0(BotWS(bs));
    /* dkbot: "Rocket Launcher": a projectile slow enough to lead */
    if ( wi->speed > 0.0f && wi->speed < DK_SLOW_PROJECTILE )
      aim_accuracy = sqrt(aim_accuracy);
    //get the enemy entity information
    entinfo = AAS_EntityInfo(bs->enemy);
    //the bot's aim point
    VectorCopy(entinfo.origin, bestorigin);
    bestorigin[2] += 8;
    //get the start point shooting from
    //NOTE: the x and y projectile start offsets are ignored
    VectorCopy(bs->origin, start);
    start[2] += bs->snapshot.viewoffset[2];
    start[2] += wi->offset[2];
    //
    trace = AAS_Trace(start, mins, maxs, bestorigin, bs->entitynum, MASK_SHOT);
    //if the enemy is NOT hit
    if ( trace.fraction <= 1 && trace.ent != entinfo.number )
      bestorigin[2] += 16;
    //if the weapon has a speed and the bot is skilled enough
    if ( wi->speed && aim_skill > 0.4 )
    {
      //direction towards the enemy
      VectorSubtract(entinfo.origin, bs->origin, dir);
      //distance towards the enemy
      dist = VectorLength(dir);
      //direction the enemy is moving in
      dir[0] = entinfo.origin[0] - entinfo.lastvisorigin[0];
      dir[1] = entinfo.origin[1] - entinfo.lastvisorigin[1];
      dir[2] = 0;
      //
      speed = VectorNormalize(dir) / entinfo.update_time;
      //best spot to aim at
      VectorMA(entinfo.origin, (dist / wi->speed) * speed, dir, bestorigin);
    }
    //if the projectile does radial damage
    if ( aim_skill > 0.6 && (wi->proj->damagetype & DAMAGETYPE_RADIAL) )
    {
      //if the enemy isn't standing significantly higher than the bot
      if ( entinfo.origin[2] < bs->origin[2] + 16 )
      {
        //try to aim at the ground in front of the enemy
        VectorCopy(entinfo.origin, end);
        end[2] -= 64;
        trace = AAS_Trace(entinfo.origin, NULL, NULL, end, entinfo.number, MASK_SHOT);
        //
        VectorCopy(bestorigin, groundtarget);
        if ( trace.startsolid )
          groundtarget[2] = entinfo.origin[2] - 16;
        else
          groundtarget[2] = trace.endpos[2] - 8;
        //trace a line from projectile start to ground target
        trace = AAS_Trace(start, NULL, NULL, groundtarget, bs->entitynum, MASK_SHOT);
        //if hitpoint is not vertically too far from the ground target
        if ( fabs(trace.endpos[2] - groundtarget[2]) < 50 )
        {
          VectorSubtract(trace.endpos, groundtarget, dir);
          //if the hitpoint is near enough the ground target
          if ( VectorLength(dir) < 60 )
          {
            VectorSubtract(trace.endpos, start, dir);
            //if the hitpoint is far enough from the bot
            if ( VectorLength(dir) > 150 )
            {
              //check if the bot is visible from the ground target
              trace = AAS_Trace(trace.endpos, NULL, NULL, entinfo.origin, entinfo.number, MASK_SHOT);
              if ( trace.fraction >= 1 )
                VectorCopy(groundtarget, bestorigin);
            }
          }
        }
      }
    }
    /* Q3's own phrasing, and it is load-bearing for the ELF: under
     * -ffast-math gcc 2.7's fold() rewrites `(C * crandom()) * x` into
     * `crandom() * (x * C)`, which is exactly the reference's evaluation
     * order, and CSEs the inline `(1 - aim_accuracy)` into the double spill
     * slot the reference has.  cl.exe reaches the DLL's `crandom() * x * C`
     * from the same text. */
    bestorigin[0] += 20 * crandom() * (1 - aim_accuracy);
    bestorigin[1] += 20 * crandom() * (1 - aim_accuracy);
    bestorigin[2] += 10 * crandom() * (1 - aim_accuracy);
    //get aim direction
    VectorSubtract(bestorigin, bs->eye, dir);
    //add some random stuff to the aim direction
    /* dkbot: "Railgun": instant and long-reaching */
    if ( wi->speed <= 0.0f && dk_weapon_reach(wi) >= DK_SNIPER_REACH )
    {
      VectorNormalize(dir);
      for ( i = 0; i < 3; i++ )
        dir[i] += 0.3 * crandom() * (1 - aim_accuracy);
    }
    //set the ideal view angles
    Vector2Angles(dir, bs->ideal_viewangles);
    //take the weapon spread into account for lower skilled bots
    bs->ideal_viewangles[PITCH] += 6 * wi->vspread * crandom() * (1 - aim_accuracy);
    bs->ideal_viewangles[PITCH] = anglemod(bs->ideal_viewangles[PITCH]);
    bs->ideal_viewangles[YAW] += 6 * wi->hspread * crandom() * (1 - aim_accuracy);
    bs->ideal_viewangles[YAW] = anglemod(bs->ideal_viewangles[YAW]);
    BotChangeViewAngles(bs, bs->thinktime);
    if ( aim_accuracy > 0.8 )
    {
      //set the view angles directly
      VectorCopy(bs->ideal_viewangles, bs->viewangles);
      EA_View(bs->client, bs->viewangles);
    }
  }
}

// gladiator.dll: 10024590..10024915
// gladi386.so:   0002EDC0..0002F17E
void BotCheckAttack(bot_state_t *bs)
{

  /* reactiontime/fov are declared BEFORE the pointers: MSVC6 coalesces the two into
   * one slot (their lifetimes do not overlap, which is why IDA shows both at
   * [esp+10h]), gcc 2.7 gives them one spill slot too -- and the ELF original puts
   * that slot ABOVE `wi`'s, which only this declaration order produces. */
  float reactiontime; // [esp+10h] [ebp-1A4h] — Characteristic_BFloat/AAS_Time splash-attack
                       // timer (Q3 ai_dmq3 BotCheckAttack 'reactiontime'); this and the
                       // fov local below are SEPARATE variables sharing one stack slot
                       // (non-overlapping lifetimes), not one variable reused.
  float fov; // [esp+10h] [ebp-1A4h] — visibility check distance (Q3 ai_dmq3 BotCheckAttack 'fov');
             // shares reactiontime's stack slot, see above.
  weaponinfo_t *wi; // ebx
  projectileinfo_t *v6; // ecx
  float points; // st — register-only (Q3 ai_dmq3 BotCheckAttack 'points')
  /* Q3 BotCheckAttack's declaration order, with mins/maxs LAST and mins before maxs:
   * gcc 2.7 lays the frame out in reverse declaration order, and the ELF original has
   * maxs BELOW mins, which only that order produces. */
  vec3_t forward; // [esp+20h] [ebp-194h] BYREF
  vec3_t right; // [esp+44h] [ebp-170h] BYREF
  vec3_t start; // [esp+14h] [ebp-1A0h] BYREF — trace start; as separate locals the
                // y/z stores get dead-store-eliminated and VectorMA/AAS_Trace see
                // garbage in [1]/[2]
  vec3_t end; // [esp+5Ch] [ebp-158h] BYREF
  vec3_t dir; // [esp+38h] [ebp-17Ch] BYREF
  bsp_trace_t trace; // [esp+68h] [ebp-14Ch] BYREF
  aas_entityinfo_t entinfo; // [esp+BCh] [ebp-7Ch] BYREF
  vec3_t mins; // [esp+50h] [ebp-164h] BYREF
  vec3_t maxs; // [esp+2Ch] [ebp-188h] BYREF
  float dk_impact;  /* dkbot */

  /* Float literals: mins/maxs are float[3], not raw bit patterns.  No `attackentity`
   * local: the original never caches bs->enemy here — it is a direct
   * `cmp [bs+enemy],0` right after the mins/maxs stores. */
  mins[0] = -8.0f;
  mins[1] = -8.0f;
  mins[2] = -8.0f;
  maxs[0] = 8.0f;
  maxs[1] = 8.0f;
  maxs[2] = 8.0f;
  if ( bs->enemy )
  {
    /* Characteristic_BFloat's FPU return drives the splash-attack timer. */
    reactiontime = Characteristic_BFloat(BotCharacter(bs), CHARACTERISTIC_REACTIONTIME, 0.0, 1.0);
    if ( AAS_Time() - reactiontime >= bs->enemysight_time )
    {
      entinfo = AAS_EntityInfo(bs->enemy);
      VectorSubtract(entinfo.origin, bs->origin, dir);
      if ( VectorLength(dir) < 100.0f )
        fov = 120.0f;
      else
        fov = 50.0f;
      if ( BotEntityVisible(bs->entitynum, bs->eye, bs->viewangles, fov, bs->enemy) )
      {
        wi = sub_100354B0(BotWS(bs));
        /* dkbot: hold fire out of reach; the bot still closes in */
        if ( wi && !dk_within_reach(bs, wi) )
          wi = 0;
        if ( wi && (wi->flags & DK_WFL_ENGINE_TARGET) && !dk_engine_target(bs, wi) )
          wi = 0;  /* dkbot */
        if ( wi )
        {
          VectorCopy(bs->origin, start);
          start[2] += bs->snapshot.viewoffset[2];
          AngleVectors(bs->viewangles, forward, right, 0);
          start[0] += forward[0] * wi->offset[0] + right[0] * wi->offset[1];
          start[1] += forward[1] * wi->offset[0] + right[1] * wi->offset[1];
          start[2] += forward[2] * wi->offset[0] + right[2] * wi->offset[1] + wi->offset[2];
          VectorMA(start, 1000.0, forward, end);
          VectorMA(start, -12.0, forward, start);
          trace = AAS_Trace(start, (float*)mins, (float*)maxs, (float*)(end), bs->entitynum, DK_MASK_PROJECTILE);
          /* dkbot: no radial shot into its own blast; a trace begun in solid keeps fraction 1 */
          dk_impact = ( trace.startsolid || trace.allsolid ) ? 0.0f : trace.fraction * 1000.0f;
          if ( (trace.ent == bs->enemy
                || trace.ent <= 0 || trace.ent > botlibglobals.num_clients
                || !BotSameTeam(bs, trace.ent))
            && ((v6 = wi->proj, (v6->damagetype & DAMAGETYPE_RADIAL) == 0)
             || dk_impact >= v6->radius + DK_SPLASH_SLACK) )
          {
            if ( (trace.contents & CONTENTS_WINDOW) != 0 )
            {
              entinfo = AAS_EntityInfo(bs->enemy);
              trace = AAS_Trace(trace.endpos, (float*)(uintptr_t)(0), (float*)(uintptr_t)(0),
                                entinfo.origin, bs->entitynum, MASK_SHOT);
              if ( trace.ent != bs->enemy )
                return;
            }
            if ( (wi->flags & WFL_FIRERELEASED) != 0 )
            {
              if ( (bs->flags & BFL_ATTACKED) != 0 )
                EA_Attack(bs->client);
            }
            else
            {
              EA_Attack(bs->client);
            }
            bs->flags ^= BFL_ATTACKED;
          }
        }
      }
    }
  }
}

// gladiator.dll: 10024A10..10024E9D
// gladi386.so:   0002F180..0002F53D
bsp_entity_t *__cdecl BotEntityToActivate(int entitynum)
{
  int i;
  char *model, *tmpmodel, *classname, *target, *targetname[10];
  bsp_entity_t *ent, *cur_entities[10];
  aas_entityinfo_t entinfo;

  entinfo = AAS_EntityInfo(entitynum);
  model = AAS_ModelFromIndex(entinfo.modelindex);
  for ( ent = entities; ent; ent = ent->next )
  {
    tmpmodel = AAS_ValueForBSPEpairKey(ent, "model");
    if ( !tmpmodel )
      continue;
    if ( !strcmp(model, tmpmodel) )
      break;
  }
  if ( !ent )
  {
    botimport.Print(PRT_ERROR, "BotEntityToActivate: no entity found with model %s\n", model);
    return NULL;
  }
  classname = AAS_ValueForBSPEpairKey(ent, "classname");
  if ( !classname )
  {
    botimport.Print(PRT_ERROR, "BotEntityToActivate: entity with model %s has no classname\n", model);
    return NULL;
  }
  if ( !strcmp(classname, "func_door_secret") )
  {
    targetname[0] = AAS_ValueForBSPEpairKey(ent, "targetname");
    target = AAS_ValueForBSPEpairKey(ent, "spawnflags");
    if ( !targetname[0] || (atoi(target) & 1) )
      return ent;
  }
  if ( !strcmp(classname, "func_door") )
  {
    if ( AAS_FloatForBSPEpairKey(ent, "health") )
      return ent;
  }
  targetname[0] = AAS_ValueForBSPEpairKey(ent, "targetname");
  if ( !targetname[0] )
    return NULL;
  cur_entities[0] = entities;
  for ( i = 0; i >= 0 && i < 10; )
  {
    for ( ent = cur_entities[i]; ent; ent = ent->next )
    {
      target = AAS_ValueForBSPEpairKey(ent, "target");
      if ( !target )
        continue;
      if ( !strcmp(targetname[i], target) )
      {
        cur_entities[i] = ent->next;
        break;
      }
    }
    if ( !ent )
    {
      botimport.Print(PRT_ERROR, "BotEntityToActivate: no entity with target \"%s\"\n", targetname[i]);
      i--;
      continue;
    }
    classname = AAS_ValueForBSPEpairKey(ent, "classname");
    if ( !classname )
    {
      botimport.Print(PRT_ERROR, "BotEntityToActivate: entity with target \"%s\" has no classname\n", targetname[i]);
      return NULL;
    }
    if ( !strcmp(classname, "trigger_counter") || !strcmp(classname, "trigger_relay") )
    {
      if ( i >= 9 )
      {
        botimport.Print(PRT_ERROR, "BotEntityToActivate: stacked up more than %d trigger_counter or trigger_relay\n", i);
        return NULL;
      }
      targetname[++i] = AAS_ValueForBSPEpairKey(ent, "targetname");
      cur_entities[i] = entities;
      continue;
    }
    if ( !strcmp(classname, "func_button") )
      return ent;
    if ( !strcmp(classname, "trigger_multiple") )
      return ent;
    if ( !strcmp(classname, "trigger_once") )
      return ent;
    if ( !strcmp(classname, "func_door_rotating") )
      return ent;
    if ( !strcmp(classname, "trigger_key") )
      return NULL;
    i--;
  }
  botimport.Print(PRT_ERROR, "BotEntityToActivate: unkown activator with classname \"%s\"\n", classname);
  return NULL;
}

/* G_SetMovedir's four direction constants, 12 bytes each, in the original .data
 * order — the same order and values as game/g_utils.c:342-345, from which
 * BotSetMovedir was copied.  Non-static in Mr. Elusive's sources, so all four
 * survive by name in gladi386.so's .dynsym, each a 12-byte OBJECT, confirming
 * float[3] rather than three separate scalars.
 *
 * Defined HERE, directly above BotSetMovedir, as in Q3's ai_dmq3.c: gladi386.so's
 * .dynsym, which lists each global where its TU first mentions it, has all four after
 * BotEntityToActivate and `entities` -- not at the top of the file. */
float VEC_UP[3]       = { 0.0f, -1.0f,  0.0f };
float MOVEDIR_UP[3]   = { 0.0f,  0.0f,  1.0f };
float VEC_DOWN[3]     = { 0.0f, -2.0f,  0.0f };
float MOVEDIR_DOWN[3] = { 0.0f,  0.0f, -1.0f };

// gladiator.dll: 10024FD0..1002504D
// gladi386.so:   0002F540..0002F5E2
void __cdecl BotSetMovedir(float *angles, float *movedir)
{
  if ( VectorCompare(angles, VEC_UP) )
  {
    VectorCopy(MOVEDIR_UP, movedir);
  }
  else if ( VectorCompare(angles, VEC_DOWN) )
  {
    VectorCopy(MOVEDIR_DOWN, movedir);
  }
  else
  {
    AngleVectors(angles, movedir, NULL, NULL);
  }
}

// gladiator.dll: 10025070..1002545E
// gladi386.so:   0002F5E4..0002FBCD
/* Debug visualiser for func_button entities: walk the BSP entity list, filter by
 * classname == "func_button", read the brush's model AABB plus the "angle"/"health"
 * keys, and draw permanent debug crosses — one at the shoot point for shootable
 * buttons, three for touch/use buttons.  Capped at the first 6 matches.
 *
 * The body is Q3 BotFuncButtonActivateGoal's, statement for statement: `lip` is read
 * and defaulted but never used (the .so keeps a dead `fld 0.0; fstp st(0)` from the
 * `if (!lip)` test), and BSPModelMinsMaxs is called with zero-initialised `angles`,
 * so the bbox is the model's local-space AABB rather than a world-rotated one.
 * The vec3 declaration order is the .so's frame read top-down (gcc 2.7 lays out
 * address-taken locals in declaration order); cl.exe is indifferent to it. */
void __cdecl sub_10025070(void)
{
  bsp_entity_t *ent;
  int drawn;
  char *classname;
  char *model_str;
  int modelnum;
  vec3_t mins;
  vec3_t maxs;
  vec3_t size;
  vec3_t origin;
  vec3_t angles;
  vec3_t movedir;
  vec3_t goalorigin;
  vec3_t start;
  vec3_t end;
  vec3_t bboxmins;
  vec3_t bboxmaxs;
  aas_trace_t trace;
  float dist;
  float lip;

  drawn = 0;
  ent   = entities;
  if ( !ent )
    return;

  do
  {
    /* Do NOT remove the two (const char *) casts below, even though they trip
     * -Wdiscarded-qualifiers and are codegen-neutral here: dropping them perturbs
     * MSVC6's whole-TU scheduler enough to flip a tie-break in PC_ReadDefineParms and
     * cost that function its byte-match. */
    classname = (const char *)AAS_ValueForBSPEpairKey(ent, "classname");
    if ( !strcmp(classname, "func_button") )
    {
      model_str = (const char *)AAS_ValueForBSPEpairKey(ent, "model");
      modelnum  = AAS_IndexFromModel(model_str);
      if ( !modelnum )
        modelnum = atoi(model_str + 1);

      VectorClear(angles);
      AAS_BSPModelMinsMaxsOrigin(modelnum - 1, angles, mins, maxs, NULL);

      lip = AAS_FloatForBSPEpairKey(ent, "lip");
      if (!lip) lip = 4;
      VectorSet(angles, 0, AAS_FloatForBSPEpairKey(ent, "angle"), 0);
      BotSetMovedir(angles, movedir);

      VectorSubtract(maxs, mins, size);
      VectorAdd(mins, maxs, origin);
      VectorScale(origin, 0.5f, origin);
      dist = fabs(movedir[0]) * size[0] + fabs(movedir[1]) * size[1] + fabs(movedir[2]) * size[2];
      dist *= 0.5;

      if ( AAS_FloatForBSPEpairKey(ent, "health") )
      {
        VectorMA(origin, -dist, movedir, goalorigin);
        AAS_DrawPermanentCross(goalorigin, 4.0f, LINECOLOR_BLUE);
      }
      else
      {
        int i;

        AAS_PresenceTypeBoundingBox(PRESENCE_CROUCH, bboxmins, bboxmaxs);
        for (i = 0; i < 3; i++)
        {
          if (movedir[i] < 0) dist += fabs(movedir[i]) * fabs(bboxmaxs[i]);
          else dist += fabs(movedir[i]) * fabs(bboxmins[i]);
        }
        VectorMA(origin, -dist, movedir, goalorigin);

        VectorCopy(goalorigin, start);
        start[2] += 24.0f;
        VectorSet(end, start[0], start[1], start[2] - 100.0f);
        trace = AAS_TraceClientBBox(start, end, PRESENCE_CROUCH, -1);
        if ( !trace.startsolid )
          VectorCopy(trace.endpos, goalorigin);
        AAS_DrawPermanentCross(goalorigin, 4.0f, LINECOLOR_YELLOW);

        VectorSubtract(mins, origin, mins);
        VectorSubtract(maxs, origin, maxs);

        VectorAdd(mins, origin, start);
        AAS_DrawPermanentCross(start, 4.0f, LINECOLOR_BLUE);

        VectorAdd(maxs, origin, start);
        AAS_DrawPermanentCross(start, 4.0f, LINECOLOR_BLUE);
      }

      if ( ++drawn > 5 )
        return;
    }
    ent = ent->next; /* next entity (typed bsp_entity_t) */
  }
  while ( ent );
}

/* dkbot: most doors, buttons and health stations need a fresh press of use (Client_Use_f) */
#define DK_USE_REACH       80.0f
#define DK_MASK_USE        0x6000283
#define DK_USE_REPRESS     1.5f       /* a door with spawnflag 0x40 closes again on a second use */
#define DK_SF_DOOR_FIELD   0x10       /* func_door(_rotate): opens when approached */
#define DK_SF_BUTTON_TOUCH 1          /* func_button: fires on touch */
#define DK_MAX_USERS       256
#define DK_DOOR_PATIENCE   4.0f       /* still shut after this long: it needs a key */
#define DK_DOOR_OPENING    3.0f       /* for a pressed door to clear the way */
#define DK_DOOR_RETRY      10.0f      /* activated this recently and still shut: it stays so */
#define DK_DOOR_AVOID      30.0f
#define DK_LIFT_PATIENCE   3.0f       /* a targeted lift still not down waits for its trigger */
#define DK_LIFT_NEAR       500        /* travel time to a lift's button worth the walk */

/* dkbot: the retreat node copies the move's view but never turns to it */
static int dk_retreat_view[DK_MAX_USERS];

static struct {
  int   button;       /* the use button the activate node walks to */
  float nextpress;
  int   door;         /* the door last used or activated, since when, and last blocking */
  float since, last;
  int   hopeless;     /* a door the bot cannot open, until */
  float hopeless_until;
  int   lift;         /* the elevator link waited at, since when, and last */
  float lift_since, lift_last;
} dk_use[DK_MAX_USERS];

static int dk_use_hits(bot_state_t *bs, int entnum)
{
  vec3_t forward, end;
  vec3_t mins = { -8.0f, -8.0f, -8.0f }, maxs = { 8.0f, 8.0f, 8.0f };
  bsp_trace_t trace;

  AngleVectors(bs->viewangles, forward, NULL, NULL);
  VectorMA(bs->eye, DK_USE_REACH, forward, end);
  trace = AAS_Trace(bs->eye, mins, maxs, end, bs->entitynum, DK_MASK_USE);
  return trace.ent == entnum;
}

static bsp_entity_t *dk_bsp_entity(int modelindex);

/* Face the entity, a button from its pressed side, and press once the use trace hits it. */
static int dk_use_entity(bot_state_t *bs, bot_moveresult_t *moveresult, int entnum, int press)
{
  aas_entityinfo_t entinfo = AAS_EntityInfo(entnum);
  bsp_entity_t *ent = dk_bsp_entity(entinfo.modelindex);
  char *classname = ent ? AAS_ValueForBSPEpairKey(ent, "classname") : NULL;
  vec3_t target, dir;
  int i;

  if ( classname && !strcmp(classname, "func_button") )
  {
    vec3_t angles = { 0.0f, 0.0f, 0.0f }, movedir, size;
    float d = 9.0f;

    angles[1] = AAS_FloatForBSPEpairKey(ent, "angle");
    BotSetMovedir(angles, movedir);
    for ( i = 0; i < 3; i++ )
    {
      size[i] = entinfo.maxs[i] - entinfo.mins[i];
      target[i] = entinfo.origin[i] + 0.5f * (entinfo.mins[i] + entinfo.maxs[i]);
      d += 0.5f * fabs(movedir[i]) * size[i];
    }
    VectorMA(target, -d, movedir, target);
    target[2] += 4.0f;
    if ( target[2] < bs->eye[2] - 24.0f )
      EA_Crouch(bs->client);
  }
  else
  {
    for ( i = 0; i < 3; i++ )
    {
      float lo = entinfo.origin[i] + entinfo.mins[i] + 4.0f;
      float hi = entinfo.origin[i] + entinfo.maxs[i] - 4.0f;

      target[i] = bs->eye[i] < lo ? lo : bs->eye[i] > hi ? hi : bs->eye[i];
      if ( lo > hi )
        target[i] = entinfo.origin[i] + 0.5f * (entinfo.mins[i] + entinfo.maxs[i]);
    }
  }
  VectorSubtract(target, bs->eye, dir);
  if ( VectorLength(dir) > DK_USE_REACH )
    return 0;
  Vector2Angles(dir, moveresult->ideal_viewangles);
  moveresult->flags |= 1;
  if ( !press || bs->client < 0 || bs->client >= DK_MAX_USERS || AAS_Time() < dk_use[bs->client].nextpress
    || !dk_use_hits(bs, entnum) )
    return 0;
  EA_Use(bs->client);
  dk_use[bs->client].nextpress = AAS_Time() + DK_USE_REPRESS;
  return 1;
}

static bsp_entity_t *dk_bsp_entity(int modelindex)
{
  char *model = AAS_ModelFromIndex(modelindex), *m;
  bsp_entity_t *ent;

  if ( !model || *model != '*' )
    return NULL;
  for ( ent = entities; ent; ent = ent->next )
  {
    m = AAS_ValueForBSPEpairKey(ent, "model");
    if ( m && !strcmp(m, model) )
      return ent;
  }
  return NULL;
}

static int dk_entity_with_model(int modelindex)
{
  int i;

  for ( i = 1; i < aasworld.numentities; i++ )
  {
    if ( aasworld.entities[i].i.valid && aasworld.entities[i].i.modelindex == modelindex )
      return i;
  }
  return 0;
}

static int dk_door_opens_on_use(bsp_entity_t *ent)
{
  char *classname = AAS_ValueForBSPEpairKey(ent, "classname");

  if ( !classname || AAS_ValueForBSPEpairKey(ent, "targetname") )
    return 0;
  if ( !strcmp(classname, "func_door_secret") )
    return 1;
  if ( strcmp(classname, "func_door") && strcmp(classname, "func_door_rotate") )
    return 0;
  return !AAS_FloatForBSPEpairKey(ent, "health")
      && !(AAS_IntForBSPEpairKey(ent, "spawnflags") & DK_SF_DOOR_FIELD);
}

void DK_AvoidReach(bot_movestate_t *ms, int reachnum);

/* Avoid the link into the door too, or each goal beyond it is found hopeless in turn. */
static void dk_door_hopeless(bot_state_t *bs, int door)
{
  bot_goal_t *goal = (bot_goal_t *)BotGetTopGoal(&bs->goalstate);

  if ( bs->ms.lastreachnum )
    DK_AvoidReach((bot_movestate_t *)&bs->ms, bs->ms.lastreachnum);
  dk_use[bs->client].hopeless = door;
  dk_use[bs->client].hopeless_until = AAS_Time() + DK_DOOR_RETRY;
  if ( goal )
    BotAddToAvoidGoals(&bs->goalstate, goal->number, DK_DOOR_AVOID);
  if ( BotAINode(bs) == AINode_Seek_ActivateEntity )
    bs->activategoal_time = 0;
  bs->nbg_time = 0;
  bs->ltg_time = 0;
}

/* The door, at rest, blocks the bot while it walks to what opens it, or soon after it did. */
static int dk_door_known_shut(bot_state_t *bs, int door)
{
  float since = dk_use[bs->client].since;

  if ( dk_use[bs->client].hopeless == door && AAS_Time() < dk_use[bs->client].hopeless_until )
    return 1;
  if ( BotAINode(bs) == AINode_Seek_ActivateEntity )
    return bs->activategoal.entitynum == door;
  return dk_use[bs->client].door == door
      && AAS_Time() > since + DK_DOOR_OPENING && AAS_Time() < since + DK_DOOR_RETRY;
}

static void dk_door_activated(bot_state_t *bs, int door)
{
  if ( BotAINode(bs) == AINode_Seek_ActivateEntity )
    return;
  dk_use[bs->client].door = door;
  dk_use[bs->client].since = AAS_Time();
}

static int dk_use_door(bot_state_t *bs, bot_moveresult_t *moveresult, aas_entityinfo_t *entinfo)
{
  bsp_entity_t *ent = dk_bsp_entity(entinfo->modelindex);

  if ( !ent || !dk_door_opens_on_use(ent) )
    return 0;
  /* a rotating door off its rest angles is open: a press would swing it shut on the bot */
  if ( !strcmp(AAS_ValueForBSPEpairKey(ent, "classname"), "func_door_rotate")
    && (entinfo->angles[0] || entinfo->angles[1] || entinfo->angles[2]) )
    return 0;
  if ( dk_use[bs->client].door != entinfo->number || AAS_Time() > dk_use[bs->client].last + 1.0f )
  {
    dk_use[bs->client].door = entinfo->number;
    dk_use[bs->client].since = AAS_Time();
  }
  dk_use[bs->client].last = AAS_Time();
  if ( !VectorCompare(entinfo->origin, entinfo->lastvisorigin) )
    return 1;
  if ( AAS_Time() > dk_use[bs->client].since + DK_DOOR_PATIENCE )
  {
    dk_door_hopeless(bs, entinfo->number);
    return 1;
  }
  dk_use_entity(bs, moveresult, entinfo->number, 1);
  return 1;
}

/* A func_event_generator's keys name what it fires; with spawnflag 2 it is a touch field. */
#define DK_SF_GENERATOR_TOUCH  0x0002

static int dk_fires(bsp_entity_t *ent, const char *classname, const char *targetname)
{
  char *target = AAS_ValueForBSPEpairKey(ent, "target");

  if ( target && !strcmp(target, targetname) )
    return 1;
  return !strcmp(classname, "func_event_generator") && AAS_ValueForBSPEpairKey(ent, (char *)targetname);
}

static int dk_relays(bsp_entity_t *ent, const char *classname)
{
  return !strcmp(classname, "trigger_relay") || !strcmp(classname, "trigger_counter")
      || (!strcmp(classname, "func_event_generator")
          && !(AAS_IntForBSPEpairKey(ent, "spawnflags") & DK_SF_GENERATOR_TOUCH));
}

static int dk_activates(bsp_entity_t *ent, const char *classname)
{
  return !strcmp(classname, "func_button") || !strcmp(classname, "trigger_multiple")
      || !strcmp(classname, "trigger_once") || !strcmp(classname, "func_event_generator");
}

/* Of the door's activators, through relays, the first on the bot's side of it. */
static bsp_entity_t *dk_activator_r(bsp_entity_t *door, const char *targetname, const vec3_t mins,
                                    const vec3_t maxs, const vec3_t origin, int depth)
{
  bsp_entity_t *ent, *found;
  char *classname, *model;
  vec3_t emins, emaxs, angles = { 0.0f, 0.0f, 0.0f };
  int axis, modelnum;
  float mid;

  if ( !targetname || depth > 4 )
    return NULL;
  axis = maxs[0] - mins[0] < maxs[1] - mins[1] ? 0 : 1;
  mid = 0.5f * (mins[axis] + maxs[axis]);
  for ( ent = entities; ent; ent = ent->next )
  {
    classname = AAS_ValueForBSPEpairKey(ent, "classname");
    if ( ent == door || !classname || !dk_fires(ent, classname, targetname) )
      continue;
    if ( dk_relays(ent, classname) )
    {
      found = dk_activator_r(door, AAS_ValueForBSPEpairKey(ent, "targetname"), mins, maxs, origin,
                             depth + 1);
      if ( found )
        return found;
      continue;
    }
    if ( !dk_activates(ent, classname) )
      continue;
    model = AAS_ValueForBSPEpairKey(ent, "model");
    modelnum = model ? AAS_IndexFromModel(model) : 0;
    if ( !modelnum && model && *model == '*' )
      modelnum = atoi(model + 1);
    if ( !modelnum )
      continue;
    AAS_BSPModelMinsMaxsOrigin(modelnum - 1, angles, emins, emaxs, NULL);
    if ( (0.5f * (emins[axis] + emaxs[axis]) - mid) * (origin[axis] - mid) > 0.0f )
      return ent;
  }
  return NULL;
}

/* Of a lift's activators, through relays, the one the bot reaches first. */
static bsp_entity_t *dk_lift_activator_r(bot_state_t *bs, const char *targetname, int depth, int *besttime)
{
  bsp_entity_t *ent, *found, *best = NULL;
  char *classname, *model;
  vec3_t mins, maxs, centre, goalorigin, angles = { 0.0f, 0.0f, 0.0f };
  int modelnum, area, t;

  if ( !targetname || depth > 4 )
    return NULL;
  for ( ent = entities; ent; ent = ent->next )
  {
    classname = AAS_ValueForBSPEpairKey(ent, "classname");
    if ( !classname || !dk_fires(ent, classname, targetname) )
      continue;
    if ( dk_relays(ent, classname) )
    {
      found = dk_lift_activator_r(bs, AAS_ValueForBSPEpairKey(ent, "targetname"), depth + 1, besttime);
      if ( found )
        best = found;
      continue;
    }
    if ( !dk_activates(ent, classname) )
      continue;
    model = AAS_ValueForBSPEpairKey(ent, "model");
    modelnum = model ? AAS_IndexFromModel(model) : 0;
    if ( !modelnum && model && *model == '*' )
      modelnum = atoi(model + 1);
    if ( !modelnum )
      continue;
    AAS_BSPModelMinsMaxsOrigin(modelnum - 1, angles, mins, maxs, NULL);
    VectorAdd(mins, maxs, centre);
    VectorScale(centre, 0.5f, centre);
    VectorSubtract(mins, centre, mins);
    VectorSubtract(maxs, centre, maxs);
    area = AAS_BestReachableArea(centre, mins, maxs, goalorigin);
    if ( !area )
      continue;
    /* without lifts: the bot does not ride one to reach its button */
    t = area == bs->ms.areanum ? 1
        : AAS_AreaTravelTimeToGoalArea(bs->ms.areanum, area, 102334 & ~AAS_TravelFlagForType(11));
    if ( t && (!*besttime || t < *besttime) )
    {
      *besttime = t;
      best = ent;
    }
  }
  return best;
}

static bsp_entity_t *dk_activator(bot_state_t *bs, int entitynum, bsp_entity_t *upstream)
{
  aas_entityinfo_t entinfo = AAS_EntityInfo(entitynum);
  bsp_entity_t *door = dk_bsp_entity(entinfo.modelindex), *mine;
  vec3_t mins, maxs;
  char *classname = door ? AAS_ValueForBSPEpairKey(door, "classname") : NULL;
  int besttime = 0;

  if ( classname && !strcmp(classname, "func_plat") )
  {
    mine = dk_lift_activator_r(bs, AAS_ValueForBSPEpairKey(door, "targetname"), 0, &besttime);
    return mine ? mine : upstream;
  }
  if ( !door || !upstream || upstream == door )
    return upstream;
  VectorAdd(entinfo.origin, entinfo.mins, mins);
  VectorAdd(entinfo.origin, entinfo.maxs, maxs);
  mine = dk_activator_r(door, AAS_ValueForBSPEpairKey(door, "targetname"), mins, maxs, bs->origin, 0);
  return mine ? mine : upstream;
}

/* In reach, press the button, which ends the activate node. */
static int dk_use_button(bot_state_t *bs, bot_moveresult_t *moveresult)
{
  int c = bs->client;

  if ( c < 0 || c >= DK_MAX_USERS || !dk_use[c].button
    || BotAINode(bs) != AINode_Seek_ActivateEntity || AAS_Time() > bs->activategoal_time )
    return 0;
  if ( !dk_use_entity(bs, moveresult, dk_use[c].button, 1) )
    return (moveresult->flags & 1) != 0;
  dk_use[c].button = 0;
  dk_use[c].since = AAS_Time();
  bs->activategoal_time = 0;
  return 1;
}
/* end dkbot */

// gladiator.dll: 10025560..1002600F
// gladi386.so:   0002FBD0..000308CB
/* Genuinely void, as in Q3: each exit path just leaves whatever is in eax and
 * every caller ignores it. */
void __cdecl BotAIBlocked(bot_state_t *bs, bot_moveresult_t *moveresult, int activate)
{
  int i, modelnum;
  char *classname, *model;
  float dist, lip;
  bsp_entity_t *ent;
  vec3_t mins, maxs, triggerorigin, start, end, angles, movedir, hordir, sideward;
  vec3_t size, origin, goalorigin, bboxmins, bboxmaxs;
  vec3_t up = {0, 0, 1};
  vec3_t extramins = {-5, -5, -5}, extramaxs = {5, 5, 5};
  aas_entityinfo_t entinfo;
  aas_trace_t trace;
  bsp_trace_t bsptrace;  /* Q3's declaration, unused here; gcc 2.7 still gives it its frame slot */

  if ( bs->client >= 0 && bs->client < DK_MAX_USERS )  /* dkbot */
    dk_retreat_view[bs->client] = BotAINode(bs) == AINode_Battle_Retreat && (moveresult->flags & 1);
  if ( activate && dk_use_button(bs, moveresult) )  /* dkbot */
    return;
  /* dkbot: a targeted func_plat waits for its trigger: walk there or avoid the plat */
  if ( (moveresult->flags & 4) && !moveresult->blocked && bs->ms.lastreachnum
    && bs->client >= 0 && bs->client < DK_MAX_USERS )
  {
    aas_reachability_t reach = AAS_ReachabilityFromNum(bs->ms.lastreachnum);
    bsp_entity_t *lift = reach.traveltype == 11 ? dk_bsp_entity(reach.facenum + 1) : NULL;
    char *name = lift ? AAS_ValueForBSPEpairKey(lift, "targetname") : NULL;
    int besttime = 0;

    if ( dk_use[bs->client].lift != bs->ms.lastreachnum || AAS_Time() > dk_use[bs->client].lift_last + 1.0f )
    {
      dk_use[bs->client].lift = bs->ms.lastreachnum;
      dk_use[bs->client].lift_since = AAS_Time();
    }
    dk_use[bs->client].lift_last = AAS_Time();
    if ( name && AAS_Time() > dk_use[bs->client].lift_since + DK_LIFT_PATIENCE )
    {
      DK_AvoidReach((bot_movestate_t *)&bs->ms, bs->ms.lastreachnum);
      if ( activate && dk_lift_activator_r(bs, name, 0, &besttime) && besttime < DK_LIFT_NEAR )
      {
        moveresult->blocked = 1;
        moveresult->blockentity = dk_entity_with_model(reach.facenum + 1);
      }
    }
  }
  if ( !moveresult->blocked )
    return;
  entinfo = AAS_EntityInfo(moveresult->blockentity);
  /* dkbot: battle nodes heading for a goal open a use door in the way too */
  if ( entinfo.solid == 3 && !activate && bs->client >= 0 && bs->client < DK_MAX_USERS
    && (BotAINode(bs) == AINode_Battle_Chase || BotAINode(bs) == AINode_Battle_Retreat
     || BotAINode(bs) == AINode_Battle_NBG) && dk_use_door(bs, moveresult, &entinfo) )
  {
    dk_retreat_view[bs->client] = BotAINode(bs) == AINode_Battle_Retreat && (moveresult->flags & 1);
    return;
  }
  if ( entinfo.solid == 3 && activate && bs->client >= 0 && bs->client < DK_MAX_USERS )  /* dkbot */
  {
    if ( dk_use_door(bs, moveresult, &entinfo) )
      return;
    if ( !VectorCompare(entinfo.origin, entinfo.lastvisorigin) )
      return;                           /* on the move: wait for it */
    if ( dk_door_known_shut(bs, entinfo.number) )
    {
      dk_door_hopeless(bs, entinfo.number);
      return;
    }
  }
  if ( entinfo.solid == 3 && activate )
  {
    ent = dk_activator(bs, entinfo.number, BotEntityToActivate(entinfo.number));  /* dkbot */
    if ( !ent )
      classname = "";
    else
      classname = AAS_ValueForBSPEpairKey(ent, "classname");
    /* dkbot: only a door with health opens when shot */
    if ( (!strcmp(classname, "func_door_secret") || !strcmp(classname, "func_door"))
      && AAS_FloatForBSPEpairKey(ent, "health") )
    {
      model = AAS_ValueForBSPEpairKey(ent, "model");
      modelnum = AAS_IndexFromModel(model);
      if ( !modelnum )
        return;
      VectorClear(angles);
      AAS_BSPModelMinsMaxsOrigin(modelnum - 1, angles, mins, maxs, NULL);
      VectorAdd(maxs, mins, goalorigin);
      VectorScale(goalorigin, 0.5, goalorigin);
      VectorSubtract(goalorigin, bs->origin, movedir);
      Vector2Angles(movedir, moveresult->ideal_viewangles);
      moveresult->flags |= MOVERESULT_MOVEMENTVIEW;
      EA_UseItem(bs->client, "Blaster");
      EA_Attack(bs->client);
      return;
    }
    if ( !strcmp(classname, "func_button") )
    {
      model = AAS_ValueForBSPEpairKey(ent, "model");
      modelnum = AAS_IndexFromModel(model);
      if ( !modelnum )
        return;
      VectorClear(angles);
      AAS_BSPModelMinsMaxsOrigin(modelnum - 1, angles, mins, maxs, NULL);
      lip = AAS_FloatForBSPEpairKey(ent, "lip");
      if (!lip) lip = 4;
      VectorSet(angles, 0, AAS_FloatForBSPEpairKey(ent, "angle"), 0);
      BotSetMovedir(angles, movedir);
      VectorSubtract(maxs, mins, size);
      VectorAdd(mins, maxs, origin);
      VectorScale(origin, 0.5, origin);
      dist = fabs(movedir[0]) * size[0] + fabs(movedir[1]) * size[1] + fabs(movedir[2]) * size[2];
      dist *= 0.5;
      if ( AAS_FloatForBSPEpairKey(ent, "health") )
      {
        VectorMA(origin, -dist, movedir, goalorigin);
        /* dkbot: shoot it only from where a shot reaches it */
        if ( bs->client >= 0 && bs->client < DK_MAX_USERS )
        {
          bsptrace = AAS_Trace(bs->eye, NULL, NULL, origin, bs->entitynum, DK_MASK_PROJECTILE);
          if ( bsptrace.fraction < 1.0f && bsptrace.ent != dk_entity_with_model(modelnum) )
          {
            dk_door_hopeless(bs, entinfo.number);
            return;
          }
        }
        VectorSubtract(goalorigin, bs->origin, movedir);
        Vector2Angles(movedir, moveresult->ideal_viewangles);
        moveresult->flags |= MOVERESULT_MOVEMENTVIEW;
        EA_UseItem(bs->client, "Blaster");
        EA_Attack(bs->client);
        return;
      }
      AAS_PresenceTypeBoundingBox(PRESENCE_CROUCH, bboxmins, bboxmaxs);
      for (i = 0; i < 3; i++)
      {
        if (movedir[i] < 0) dist += fabs(movedir[i]) * fabs(bboxmaxs[i]);
        else dist += fabs(movedir[i]) * fabs(bboxmins[i]);
      }
      VectorMA(origin, -dist, movedir, goalorigin);
      VectorCopy(goalorigin, start);
      start[2] += 24;
      VectorSet(end, start[0], start[1], start[2] - 100);
      trace = AAS_TraceClientBBox(start, end, PRESENCE_CROUCH, -1);
      if ( !trace.startsolid )
        VectorCopy(trace.endpos, goalorigin);
      VectorCopy(origin, bs->activategoal.origin);
      bs->activategoal.areanum = AAS_PointAreaNum(goalorigin);
      VectorSubtract(mins, origin, bs->activategoal.mins);
      VectorSubtract(maxs, origin, bs->activategoal.maxs);
      VectorAdd(bs->activategoal.mins, extramins, bs->activategoal.mins);
      VectorAdd(bs->activategoal.maxs, extramaxs, bs->activategoal.maxs);
      bs->activategoal.entitynum = entinfo.number;
      bs->activategoal.number = 0;
      bs->activategoal.flags = 0;
      bs->activategoal_time = AAS_Time() + 10;
      /* dkbot: a button without the touch flag needs use */
      if ( bs->client >= 0 && bs->client < DK_MAX_USERS )
        dk_use[bs->client].button = AAS_IntForBSPEpairKey(ent, "spawnflags") & DK_SF_BUTTON_TOUCH
                                    ? 0 : dk_entity_with_model(modelnum);
      if ( !AAS_AreaReachability(bs->activategoal.areanum) )
      {
        if ( BotAINode(bs) == AINode_Seek_NBG )
          bs->nbg_time = 0;
        else if ( BotAINode(bs) == AINode_Seek_LTG )
          bs->ltg_time = 0;
      }
      else
      {
        dk_door_activated(bs, entinfo.number);  /* dkbot */
        AIEnter_Seek_ActivateEntity(bs);
      }
      return;
    }
    if ( !strcmp(classname, "trigger_multiple") || !strcmp(classname, "trigger_once") )
    {
      model = AAS_ValueForBSPEpairKey(ent, "model");
      modelnum = AAS_IndexFromModel(model);
      if ( !modelnum )
        modelnum = atoi(model + 1);
      VectorClear(angles);
      AAS_BSPModelMinsMaxsOrigin(modelnum - 1, angles, mins, maxs, NULL);
      VectorAdd(mins, maxs, triggerorigin);
      VectorScale(triggerorigin, 0.5, triggerorigin);
      VectorCopy(triggerorigin, start);
      start[2] = maxs[2] + 24;
      VectorSet(end, start[0], start[1], start[2] - 100);
      trace = AAS_TraceClientBBox(start, end, PRESENCE_CROUCH, -1);
      if ( !trace.startsolid )
      {
        VectorCopy(trace.endpos, goalorigin);
        VectorCopy(triggerorigin, bs->activategoal.origin);
        bs->activategoal.areanum = AAS_PointAreaNum(goalorigin);
        VectorSubtract(mins, triggerorigin, bs->activategoal.mins);
        VectorSubtract(maxs, triggerorigin, bs->activategoal.maxs);
        bs->activategoal.entitynum = entinfo.number;
        bs->activategoal.number = 0;
        bs->activategoal.flags = 0;
        bs->activategoal_time = AAS_Time() + 10;
        /* if/else, not an early return: with the return, cl.exe keeps the
         * button path's copy of the shared `ltg_time` tail and cross-jumps
         * this one into it -- the DLL keeps this one.  (The button path is
         * byte-neutral either way and is written the same.) */
        if ( !AAS_AreaReachability(bs->activategoal.areanum) )
        {
          if ( BotAINode(bs) == AINode_Seek_NBG )
            bs->nbg_time = 0;
          else if ( BotAINode(bs) == AINode_Seek_LTG )
            bs->ltg_time = 0;
        }
        else
        {
          dk_door_activated(bs, entinfo.number);  /* dkbot */
          AIEnter_Seek_ActivateEntity(bs);
        }
      }
      return;
    }
  }
  hordir[0] = moveresult->movedir[0];
  hordir[1] = moveresult->movedir[1];
  hordir[2] = 0;
  VectorNormalize(hordir);
  VectorCopy(bs->origin, start);
  start[2] += libvar_sv_step->value;
  VectorMA(start, 5, hordir, end);
  VectorSet(mins, -16, -16, -24);
  VectorSet(maxs, 16, 16, 4);
  CrossProduct(hordir, up, sideward);
  if ( bs->flags & BFL_AVOIDRIGHT )
  {
    sideward[0] = -sideward[0];
    sideward[1] = -sideward[1];
    sideward[2] = -sideward[2];
  }
  if ( !BotMoveInDirection((bot_movestate_t *)&bs->ms, sideward, 400, MOVE_WALK) )
  {
    bs->flags ^= BFL_AVOIDRIGHT;
    sideward[0] = -sideward[0];
    sideward[1] = -sideward[1];
    sideward[2] = -sideward[2];
    BotMoveInDirection((bot_movestate_t *)&bs->ms, sideward, 400, MOVE_WALK);
  }
  if ( BotAINode(bs) == AINode_Seek_NBG )
    bs->nbg_time = 0;
  else if ( BotAINode(bs) == AINode_Seek_LTG )
    bs->ltg_time = 0;
}

// gladiator.dll: 100262C0..1002638D
// gladi386.so:   000308CC..000309C2
void __cdecl sub_100262C0(bot_state_t *bs, bot_goal_t *a2)
{
  aas_entityinfo_t info; // [esp+8h] [ebp-7Ch] BYREF

  if ( ctf->value != 0.0f )
  {
    if ( a2->entitynum )
    {
      info = AAS_EntityInfo(a2->entitynum);
      if ( (info.modelindex == modelindex_tech1 || info.modelindex == modelindex_tech2 || info.modelindex == modelindex_tech3 || info.modelindex == modelindex_tech4)
        && (bs->inventory[INVENTORY_TECH1] > 0 && info.modelindex != modelindex_tech1
         || bs->inventory[INVENTORY_TECH2] > 0 && info.modelindex != modelindex_tech2
         || bs->inventory[INVENTORY_TECH3] > 0 && info.modelindex != modelindex_tech3
#if GLAD_SERVERFIX /* GLAD_SERVERFIX(ctf-tech4-compares-tech3) */
         || bs->inventory[INVENTORY_TECH4] > 0 && info.modelindex != modelindex_tech4) )
#else
         /* The original's: the fourth test compares the goal with tech 3, not 4. */
         || bs->inventory[INVENTORY_TECH4] > 0 && info.modelindex != modelindex_tech3) )
#endif
      {
        EA_DropItem(bs->client, "tech");
      }
    }
  }
}

// gladiator.dll: 100263D0..10026414
// gladi386.so:   000309C4..00030A41
void __cdecl BotCTFRetreatGoals(bot_state_t *bs)
{

  float v1; // st7

  if ( BotCTFCarryingFlag(bs) )
  {
    if ( bs->ltgtype != 5 )
    {
      bs->ltgtype = 5;
      v1 = AAS_Time();
      bs->teamgoal_time = v1 + 120.0f;
      *(int *)&bs->rushbaseaway_time = 0;
    }
  }
}

// gladiator.dll: 10026440..10026604
// gladi386.so:   00030A44..00030CBF
void __cdecl BotCTFSeekGoals(bot_state_t *bs)
{

  int v3; // eax
  float rnd;

  if ( BotCTFCarryingFlag(bs) )
  {
    if ( bs->ltgtype != 5 )
    {
      bs->ltgtype = 5;
      bs->teamgoal_time = AAS_Time() + 120.0f;
      *(int *)&bs->rushbaseaway_time = 0;
    }
  }
  else if ( AAS_Time() >= bs->ctfroam_time )
  {
    v3 = bs->ltgtype;
    if ( v3 != 1 && v3 != 2 && v3 != 3 && v3 != 4 && v3 != 5 && v3 != 6 && v3 != 7 && BotAggression(bs) >= 50.0f )
    {
      bs->teammessage_time = AAS_Time() + 2 * random();
      /* Q3's `rnd = random();` into a float.  IDA's `(rand() & 0x7FFF) *
       * 0.0000305185f` rounded to a different float (0x380000FD, not 1/32767's
       * 0x38000100); the correct constant written that way lets gcc CSE it with
       * the random() above, which the .so does not do. */
      rnd = random();
      if ( rnd < 0.33 && ctf_flag1.areanum && ctf_flag2.areanum )
      {
        bs->ltgtype = 4;
        bs->teamgoal_time = AAS_Time() + 180.0f;
      }
      else if ( rnd < 0.66 && ctf_flag1.areanum && ctf_flag2.areanum )
      {
        if ( BotCTFTeam(bs) == 1 )
          memcpy(&bs->teamgoal, &ctf_flag1, 0x38u);
        else
          memcpy(&bs->teamgoal, &ctf_flag2, 0x38u);
        bs->ltgtype = 3;
        bs->teamgoal_time = AAS_Time() + 120.0f;
        *(int *)&bs->defendaway_time = 0;
      }
      else
      {
        bs->ltgtype = 0;
        bs->ctfroam_time = AAS_Time() + 60.0f;
      }
    }
  }
}

// gladiator.dll: 10026690..100266D6
// gladi386.so:   00030CC0..00030D42
BOOL TeamPlayIsOn()
{
  return ((int)dmflags->value & (DF_MODELTEAMS | DF_SKINTEAMS)) != 0
      || ctf->value != 0.0f
      || teamplay->value != 0.0f;
}

// gladiator.dll: 10026700..10026746
// gladi386.so:   00030D44..00030D92
BOOL __cdecl BotGetItemTeamGoal(char *goalname, bot_goal_t *goal)
{
  int i;

  if ( !strlen(goalname) )
    return 0;
  i = -1;
  /* Keep the do/while: the original carries the second iteration path even
   * though a positive first result returns immediately. */
  do
  {
    i = BotGetLevelItemGoal(i, goalname, goal);
    if ( i > 0 )
      return 1;
  }
  while ( i > 0 );
  return 0;
}

// gladiator.dll: 10026770..100267BF
// gladi386.so:   00030D94..00030E29
int __cdecl BotGetMessageTeamGoal(bot_state_t *bs, char *goalname, bot_goal_t *goal)
{
  bot_waypoint_t *cp; // eax

  if ( BotGetItemTeamGoal(goalname, goal) )
    return 1;
  cp = BotFindWayPoint(BotCheckpoints(bs), goalname);
  if ( cp )
  {
    memcpy(goal, &cp->goal, sizeof(bot_goal_t));
    return 1;
  }
  return 0;
}

// gladiator.dll: 100267E0..1002689B
// gladi386.so:   00030E2C..00030EF0
float __cdecl BotGetTime(bot_match_t *match)
{
  float v1; // st7
  float t; // [esp+0h] [ebp-18Ch]
  /* One bot_match_t (240 B), filled by BotFindMatch — see chat_state.h. */
  bot_match_t timematch; // [esp+9Ch] [ebp-F0h] BYREF
  char timestring[152]; // [esp+4h] [ebp-188h] BYREF

  if ( (match->subtype & ST_TIME) != 0 )
  {
    BotMatchVariable(match, TIME, timestring);
    if ( BotFindMatch(timestring, &timematch, MTCONTEXT_TIME) )
    {
      BotMatchVariable(&timematch, TIME, timestring);
      if ( timematch.type == MSG_MINUTES )
      {
        v1 = atof(timestring) * 60.0;
        t = v1;
      }
      else if ( timematch.type == MSG_SECONDS )
      {
        v1 = atof(timestring);
        t = v1;
      }
      if ( t > 0.0f )
        return AAS_Time() + t;
    }
  }
  return 0.0f;
}

// gladiator.dll: 100268D0..10026953
// gladi386.so:   00030EF0..00031037
int __cdecl FindClientByName(char *name)
{
  int i;

  for ( i = 0; i < botlibglobals.num_clients; i++ )
  {
    if ( !_strcmpi(clientsettings[i].netname, name) )
      return i;
  }
  for ( i = 0; i < botlibglobals.num_clients; i++ )
  {
    if ( stristr(clientsettings[i].netname, name) )
      return i;
  }
  return -1;
}

// gladiator.dll: 10026990..10026B5F
// gladi386.so:   00031038..0003139B
int __cdecl BotGetPatrolWaypoints(bot_state_t *bs, bot_match_t *match)
{
  int patrolflags; // edi (patrol flags accumulator)
  bot_waypoint_t *newpatrolpoints; // esi (head of new patrol list)
  bot_waypoint_t *newwp; // eax (newly-allocated node)
  bot_waypoint_t *wp; // ecx (tail walker)
  char Destination[152]; // [esp+48h] [ebp-188h] BYREF
  /* One bot_match_t (240 B), filled by BotFindMatch — see chat_state.h. */
  bot_match_t keyareamatch; // [esp+E0h] [ebp-F0h] BYREF
  bot_goal_t goal; // [esp+10h] [ebp-1C0h] BYREF — parsed goal for current keypoint name

  newpatrolpoints = NULL;
  patrolflags = 0;
  BotMatchVariable(match, KEYAREA, Destination);
  while ( 1 )
  {
    if ( !BotFindMatch(Destination, &keyareamatch, MTCONTEXT_PATROLKEYAREA) )
    {
      EA_SayTeam(bs->client, "what do you say?");
      BotFreeWaypoints(newpatrolpoints);
      BotPatrolpoints(bs) = NULL;
      return 0;
    }
    BotMatchVariable(&keyareamatch, KEYAREA, Destination);
    if ( !BotGetMessageTeamGoal(bs, Destination, &goal) )
    {
      BotInitialChat(&bs->chatstate, "cannotfind", Destination, (char *)0);
      BotEnterChat(&bs->chatstate, bs->client, CHAT_TEAM);
      BotFreeWaypoints(newpatrolpoints);
      BotPatrolpoints(bs) = NULL;
      return 0;
    }
    /* Thunk 0x10001401 goes to BotCreateWayPoint, not BotResetState. */
    newwp = BotCreateWayPoint(Destination, goal.origin, goal.areanum);
    newwp->next = NULL;
    for ( wp = newpatrolpoints; wp && wp->next; wp = wp->next )
      ;
    if ( !wp )
    {
      newpatrolpoints = newwp;
      newwp->prev = NULL;
    }
    else
    {
      wp->next = newwp;
      newwp->prev = wp;
    }
    if ( (keyareamatch.subtype & ST_BACK) != 0 )
    {
      patrolflags = 1;
      break;
    }
    else if ( (keyareamatch.subtype & ST_REVERSE) != 0 )
    {
      patrolflags = 2;
      break;
    }
    else if ( (keyareamatch.subtype & ST_MORE) != 0 )
    {
      BotMatchVariable(&keyareamatch, MORE, Destination);
    }
    else
    {
      break;
    }
  }
  if ( !newpatrolpoints || !newpatrolpoints->next )
  {
    EA_SayTeam(bs->client, "I need more key points to patrol\n");
    BotFreeWaypoints(newpatrolpoints);
    return 0;
  }
  else
  {
    BotFreeWaypoints(BotPatrolpoints(bs));
    BotPatrolpoints(bs) = newpatrolpoints;
    BotCurPatrolPoint(bs) = newpatrolpoints;
    bs->patrolflags = patrolflags;
    return 1;
  }
}

// gladiator.dll: 10026BE0..10026DBC
// gladi386.so:   0003139C..000315C2
int __cdecl BotAddressedToBot(bot_state_t *bs, bot_match_t *match)
{
  int client; // eax
  int result; // eax
  const char *botname; // esi (Q2 ClientName returns char*)
  float v5; // [esp+Ch] [ebp-2BCh]
  char addressedto[152]; // [esp+A8h] [ebp-220h] BYREF
  char netname[152]; // [esp+230h] [ebp-98h] BYREF
  char name[152]; // [esp+10h] [ebp-2B8h] BYREF
  /* One bot_match_t (240 B), filled by BotFindMatch — see chat_state.h. */
  bot_match_t addresseematch; // [esp+140h] [ebp-188h] BYREF

  BotMatchVariable(match, NETNAME, netname);
  client = ClientFromName(netname);
  if ( client < 0 )
    return 0;
  result = BotSameTeam(bs, client + 1);
  if ( !result )
    return result;
  if ( (match->subtype & ST_ADDRESSED) != 0 )
  {
    BotMatchVariable(match, ADDRESSEE, addressedto);
    botname = ClientName(bs->client);
    while ( BotFindMatch(addressedto, &addresseematch, MTCONTEXT_ADDRESSEE) )
    {
      if ( addresseematch.type == MSG_EVERYONE )
      {
        return 1;
      }
      if ( addresseematch.type == MSG_MULTIPLENAMES )
      {
        BotMatchVariable(&addresseematch, TEAMMATE, name);
        if ( StringContains(botname, name, 0) )
          return 1;
        if ( StringContains(bs->teamleader, name, 0) )
          return 1;
        BotMatchVariable(&addresseematch, MORE, addressedto);
      }
      else
      {
        BotMatchVariable(&addresseematch, TEAMMATE, name);
        if ( StringContains(botname, name, 0) )
          return 1;
        if ( StringContains(bs->teamleader, name, 0) )
          return 1;
        break;
      }
    }
    return 0;
  }
  else
  {
    v5 = random();
    if ( 1.0f / (float)(BotNumTeamMates(bs) - 1) < v5 )
      return 0;
  }
  return 1;
}

// gladiator.dll: 10026E40..10026ED9
// gladi386.so:   000315C4..000316B2
// Q3's BotGPSToPosition.  DEAD in Gladiator — /INCREMENTAL.
int __cdecl BotGPSToPosition(char *buf, float *position)
{
  int i;
  int j = 0;
  int num;
  int sign;

  for ( i = 0; i < 3; i++ )
  {
    num = 0;
    while ( buf[j] == ' ' )
      j++;
    if ( buf[j] == '-' )
    {
      j++;
      sign = -1;
    }
    else
    {
      sign = 1;
    }
    while ( buf[j] )
    {
      if ( buf[j] >= '0' && buf[j] <= '9' )
      {
        num = num * 10 + buf[j] - '0';
        j++;
      }
      else
      {
        j++;
        break;
      }
    }
    botimport.Print(PRT_MESSAGE, "%d\n", sign * num);
    position[i] = (float)sign * num;
  }
  return 1;
}

// gladiator.dll: 10026F10..10028198
// gladi386.so:   000316B4..00032D6B
int __cdecl BotMatchMessage(bot_state_t *bs, char *message)
{
  /* One function in Gladiator; Q3's ai_cmd.c splits it into the BotMatch_*
   * handlers, whose statements and local names it keeps.  The locals are
   * shared by the cases: gcc 2.7 frees a block's arrays at block exit, so four
   * coexisting message buffers mean one scope, and the .so spills `client`,
   * `areanum` and `other` to one slot each in every case that uses them --
   * which folds IDA's per-case temps (v4/v18, v6/v27/v41, v5 and its stack
   * copy v54) into three variables.  The arrays are in the .so's top-down slot
   * order.  Case labels are the MSG_ values of pak7's match.h. */
  int v3; // eax
  int client, areanum, other;
  float space;
  int v49; // eax
  int v50; // eax
  bot_waypoint_t *cp;
  bot_match_t match;
  bot_match_t teammatematch;
  vec3_t position;
  char buf[MAX_MESSAGE_SIZE];
  char itemname[MAX_MESSAGE_SIZE];
  char netname[MAX_MESSAGE_SIZE];
  char teammate[MAX_MESSAGE_SIZE];
  aas_entityinfo_t entinfo;

  match.type = 0;
  if ( !BotFindMatch(message, &match, MTCONTEXT_CLIENTOBITUARY|MTCONTEXT_ENTERGAME|MTCONTEXT_INITIALTEAMCHAT) )
    return 0;
  switch ( match.type )
  {
    case MSG_DEATH:
      BotMatchVariable(&match, VICTIM, buf);
      v3 = ClientFromName(buf);
      if ( v3 == bs->client )
      {
        bs->botdeathtype = match.subtype;
        break;
      }
      else
      {
        if ( v3 + 1 != bs->enemy )
          break;
        bs->enemydeathtype = match.subtype;
        bs->killedenemy_time = AAS_Time();
        break;
      }
    case MSG_ENTERGAME:
    case MSG_DOFORMATION:
      break;
    case MSG_HELP:
    case MSG_ACCOMPANY:
      if ( !TeamPlayIsOn() || !BotAddressedToBot(bs, &match) )
        break;
      BotMatchVariable(&match, TEAMMATE, teammate);
      if ( BotFindMatch(teammate, &teammatematch, MTCONTEXT_TEAMMATE) && teammatematch.type == MSG_ME )
      {
        BotMatchVariable(&match, NETNAME, netname);
        client = ClientFromName(netname) + 1;
        other = 0;
      }
      else
      {
        client = FindClientByName(teammate) + 1;
        if ( client == bs->entitynum )
          break;
        other = 1;
      }
      if ( !client )
      {
        if ( other )
          BotInitialChat(&bs->chatstate, "whois", teammate, (char *)0);
        else
          BotInitialChat(&bs->chatstate, "whois", netname, (char *)0);
        BotEnterChat(&bs->chatstate, bs->client, CHAT_TEAM);
        break;
      }
      bs->teamgoal.entitynum = 0;
      entinfo = AAS_EntityInfo(client);
      if ( entinfo.valid )
      {
        areanum = AAS_PointAreaNum(entinfo.origin);
        if ( areanum )
        {
          if ( AAS_AreaReachability(areanum) )
          {
            bs->teamgoal.entitynum = client;
            bs->teamgoal.areanum = areanum;
            VectorCopy(entinfo.origin, bs->teamgoal.origin);
            VectorSet(bs->teamgoal.mins, -8, -8, -8);
            VectorSet(bs->teamgoal.maxs, 8, 8, 8);
          }
        }
      }
      if ( !bs->teamgoal.entitynum )
      {
        if ( match.subtype & ST_NEARITEM )
        {
          BotMatchVariable(&match, ITEM, itemname);
          if ( !BotGetMessageTeamGoal(bs, itemname, &bs->teamgoal) )
          {
            BotInitialChat(&bs->chatstate, "cannotfind", itemname, (char *)0);
            BotEnterChat(&bs->chatstate, bs->client, CHAT_TEAM);
            break;
          }
        }
      }
      if ( !bs->teamgoal.entitynum )
      {
        if ( other )
          BotInitialChat(&bs->chatstate, "whereis", teammate, (char *)0);
        else
          BotInitialChat(&bs->chatstate, "whereareyou", netname,
                         (char *)0);
        BotEnterChat(&bs->chatstate, bs->client, CHAT_TEAM);
        break;
      }
      bs->teammate = client;
      bs->teammatevisible_time = AAS_Time();
      bs->teammessage_time = AAS_Time() + 2 * random();
      bs->teamgoal_time = BotGetTime(&match);
      if ( match.type == MSG_HELP )
      {
        bs->ltgtype = 1;
        if ( !bs->teamgoal_time )
          bs->teamgoal_time = AAS_Time() + 60;
      }
      else
      {
        bs->ltgtype = 2;
        if ( !bs->teamgoal_time )
          bs->teamgoal_time = AAS_Time() + 240;
        bs->formation_dist = 3.5 * 32;
        bs->arrive_time = 0;
      }
      break;
    case MSG_DEFENDKEYAREA:
      if ( !TeamPlayIsOn() || !BotAddressedToBot(bs, &match) )
        break;
      BotMatchVariable(&match, KEYAREA, itemname);
      if ( !BotGetMessageTeamGoal(bs, itemname, &bs->teamgoal) )
      {
        BotInitialChat(&bs->chatstate, "cannotfind", itemname, (char *)0);
        BotEnterChat(&bs->chatstate, bs->client, CHAT_TEAM);
        break;
      }
      bs->teammessage_time = AAS_Time() + 2 * random();
      bs->ltgtype = 3;
      bs->teamgoal_time = BotGetTime(&match);
      if ( !bs->teamgoal_time )
        bs->teamgoal_time = AAS_Time() + 120;
      bs->defendaway_time = 0;
      break;
    case MSG_CAMP:
      if ( !TeamPlayIsOn() || !BotAddressedToBot(bs, &match) )
        break;
      BotMatchVariable(&match, NETNAME, netname);
      client = FindClientByName(netname) + 1;
      if ( !client )
      {
        BotInitialChat(&bs->chatstate, "whois", netname, (char *)0);
        BotEnterChat(&bs->chatstate, bs->client, CHAT_TEAM);
        break;
      }
      BotMatchVariable(&match, KEYAREA, itemname);
      if ( match.subtype & ST_THERE )
      {
        bs->teamgoal.entitynum = bs->entitynum;
        bs->teamgoal.areanum = bs->ms.areanum;
        VectorCopy(bs->origin, bs->teamgoal.origin);
        VectorSet(bs->teamgoal.mins, -8, -8, -8);
        VectorSet(bs->teamgoal.maxs, 8, 8, 8);
      }
      else if ( match.subtype & ST_HERE )
      {
        if ( client == bs->entitynum )
          break;
        bs->teamgoal.entitynum = 0;
        entinfo = AAS_EntityInfo(client);
        if ( entinfo.valid )
        {
          areanum = AAS_PointAreaNum(entinfo.origin);
          if ( areanum )
          {
            if ( AAS_AreaReachability(areanum) && BotEntityVisible(bs->entitynum, bs->eye, bs->viewangles, 360.0, client) )
            {
              bs->teamgoal.entitynum = client;
              bs->teamgoal.areanum = areanum;
              VectorCopy(entinfo.origin, bs->teamgoal.origin);
              VectorSet(bs->teamgoal.mins, -8, -8, -8);
              VectorSet(bs->teamgoal.maxs, 8, 8, 8);
            }
          }
        }
        if ( !bs->teamgoal.entitynum )
        {
          BotInitialChat(&bs->chatstate, "whereareyou", netname,
                         (char *)0);
          BotEnterChat(&bs->chatstate, bs->client, CHAT_TEAM);
          break;
        }
      }
      else if ( !BotGetMessageTeamGoal(bs, itemname, &bs->teamgoal) )
      {
        BotInitialChat(&bs->chatstate, "cannotfind", itemname, (char *)0);
        BotEnterChat(&bs->chatstate, bs->client, CHAT_TEAM);
        break;
      }
      bs->teammessage_time = AAS_Time() + 2 * random();
      bs->ltgtype = 6;
      bs->teamgoal_time = BotGetTime(&match);
      if ( !bs->teamgoal_time )
        bs->teamgoal_time = AAS_Time() + 300;
      bs->teammate = client;
      bs->arrive_time = 0;
      break;
    case MSG_PATROL:
      if ( !TeamPlayIsOn() )
        break;
      if ( !BotAddressedToBot(bs, &match) )
        break;
      if ( !BotGetPatrolWaypoints(bs, &match) )
        break;
      bs->teammessage_time = AAS_Time() + 2 * random();
      bs->ltgtype = 7;
      bs->teamgoal_time = BotGetTime(&match);
      if ( !bs->teamgoal_time )
        bs->teamgoal_time = AAS_Time() + 300;
      break;
    case MSG_GETFLAG:
      if ( ctf->value == 0.0f || !ctf_flag1.areanum || !ctf_flag2.areanum || !BotAddressedToBot(bs, &match) )
        break;
      bs->teammessage_time = AAS_Time() + 2 * random();
      bs->ltgtype = 4;
      bs->teamgoal_time = AAS_Time() + 180;
      break;
    case MSG_RUSHBASE:
      if ( ctf->value == 0.0f || !ctf_flag1.areanum || !ctf_flag2.areanum || !BotAddressedToBot(bs, &match) )
        break;
      bs->teammessage_time = AAS_Time() + 2 * random();
      bs->ltgtype = 5;
      bs->teamgoal_time = AAS_Time() + 120;
      bs->rushbaseaway_time = 0;
      break;
    case MSG_JOINSUBTEAM:
      if ( !TeamPlayIsOn() || !BotAddressedToBot(bs, &match) )
        break;
      BotMatchVariable(&match, TEAMNAME, teammate);
      strncpy(bs->teamleader, teammate, 32);
      bs->teamleader[31] = '\0';
      BotInitialChat(&bs->chatstate, "joinedteam", teammate, (char *)0);
      BotEnterChat(&bs->chatstate, bs->client, CHAT_TEAM);
      break;
    case MSG_LEAVESUBTEAM:
      if ( !TeamPlayIsOn() || !BotAddressedToBot(bs, &match) )
        break;
      if ( strlen(bs->teamleader) )
        BotInitialChat(&bs->chatstate, "leftteam", bs->teamleader,
                       (char *)0);
      BotEnterChat(&bs->chatstate, bs->client, CHAT_TEAM);
      strcpy(bs->teamleader, "");
      break;
    case MSG_CHECKPOINT:
      if ( !TeamPlayIsOn() )
        break;
      BotMatchVariable(&match, POSITION, buf);
      VectorClear(position);
      sscanf(buf, "%f %f %f", &position[0], &position[1], &position[2]);
      position[2] += 0.5;
      areanum = AAS_PointAreaNum(position);
      if ( !areanum )
      {
        if ( !BotAddressedToBot(bs, &match) )
          break;
        BotInitialChat(&bs->chatstate, "checkpoint_invalid", (char *)0);
        BotEnterChat(&bs->chatstate, bs->client, CHAT_TEAM);
        break;
      }
      BotMatchVariable(&match, NAME, buf);
      cp = BotFindWayPoint(BotCheckpoints(bs), buf);
      if ( cp )
      {
        if ( cp->next )
          cp->next->prev = cp->prev;
        if ( cp->prev )
          cp->prev->next = cp->next;
        else
          BotCheckpoints(bs) = cp->next;
        FreeMemory(cp);
      }
      cp = BotCreateWayPoint(buf, position, areanum);
      cp->next = BotCheckpoints(bs);
      if ( BotCheckpoints(bs) )
        BotCheckpoints(bs)->prev = cp;
      BotCheckpoints(bs) = cp;
      if ( BotAddressedToBot(bs, &match) )
      {
        sprintf(buf, "%1.0f %1.0f %1.0f", cp->goal.origin[0], cp->goal.origin[1], cp->goal.origin[2]);
        BotInitialChat(&bs->chatstate, "checkpoint_confirm", cp->name, buf,
                       (char *)0);
        BotEnterChat(&bs->chatstate, bs->client, CHAT_TEAM);
      }
      break;
    case MSG_CREATENEWFORMATION:
      EA_SayTeam(bs->client,
                 "the part of my brain to create formations has been damaged");
      break;
    case MSG_FORMATIONPOSITION:
      EA_SayTeam(bs->client,
                 "the part of my brain to create formations has been damaged");
      break;
    case MSG_FORMATIONSPACE:
      if ( !TeamPlayIsOn() || !BotAddressedToBot(bs, &match) )
        break;
      BotMatchVariable(&match, NUMBER, buf);
      if ( match.subtype & ST_FEET )
        space = 0.3048 * 32 * atof(buf);
      else
        space = 32 * atof(buf);
      if ( space < 48 || space > 500 )
        space = 100;
      bs->formation_dist = space;
      break;
    case MSG_DISMISS:
      if ( !TeamPlayIsOn() )
        break;
      if ( !BotAddressedToBot(bs, &match) )
        break;
      if ( bs->ltgtype == 2 || bs->ltgtype == 1 )
        bs->ltgtype = 0;
      break;
    case MSG_STARTTEAMLEADERSHIP:
      if ( !TeamPlayIsOn() )
        break;
      BotMatchVariable(&match, TEAMMATE, teammate);
      if ( match.subtype & ST_I )
      {
        strncpy(bs->formation_teammate, teammate, 16);
        bs->formation_teammate[15] = '\0';
        break;
      }
      v49 = FindClientByName(teammate);
      if ( v49 < 0 )
        break;
      strcpy(bs->formation_teammate, ClientName(v49));
      break;
    case MSG_STOPTEAMLEADERSHIP:
      if ( !TeamPlayIsOn() )
        break;
      BotMatchVariable(&match, TEAMMATE, teammate);
      if ( match.subtype & ST_I )
      {
        BotMatchVariable(&match, NETNAME, netname);
        v50 = FindClientByName(netname);
      }
      else
      {
        v50 = FindClientByName(teammate);
      }
      if ( v50 < 0 )
        break;
      if ( _strcmpi(bs->formation_teammate, ClientName(v50)) )
        break;
      bs->formation_teammate[0] = '\0';
      break;
    case MSG_WHATAREYOUDOING:
      if ( !BotAddressedToBot(bs, &match) )
        break;
      switch ( bs->ltgtype )
      {
        case 1:
          BotMatchVariable(&match, NETNAME, netname);
          EasyClientName(bs->teammate - 1, netname);
          BotInitialChat(&bs->chatstate, "helping", netname, (char *)0);
          break;
        case 2:
          BotMatchVariable(&match, NETNAME, netname);
          EasyClientName(bs->teammate - 1, netname);
          BotInitialChat(&bs->chatstate, "accompanying", netname,
                         (char *)0);
          break;
        case 3:
          BotInitialChat(&bs->chatstate, "defending", BotGoalName(bs->teamgoal.number), (char *)0);
          break;
        case 6:
          BotInitialChat(&bs->chatstate, "camping", (char *)0);
          break;
        case 7:
          BotInitialChat(&bs->chatstate, "patrolling", (char *)0);
          break;
        case 4:
          BotInitialChat(&bs->chatstate, "capturingflag", (char *)0);
          break;
        case 5:
          BotInitialChat(&bs->chatstate, "rushingbase", (char *)0);
          break;
        default:
          return 0;
      }
      BotEnterChat(&bs->chatstate, bs->client, CHAT_TEAM);
      break;
    default:
      botimport.Print(PRT_MESSAGE, "unknown match type\n");
      break;
  }
  return 1;
}

// gladiator.dll: 10028650..100288E8
// gladi386.so:   00032D6C..000330B5
void __cdecl BotCheckConsoleMessages(bot_state_t *bs)
{
  char *botname, *ptr;
  float chat_reply;
  int context;
  bot_consolemessage_t *m;

  botname = ClientName(bs->client);
  for ( m = BotNextConsoleMessage(&bs->chatstate); m; m = BotNextConsoleMessage(&bs->chatstate) )
  {
    if ( BotNumConsoleMessages(&bs->chatstate) < 10 )
    {
      if ( m->type == CMS_CHAT && m->time > AAS_Time() - (1 + random()) )
        return;
    }
    if ( m->type == CMS_CHAT )
    {
      ptr = strstr(m->message, ":");
      if ( ptr )
      {
        if ( !strncmp(m->message, botname, ptr - m->message) || !strncmp(m->message + 1, botname, ptr - m->message - 2) )
        {
          BotRemoveConsoleMessage(&bs->chatstate, m);
          continue;
        }
      }
      else
      {
        BotRemoveConsoleMessage(&bs->chatstate, m);
        continue;
      }
    }
    UnifyWhiteSpaces(m->message);
    context = CONTEXT_NORMAL|CONTEXT_NEARBYITEM;
    if ( ctf->value )
      context = BotCTFTeam(bs) == 1 ? (CONTEXT_NORMAL|CONTEXT_NEARBYITEM|CONTEXT_CTFREDTEAM)
                                    : (CONTEXT_NORMAL|CONTEXT_NEARBYITEM|CONTEXT_CTFBLUETEAM);
    BotReplaceSynonyms(m->message, context);
    if ( !BotMatchMessage(bs, m->message) )
    {
      if ( m->type == CMS_CHAT && !nochat->value )
      {
        if ( BotAINode(bs) != AINode_Stand && BotValidChatPosition(bs) )
        {
          chat_reply = Characteristic_BFloat(BotCharacter(bs), CHARACTERISTIC_CHAT_REPLY, 0, 1);
          if ( random() < 1.5 / (NumBots() + 1) && random() < chat_reply )
          {
            ptr = strstr(m->message, ":");
            /* Q3's shape: a miss removes the message and continues.  Both
             * compilers cross-jump this remove into the loop's closing one,
             * so neither binary shows it, but cl.exe still counts it: the
             * extra &bs->chatstate reference is what gives that pointer ebp
             * and leaves `bs` in the edi that the strlen clobbers. */
            if ( !ptr )
            {
              BotRemoveConsoleMessage(&bs->chatstate, m);
              continue;
            }
            memmove(m->message, ptr + 1, strlen(ptr + 1) + 1);
            UnifyWhiteSpaces(m->message);
            if ( BotReplyChat(&bs->chatstate, m->message) )
            {
              BotRemoveConsoleMessage(&bs->chatstate, m);
              bs->stand_time = AAS_Time() + BotChatTime(bs);
              AIEnter_Stand(bs);
              return;
            }
          }
        }
      }
    }
    BotRemoveConsoleMessage(&bs->chatstate, m);
  }
}

// gladiator.dll: 100289A0..10028A15
// gladi386.so:   000330B8..0003312C
void __cdecl sub_100289A0(bot_state_t *bs, float a2)
{
  bs->ltime += a2;
  bs->thinktime = a2;
  VectorCopy(bs->snapshot.origin, bs->origin);
  VectorAdd(bs->snapshot.origin, bs->snapshot.viewoffset, bs->eye);
  memcpy(bs->inventory, bs->snapshot.inventory, 0x400u);
}

// gladiator.dll: 10028A40..10028A56
// gladi386.so:   0003312C..00033150
/* dkbot: cancel the push PM_AddCurrents adds, which the AI does not steer for */
#define DK_WATERSPEED 400.0f
static void dk_cancel_current(bot_state_t *bs)
{
  ea_state_t *ea = &botinputs[bs->client];
  vec3_t point, want;
  int contents, waist;
  float s;

  VectorCopy(bs->origin, point);
  point[2] -= 23.0f;                   /* PM_CatagorizePosition: mins[2] + 1 */
  contents = AAS_PointContents(point);
  if ( !(contents & 0x38) || !(contents & 0xFC0000) )
    return;
  VectorScale(ea->dir, ea->speed, want);
  s = DK_WATERSPEED;
  waist = (int)(bs->snapshot.viewoffset[2] + 24.0f) / 2;
  point[2] = bs->origin[2] - 24.0f + waist;
  if ( (bs->snapshot.pm_flags & 4) && !(AAS_PointContents(point) & 0x38) )
    s *= 0.5f;
  if ( contents & 0x40000 )  want[0] -= s;
  if ( contents & 0x80000 )  want[1] -= s;
  if ( contents & 0x100000 ) want[0] += s;
  if ( contents & 0x200000 ) want[1] += s;
  if ( contents & 0x400000 ) want[2] -= s;
  if ( contents & 0x800000 ) want[2] += s;
  ea->speed = VectorNormalize(want);
  VectorCopy(want, ea->dir);
}

int __cdecl sub_10028A40(bot_state_t *bs, float a2)
{
  dk_cancel_current(bs);  /* dkbot */
  return ((int (__cdecl *)(int, float))EA_EndRegular)(bs->client, a2);
}

/* dkbot: a health station heals a user within 64 units facing it; frame 0 is recharging */
#define DK_ITEM_STATION    8          /* tools/gen-botcfg.py */
#define DK_STATION_SEEK    96.0f      /* where the bot stops walking and starts using */
#define DK_STATION_NEAR    40.0f      /* flush with the station, solid at +-16 */
#define DK_STATION_REACH   3.0f       /* to get flush with it and press */
#define DK_STATION_IDLE    2.0f       /* no point gained this long: full or empty */
#define DK_STATION_MAX     30.0f      /* the station ends a session first */

int DK_LevelItemType(int number);

static struct dk_station_s {
  int   number, entnum, health, pressed;
  float entered, gained, lastpress;
} dk_station[DK_MAX_USERS];

int __cdecl AINode_DK_Station(bot_state_t *bs);

static void dk_leave_station(bot_state_t *bs)
{
  bot_goal_t *goal = (bot_goal_t *)BotGetTopGoal(&bs->goalstate);

  if ( goal && goal->number == dk_station[bs->client].number )
    BotPopGoal(&bs->goalstate);
}

int DK_AreaTrapped(int areanum);

#define DK_TRAPPED_WAIT  10.0f

static float dk_trapped_since[DK_MAX_USERS];

/* dkbot: a bot with no route back into the game kills itself, as a player would */
static void dk_check_trapped(bot_state_t *bs)
{
  int c = bs->client;

  if ( c < 0 || c >= DK_MAX_USERS )
    return;
  if ( BotIsDead(bs) || bs->enemy > 0 || !DK_AreaTrapped(AAS_PointAreaNum(bs->origin)) )
  {
    dk_trapped_since[c] = 0.0f;
    return;
  }
  if ( !dk_trapped_since[c] )
    dk_trapped_since[c] = AAS_Time();
  else if ( AAS_Time() > dk_trapped_since[c] + DK_TRAPPED_WAIT )
  {
    EA_Command(c, "kill", (char *)0);
    dk_trapped_since[c] = 0.0f;
  }
}

static void dk_check_station(bot_state_t *bs)
{
  ai_node_fn_t node = BotAINode(bs);
  bot_goal_t *goal;
  aas_entityinfo_t entinfo;
  bsp_trace_t trace;
  vec3_t dir;

  if ( bs->client < 0 || bs->client >= DK_MAX_USERS
    || (node != AINode_Seek_LTG && node != AINode_Seek_NBG) )
    return;
  goal = (bot_goal_t *)BotGetTopGoal(&bs->goalstate);
  if ( !goal || goal->entitynum <= 0 || DK_LevelItemType(goal->number) != DK_ITEM_STATION )
    return;
  entinfo = AAS_EntityInfo(goal->entitynum);
  if ( !entinfo.valid || entinfo.frame != 1 )
    return;
  VectorSubtract(entinfo.origin, bs->origin, dir);
  if ( VectorLength(dir) > DK_STATION_SEEK )
    return;
  trace = AAS_Trace(bs->eye, NULL, NULL, entinfo.origin, bs->entitynum, DK_MASK_USE);
  if ( trace.fraction < 1.0f && trace.ent != goal->entitynum )
    return;
  dk_station[bs->client].number = goal->number;
  dk_station[bs->client].entnum = goal->entitynum;
  dk_station[bs->client].health = bs->inventory[INVENTORY_HEALTH];
  dk_station[bs->client].pressed = 0;
  dk_station[bs->client].entered = AAS_Time();
  BotRecordNodeSwitch(bs, "health station", BotGoalName(goal->number));
  BotAINode(bs) = AINode_DK_Station;
}

int __cdecl AINode_DK_Station(bot_state_t *bs)
{
  aas_entityinfo_t entinfo;
  bot_moveresult_t use;
  vec3_t dir;
  struct dk_station_s *st;

  if ( BotIsObserver(bs) )
  {
    AIEnter_Observer(bs);
    return 0;
  }
  if ( BotIntermission(bs) )
  {
    AIEnter_Intermission(bs);
    return 0;
  }
  if ( BotIsDead(bs) )
  {
    AIEnter_Respawn(bs);
    return 0;
  }
  bs->enemy = 0;
  if ( BotFindEnemy(bs) )
  {
    dk_leave_station(bs);
    if ( BotWantsToRetreat((int *)bs) )
      AIEnter_Battle_Retreat(bs);
    else
    {
      BotResetLastAvoidReach(&bs->ms);
      BotEmptyGoalStack(&bs->goalstate);
      AIEnter_Battle_Fight(bs);
    }
    return 0;
  }
  st = &dk_station[bs->client];
  if ( bs->inventory[INVENTORY_HEALTH] > st->health )
  {
    st->health = bs->inventory[INVENTORY_HEALTH];
    st->gained = AAS_Time();
  }
  entinfo = AAS_EntityInfo(st->entnum);
  if ( !entinfo.valid || entinfo.frame != 1 || AAS_Time() > st->entered + DK_STATION_MAX
    || (st->pressed ? AAS_Time() > st->gained + DK_STATION_IDLE
                    : AAS_Time() > st->entered + DK_STATION_REACH) )
  {
    dk_leave_station(bs);
    AIEnter_Seek_LTG(bs);
    return 0;
  }
  VectorSubtract(entinfo.origin, bs->origin, dir);
  dir[2] = 0.0f;
  if ( VectorNormalize(dir) > DK_STATION_NEAR )
    EA_Move(bs->client, dir, 200.0f);
  memset(&use, 0, sizeof(use));
  /* a press while it heals only plays the station's busy sound */
  if ( dk_use_entity(bs, &use, st->entnum, !st->pressed || st->gained <= st->lastpress) )
  {
    if ( !st->pressed )
      st->gained = AAS_Time();
    st->pressed = 1;
    st->lastpress = AAS_Time();
  }
  if ( use.flags & 1 )
    VectorCopy(use.ideal_viewangles, bs->ideal_viewangles);
  BotChangeViewAngles(bs, bs->thinktime);
  return 1;
}
/* end dkbot */

// gladiator.dll: 10028A70..10028BCF
// gladi386.so:   00033150..0003341C
int BotDeathmatchAI(bot_state_t *bs, float thinktime)
{
  int i; // edi

  sub_100289A0(bs, thinktime);
  if ( mapchange && AAS_Initialized() )
    mapchange = 0;
  if ( bs->inuse_marker )
  {
    char *characteristic_string;

    characteristic_string = Characteristic_String(BotCharacter(bs), CHARACTERISTIC_GENDER);
    EA_Command(bs->client, "gender", characteristic_string, (char *)0);
    if ( LibVarValue("altnames", (char *)"0") != 0.0f )
    {
      characteristic_string = Characteristic_String(BotCharacter(bs), CHARACTERISTIC_ALT_NAME);
      EA_Command(bs->client, "name", characteristic_string, (char *)0);
    }
    bs->inuse_marker = 0;
  }
  BotUpdateInventory(bs);
  BotCheckConsoleMessages(bs);
  if ( !BotAINode(bs) )
    AIEnter_Seek_LTG(bs);
  if ( AAS_Time() - 8.0f < bs->setup_time && BotChat_EnterGame(bs) )
  {
    bs->stand_time = AAS_Time() + BotChatTime(bs);
    AIEnter_Stand(bs);
  }
  BotResetNodeSwitches();
  dk_check_station(bs);  /* dkbot */
  dk_check_trapped(bs);  /* dkbot */
  if ( bs->client >= 0 && bs->client < DK_MAX_USERS )
    dk_retreat_view[bs->client] = 0;
  for ( i = 0; i < 50; ++i )
  {
    if ( (BotAINode(bs))(bs) )
      break;
  }
  /* dkbot: see dk_retreat_view */
  if ( bs->client >= 0 && bs->client < DK_MAX_USERS && dk_retreat_view[bs->client]
    && BotAINode(bs) == AINode_Battle_Retreat )
    BotChangeViewAngles(bs, bs->thinktime);
  if ( i >= 50 )
  {
    BotDumpGoalStack(&bs->goalstate);
    BotDumpAvoidGoals(&bs->goalstate);
    BotDumpNodeSwitches(bs);
  }
  if ( *(_DWORD *)bs )
    return sub_10028A40(bs, thinktime);
}

// gladiator.dll: 10028C30..10028DF7
// gladi386.so:   0003341C..0003366C
void DK_ForbidFatalJumps(void);  /* dkbot */

void BotSetupDeathmatchAI()
{
  dmflags = LibVar("dmflags", (char *)"0");
  ctf = LibVar("ctf", (char *)"0");
  ch = LibVar("ch", (char *)"0");
  ra = LibVar("ra", (char *)"0");
  fastchat = LibVar("fastchat", (char *)"0");
  nochat = LibVar("nochat", (char *)"0");
  teamplay = LibVar("teamplay", (char *)"0");
  usehook = LibVar("usehook", (char *)"0");
  rocketjump = LibVar("rocketjump", (char *)"1");
  techs = LibVar("runes", (char *)"0");
  teamplay_shell = LibVar("teamplay_shell", (char *)"0");
  assimilation = LibVar("assimilation", (char *)"0");
  DK_ForbidFatalJumps();  /* dkbot */
  if ( ctf->value != 0.0f )
  {
    /* dkbot: our items are named by classname (tools/gen-botcfg.py) */
    if ( BotGetLevelItemGoal(-1, "item_flag_team1", &ctf_flag1) < 0 )
      botimport.Print(PRT_WARNING, "CTF without item_flag_team1\n");
    if ( BotGetLevelItemGoal(-1, "item_flag_team2", &ctf_flag2) < 0 )
      botimport.Print(PRT_WARNING, "CTF without item_flag_team2\n");
    modelindex3_flag1 = AAS_IndexFromModel("players/male/flag1.md2");
    modelindex3_flag2 = AAS_IndexFromModel("players/male/flag2.md2");
    modelindex_tech1 = AAS_IndexFromModel("models/ctf/resistance/tris.md2");
    modelindex_tech2 = AAS_IndexFromModel("models/ctf/strength/tris.md2");
    modelindex_tech3 = AAS_IndexFromModel("models/ctf/haste/tris.md2");
    modelindex_tech4 = AAS_IndexFromModel("models/ctf/regeneration/tris.md2");
  }
  mapchange = 1;
}

// gladiator.dll: 10028E80..10028E81
// gladi386.so:   0003366C..0003366D
/* Empty in the original.  Q3 pairs BotShutdownDeathmatchAI right after
 * BotSetupDeathmatchAI in the same file, and the ELF oracle confirms the same
 * adjacency here (F814 ends exactly where the 1-byte F815 begins).  Defining it in
 * be_ai2_main.c lets gcc -O6 auto-inline the empty body away at its only call site;
 * keeping it here, where that TU cannot see the definition, forces the real
 * out-of-line call both binaries have. */
void BotShutdownDeathmatchAI(void) { /* empty body — original returns immediately */ }
