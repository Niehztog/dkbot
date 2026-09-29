/*
 * be_ai_goal.c — Gladiator Bot v0.96 botlib (Mr. Elusive, 1999), reconstructed
 * from the Windows gladiator.dll.  DLL extent 0x1002ED20..0x10030A20.
 */

#include "botlib_port.h"
#include "l_libvar.h"
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
#include "be_ai_goal.h"
#include "be_aas_bspq2.h"
#include "be_aas_entity.h"
#include "be_aas_main.h"
#include "be_aas_move.h"
#include "be_aas_reach.h"
#include "be_aas_route.h"
#include "be_ai_move.h"
#include "be_ai_weight.h"
#include "be_interface.h"
#include "l_libvar.h"
#include "l_log.h"
#include "l_memory.h"
#include "l_precomp.h"
#include "l_script.h"
#include "l_struct.h"
#include "l_utils.h"

/* iteminfo_struct — descriptor at 0x1005D890 (284 B); field table at 0x1005D7B0.
 * Deliberately NOT `static`: gladi386.so exports it as a `D` symbol
 * (0005bcc8 D iteminfo_fields), which a file-static array can never be. */
char *iteminfo_fields[] = {
    FE("name",        0x000, FT_STRING, 0, 0x00000000),
    FE("model",       0x0A0, FT_STRING, 0, 0x00000000),
    FE("type",        0x0F4, FT_INT, 0, 0x00000000),
    FE("index",       0x0F8, FT_INT, 0, 0x00000000),
    FE("respawntime", 0x0FC, FT_FLOAT, 0, 0x00000000),
    FE("mins",        0x100, FT_FLOAT|FT_ARRAY, 3, 0x00000000),  /* vec3 */
    FE("maxs",        0x10C, FT_FLOAT|FT_ARRAY, 3, 0x00000000),  /* vec3 */
    FE_END
};
structdef_t iteminfo_struct = { 284, iteminfo_fields };

levelitem_t *freelevelitems; // 0x10064344 free-list head    (be_ai_goal.c; was dword_10064344)
int numlevelitems;           // 0x10064354 active item count  (be_ai_goal.c; was dword_10064354)
levelitem_t *levelitemheap;  // 0x10064358 pool base          (be_ai_goal.c; was dword_10064358)
itemconfig_t *itemconfig; /* current item config (was dword_1006435C) */
levelitem_t *levelitems;     // 0x10064360 active-list head   (be_ai_goal.c; was dword_10064360)

// gladiator.dll: 1002ED20..1002F027
// gladi386.so:   0003E030..0003E338
itemconfig_t * LoadItemConfig(char *filename)
{
  int max_iteminfo;
  source_t *src;
  itemconfig_t *cfg;
  iteminfo_t *item;
  /* Declaration order reversed vs. stack-offset order (reverse-declaration-order rule
   * — see BotTravel_Jump): the original has file_ref at the LOWEST offset of these
   * three, then Destination, then ArgList highest, so the source must declare them in
   * the opposite order. */
  char ArgList[sizeof(token_t)] __attribute__((aligned(8))); // [esp+13Ch] [ebp-430h] BYREF
  char Destination[144]; // [esp+ACh] [ebp-4C0h] BYREF
  bot_fileref_t file_ref; /* original bot_fileref_t local */

  max_iteminfo = (int)LibVarValue("max_iteminfo", (char *)"256");
  if ( max_iteminfo < 0 )
  {
    botimport.Print(PRT_ERROR, "max_iteminfo = %d\n", max_iteminfo);
    max_iteminfo = 256;
    LibVarSet("max_iteminfo", (char *)"256");
  }
  memset(&file_ref, 0, sizeof(file_ref));
  strncpy(Destination, filename, 0x90u);
  if ( !FindQuakeFile(Destination, &file_ref) )
  {
    botimport.Print(PRT_ERROR, "couldn't find %s\n", Destination);
    return 0;
  }
  src = LoadSourceFile(file_ref.path, file_ref.fileofs, file_ref.filelen);
  if ( !src )
  {
    botimport.Print(PRT_ERROR, "counldn't load %s\n", Destination);
    return 0;
  }
  cfg = (itemconfig_t *)GetClearedMemory(sizeof(itemconfig_t) + sizeof(iteminfo_t) * max_iteminfo);
  cfg->items    = (iteminfo_t *)(cfg + 1);
  cfg->numitems = 0;
  /* Same shape as the sibling LoadWeaponConfig: a plain top-tested
   * `while (PC_ReadTokenHandle(...))`, every error path written out INLINE with no
   * shared label, and the unknown-definition case as the trailing `else`.  An
   * `if(read){ do{…}while(read); }` form makes MSVC6 peel the strcmp. */
  while ( PC_ReadTokenHandle(src, ArgList) )
  {
    if ( !strcmp(ArgList, "iteminfo") )
    {
      if ( cfg->numitems >= max_iteminfo )
      {
        SourceError(src, "more than %d item info defined\n", max_iteminfo);
        FreeMemory(cfg);
        FreeSource(src);
        return 0;
      }
      item = &cfg->items[cfg->numitems];
      memset(item, 0, sizeof(iteminfo_t));
      if ( !PC_ExpectTokenType(src, TT_STRING, 0, ArgList) )
      {
        FreeMemory(cfg);
        FreeMemory(src);
        return 0;
      }
      StripDoubleQuotes(ArgList);
      strncpy(item->dispname, ArgList, 80);
      if ( !ReadStructure(src, &iteminfo_struct, item) )
      {
        FreeMemory(cfg);
        FreeSource(src);
        return 0;
      }
      item->number = cfg->numitems++;
    }
    else
    {
      SourceError(src, "unknown definition %s\n", ArgList);
      FreeMemory(cfg);
      FreeSource(src);
      return 0;
    }
  }
  FreeSource(src);
  if ( !cfg->numitems )
    botimport.Print(PRT_WARNING, "no item info loaded\n");
  if ( file_ref.filelen )
    botimport.Print(PRT_MESSAGE, "loaded %s\\%s\n", file_ref.path, Destination);
  else
    botimport.Print(PRT_MESSAGE, "loaded %s\n", Destination);
  return cfg;
}

// gladiator.dll: 1002F100..1002F171
// gladi386.so:   0003E338..0003E3C2
int *__cdecl ItemWeightIndex(weightconfig_t *iwc, itemconfig_t *ic)
{
  int *index; // eax / [esp+10h] [ebp+8h]
  int i; // ebx

  index = (int *)GetClearedMemory(4 * ic->numitems);
  for ( i = 0; i < ic->numitems; ++i )
  {
    index[i] = FindFuzzyWeight(iwc, ic->items[i].dispname);
    if ( index[i] < 0 )
      Log_Write("item info %d \"%s\" has no fuzzy weight", i,
                ic->items[i].dispname);
  }
  return index;
}

// gladiator.dll: 1002F1A0..1002F231
// gladi386.so:   0003E3C4..0003E54B
void InitLevelItemHeap()
{
  int max_levelitems;
  int i;

  if ( levelitemheap )
    FreeMemory(levelitemheap);
  max_levelitems = (int)LibVarValue("max_levelitems", (char *)"512");
  levelitemheap = (levelitem_t *)GetMemory(sizeof(levelitem_t) * max_levelitems);
  /* ONE textual tail: MSVC6 /O2 duplicates it so the loop-skipped path can reuse the
   * still-live GetMemory result while the post-loop path reloads the global.  Writing
   * both tails out explicitly makes them identical and they get tail-merged back into
   * one. */
  for ( i = 0; i < max_levelitems - 2; ++i )
    levelitemheap[i].next = &levelitemheap[i + 1];
  levelitemheap[max_levelitems - 1].next = NULL;
  freelevelitems = levelitemheap;
}

// gladiator.dll: 1002F270..1002F296
// gladi386.so:   0003E54C..0003E589
_DWORD *__cdecl AllocLevelItem(void)
{
  levelitem_t *result;

  result = freelevelitems;
  if ( !freelevelitems )
  {
    botimport.Print(PRT_FATAL, "out of level items\n");
    return 0;
  }
  freelevelitems = result->next;
  return (_DWORD *)result;
}

// gladiator.dll: 1002F2B0..1002F2C3
// gladi386.so:   0003E58C..0003E5B2
void __cdecl FreeLevelItem(levelitem_t *li)
{
  li->next = freelevelitems;
  freelevelitems = li;
}

// gladiator.dll: 1002F2E0..1002F307
// gladi386.so:   0003E5B4..0003E5F0
levelitem_t *__cdecl AddLevelItemToList(levelitem_t *li)
{
  if ( levelitems )
    levelitems->prev = li;
  li->prev = 0;
  li->next = levelitems;
  levelitems = li;
}

// gladiator.dll: 1002F320..1002F34A
// gladi386.so:   0003E5F0..0003E62A
levelitem_t *__cdecl RemoveLevelItemFromList(levelitem_t *li)
{
  levelitem_t *prev;
  levelitem_t *next;

  prev = li->prev;
  if ( prev )
    prev->next = li->next;
  else
    levelitems = li->next;
  next = li->next;
  if ( next )
    next->prev = li->prev;
}

// gladiator.dll: 1002F360..1002F5E6
// gladi386.so:   0003E62C..0003E96E
void BotInitLevelItems()
{
  /* ic declared last of the pointer/int scalars (reverse-declaration-order
   * rule) — real has ic at a LOWER stack offset than notspawnflags_mask. */
  int notspawnflags_mask; // [esp+18h] (LibVar("notspawnflags","2048") return value)
  bsp_entity_t *v4; // ebp
  /* `i` is ONE variable reused for both the modelindex-fixup loop and the later
   * item-name search loop.  IDA split it into `i`/`v7` as if they were different
   * locals with disjoint lifetimes; merging them back is what makes gcc assign it
   * edi/v4 ebp exactly like the original.  A plain declaration-order swap of two
   * distinct locals has no effect on the register choice, so one shared counter is the
   * actual original shape. */
  int i; // edi
  const char *classname; // [esp+28h] [ebp-18h]
  bsp_entity_t *ent; // [esp+2Ch] [ebp-14h]
  itemconfig_t *ic; // ebx
  /* The item origin must be a real vec3_t — it is passed by address to
   * AAS_VectorForBSPEpairKey, AAS_DropToFloor and AAS_BestReachableArea, and split into
   * separate locals nearly every item ends up with areanum 0. */
  vec3_t origin; // [esp+34h] [ebp-Ch] BYREF (was v12+v13+v14)

  InitLevelItemHeap();
  levelitems = 0;
  numlevelitems = 0;
  ic = itemconfig;
  if ( ic )
  {
    v4 = AAS_ParseBSPEntities();
    notspawnflags_mask = (int)LibVarValue("notspawnflags", (char *)"2048");
    for ( i = 0; i < ic->numitems; ++i )
    {
      ic->items[i].modelindex = AAS_IndexFromModel(ic->items[i].model);
      if ( !ic->items[i].modelindex )
        Log_Write("item %s has modelindex 0", ic->items[i].dispname);
    }
    ent = v4;
    if ( v4 )
    {
      do
      {
        classname = (const char *)AAS_ValueForBSPEpairKey(ent, "classname");
        if ( classname && (AAS_IntForBSPEpairKey(ent, "spawnflags") & notspawnflags_mask) == 0 )
        {
          for ( i = 0; i < ic->numitems; ++i )
          {
            /* dkbot: an iteminfo per model; take the one the map registered */
            if ( !strcmp(classname, ic->items[i].dispname) && ic->items[i].modelindex )
            {
              if ( AAS_VectorForBSPEpairKey(ent, "origin", origin) )
              {
                levelitem_t *li = (levelitem_t *)AllocLevelItem();
                if ( !li )
                  goto done;
                li->number = ++numlevelitems;
                li->timeout = 0.0f;
                li->entitynum = 0;
                if ( !AAS_DropToFloor(origin, ic->items[i].mins, ic->items[i].maxs) )
                  botimport.Print(PRT_MESSAGE, "%s in solid at (%1.1f %1.1f %1.1f)\n", classname, origin[0], origin[1], origin[2]);
                li->iteminfo = i;
                VectorCopy(origin, li->origin);
                li->areanum = AAS_BestReachableArea(
                                         origin,
                                         ic->items[i].mins,
                                         ic->items[i].maxs,
                                         li->goalorigin);
                AddLevelItemToList(li);
              }
              else
              {
                botimport.Print(PRT_ERROR, "item %s without origin\n", classname);
              }
              break;
            }
          }
          if ( i >= ic->numitems )
            Log_Write("entity %s unkown item", classname);
        }
        ent = ent->next;
      }
      while ( ent );
    }
    botimport.Print(PRT_MESSAGE, "found %d level items\n", numlevelitems);
  }
  // Single shared exit: the function is void, so the !itemconfig, alloc-fail and normal
  // paths all fall into ONE epilogue.  A return value would give each `return` its own.
done:
  ;
}

// gladiator.dll: 1002F6A0..1002F6DA
// gladi386.so:   0003E970..0003E9CF
char *__cdecl BotGoalName(int number)
{
  levelitem_t *v1;

  if ( !itemconfig )
    return "";
  for ( v1 = levelitems; v1; v1 = v1->next )
  {
    if ( v1->number == number )
      return itemconfig->items[v1->iteminfo].name;
  }
  return "";
}

// gladiator.dll: 1002F6F0..1002F713
// gladi386.so:   0003E9D0..0003EA0E
void __cdecl BotResetAvoidGoals(bot_goalstate_t *goalstate)
{
  memset(goalstate->avoidgoals, 0, sizeof(goalstate->avoidgoals));
  memset(goalstate->avoidgoaltimes, 0, sizeof(goalstate->avoidgoaltimes));
}

// gladiator.dll: 1002F730..1002F789
// gladi386.so:   0003EA10..0003EB46
void __cdecl BotDumpAvoidGoals(bot_goalstate_t *goalstate)
{
  int i;

  for ( i = 0; i < 64; i++ )
  {
    if ( goalstate->avoidgoaltimes[i] >= AAS_Time() )
    {
      Log_Write("avoid goal %s, number %d for %f seconds", BotGoalName(goalstate->avoidgoals[i]),
                goalstate->avoidgoals[i], goalstate->avoidgoaltimes[i] - AAS_Time());
    }
  }
}

// gladiator.dll: 1002F7B0..1002F7F9
// gladi386.so:   0003EB48..0003EC11
void __cdecl BotAddToAvoidGoals(bot_goalstate_t *gs, int number, float avoidtime)
{
  int v3; // esi

  for ( v3 = 0; v3 < 64; ++v3 )
  {
    if ( AAS_Time() > gs->avoidgoaltimes[v3] )
    {
      gs->avoidgoals[v3] = number;
      gs->avoidgoaltimes[v3] = AAS_Time() + avoidtime;
      return;
    }
  }
}

// gladiator.dll: 1002F820..1002F86F
// gladi386.so:   0003EC14..0003ECF7
float __cdecl BotAvoidGoalTime(bot_goalstate_t *goalstate, int number)
{
  int i;

  for ( i = 0; i < 64; i++ )
  {
    if ( goalstate->avoidgoals[i] == number && goalstate->avoidgoaltimes[i] >= AAS_Time() )
    {
      return goalstate->avoidgoaltimes[i] - AAS_Time();
    }
  }
  return 0.0f;
}

/* dkbot: from lava or slime, route through it but skip goals in it */
int DK_OriginInHazard(const float *origin);
#define DK_GOAL_IN_TOXIC(a) \
  (aasworld.areasettings[(a)].contents & (AREACONTENTS_LAVA | AREACONTENTS_SLIME))

/* dkbot: item types of tools/gen-botcfg.py; a station is ready at frame 1 */
#define DK_ITEM_STATION 8
#define DK_ITEM_WEAPON  2

/* Map weapons only: a dropped item has a timeout. */
int DK_LevelHasWeapons(void)
{
  levelitem_t *li;

  if ( !itemconfig )
    return 0;
  for ( li = levelitems; li; li = li->next )
  {
    if ( !li->timeout && itemconfig->items[li->iteminfo].type == DK_ITEM_WEAPON )
      return 1;
  }
  return 0;
}

int DK_LevelItemType(int number)
{
  levelitem_t *li;

  if ( !itemconfig )
    return -1;
  for ( li = levelitems; li; li = li->next )
  {
    if ( li->number == number )
      return itemconfig->items[li->iteminfo].type;
  }
  return -1;
}

static int dk_goal_unready(itemconfig_t *ic, levelitem_t *li)
{
  return ic->items[li->iteminfo].type == DK_ITEM_STATION
      && (!li->entitynum || AAS_EntityInfo(li->entitynum).frame != 1);
}

// gladiator.dll: 1002F890..1002F9C5
// gladi386.so:   0003ECF8..0003EE40
int __cdecl BotGetLevelItemGoal(int index, char *name, bot_goal_t *goal)
{
  levelitem_t *li;

  if ( !itemconfig )
    return -1;
  for ( li = levelitems; li; li = li->next )
  {
    if ( li->number > index && !_strcmpi(name, itemconfig->items[li->iteminfo].name) )
    {
      goal->areanum   = li->areanum;
      VectorCopy(li->goalorigin, goal->origin);
      goal->entitynum = li->entitynum;
      VectorCopy(itemconfig->items[li->iteminfo].mins, goal->mins);
      VectorCopy(itemconfig->items[li->iteminfo].maxs, goal->maxs);
      goal->number    = li->number;
      return li->number;
    }
  }
  return -1;
}

// gladiator.dll: 1002FA20..1002FC9F
// gladi386.so:   0003EE40..0003F195
void BotUpdateEntityItems(void)
{
  int ent, i, modelindex;
  vec3_t dir;
  levelitem_t *li, *nextli;
  aas_entityinfo_t entinfo;
  itemconfig_t *ic;

  for ( li = levelitems; li; li = nextli )
  {
    nextli = li->next;
    if ( li->timeout )
    {
      if ( li->timeout < AAS_Time() )
      {
        RemoveLevelItemFromList(li);
        FreeLevelItem(li);
      }
    }
  }
  ic = itemconfig;
  if ( !itemconfig )
    return;
  for ( ent = AAS_NextBSPEntity(0); ent; ent = AAS_NextBSPEntity(ent) )
  {
    modelindex = AAS_EntityModelindex(ent);
    if ( !modelindex )
      continue;
    entinfo = AAS_EntityInfo(ent);
    if ( entinfo.origin[0] != entinfo.lastvisorigin[0] ||
         entinfo.origin[1] != entinfo.lastvisorigin[1] ||
         entinfo.origin[2] != entinfo.lastvisorigin[2] )
      continue;
    for ( li = levelitems; li; li = li->next )
    {
      if ( ic->items[li->iteminfo].modelindex == modelindex )
      {
        if ( li->entitynum )
        {
          if ( li->entitynum == ent )
          {
            VectorCopy(entinfo.origin, li->origin);
            break;
          }
        }
        else
        {
          VectorSubtract(li->origin, entinfo.origin, dir);
          if ( VectorLength(dir) < 20 )
          {
            li->entitynum = ent;
            VectorCopy(entinfo.origin, li->origin);
            /* `i` is stale here -- the 1999 original's own bug, disasm-confirmed (the
             * model check loads li->iteminfo into eax @1002fb20, but the call passes
             * ebx @1002fbd2): Q3 fixed it to ic->iteminfo[li->iteminfo], which is what
             * GLAD_SERVERFIX(bot-item-stale-iteminfo) builds. */
            li->areanum = AAS_BestReachableArea(li->origin,
#if GLAD_SERVERFIX /* GLAD_SERVERFIX(bot-item-stale-iteminfo) */
                                                ic->items[li->iteminfo].mins,
                                                ic->items[li->iteminfo].maxs,
#else
                                                ic->items[i].mins,
                                                ic->items[i].maxs,
#endif
                                                li->goalorigin);
            break;
          }
        }
      }
    }
    if ( li )
      continue;
    for ( i = 0; i < ic->numitems; i++ )
    {
      if ( ic->items[i].modelindex == modelindex )
        break;
    }
    if ( i >= ic->numitems )
      continue;
    li = (levelitem_t *)AllocLevelItem();
    li->entitynum = ent;
    li->number = numlevelitems + ent;
    li->iteminfo = i;
    VectorCopy(entinfo.origin, li->origin);
    li->areanum = AAS_BestReachableArea(li->origin, ic->items[i].mins, ic->items[i].maxs, li->goalorigin);
    li->timeout = AAS_Time() + 30;
    AddLevelItemToList(li);
  }
}

// gladiator.dll: 1002FD40..1002FD80
// gladi386.so:   0003F198..0003F22F
void __cdecl BotDumpGoalStack(bot_goalstate_t *goalstate)
{
  int i; // esi

  for ( i = 1; i <= goalstate->goalstacktop; i++ )
  {
    Log_Write("%d: %s", i, BotGoalName(goalstate->goalstack[i].number));
  }
}

// gladiator.dll: 1002FD90..1002FDDB
// gladi386.so:   0003F230..0003F328
void __cdecl BotPushGoal(bot_goalstate_t *gs, const void *goal)
{
  if ( gs->goalstacktop >= 7 )
  {
    botimport.Print(PRT_ERROR, "goal heap overflow\n");
    BotDumpGoalStack(gs);
    return;
  }
  gs->goalstacktop++;
  memcpy(&gs->goalstack[gs->goalstacktop], goal, sizeof(bot_goal_t));
}

// gladiator.dll: 1002FE00..1002FE16
// gladi386.so:   0003F328..0003F33E
int __cdecl BotPopGoal(bot_goalstate_t *goalstate)
{
  int result; // eax

  result = goalstate->goalstacktop;
  if ( result > 0 )
    goalstate->goalstacktop = --result;
  return result;
}

// gladiator.dll: 1002FE30..1002FE3F
// gladi386.so:   0003F340..0003F34F
void __cdecl BotEmptyGoalStack(bot_goalstate_t *goalstate)
{
  goalstate->goalstacktop = 0;
}

// gladiator.dll: 1002FE50..1002FE6D
// gladi386.so:   0003F350..0003F36F
void *__cdecl BotGetTopGoal(bot_goalstate_t *goalstate)
{
  int result; // eax

  result = goalstate->goalstacktop;
  if ( !result )
    return (void *)(intptr_t)result;
  return &goalstate->goalstack[result];
}

// gladiator.dll: 1002FE80..1002FEA0
// gladi386.so:   0003F370..0003F38F
void *__cdecl BotGetSecondGoal(bot_goalstate_t *goalstate)
{
  int v1; // eax

  v1 = goalstate->goalstacktop;
  if ( v1 <= 1 )
    return 0;
  return &goalstate->goalstack[v1 - 1];
}

// gladiator.dll: 1002FEB0..1003019A
// gladi386.so:   0003F390..0003F7E0
int __cdecl BotChooseLTGItem(bot_goalstate_t *goalstate, vec3_t origin, char *inventory, int travelflags)
{
  int hazard;  /* dkbot */
  /* Spill-slot order, measured with slotmap.py --refs: gcc 2.7 assigns the spill slots
   * of register-candidate scalars in DECLARATION order, and the ELF original has them
   * areanum > bestweight > avoidtime > ic > li > bestitem.  The rest never leave
   * registers, so their position is inert; `goal` is the only address-taken local. */
  int areanum; // [esp+14h] [ebp-44h]
  float bestweight; // [esp+60h] [ebp+8h]
  float avoidtime; // [esp+68h] [ebp+10h]
  itemconfig_t *ic; // [esp+18h] [ebp-40h]  - 64-bit fix (was int)
  levelitem_t *li; // esi (was v8)
  levelitem_t *bestitem; // [esp+10h] [ebp-48h]
  int v9; // ebx
  iteminfo_t *iteminfo; // edi (was _DWORD *) - 64-bit fix
  int weightnum; // eax
  float weight; // st7 (was double)
  float v13; // st7 (was double)
  int t; // [esp+1Ch] [ebp-3Ch]
  float v19; // [esp+5Ch] [ebp+4h]
  bot_goal_t goal; // [esp+20h] [ebp-38h] BYREF
  /* 64-bit fix: see BotChooseNBGItem — itemweightconfig/itemweightindex live in sideband. */
#if BOTLIB_NEED_SIDEBAND
  bot_state_t *bs = (bot_state_t *)((char *)goalstate - offsetof(bot_state_t, goalstate));
  weightconfig_t *p0 = (weightconfig_t *)BotGoalP0(bs);
  int *p1 = (int *)BotGoalP1(bs);
#define LTG_IWC p0
#define LTG_IWI p1
#else
#define LTG_IWC (*(weightconfig_t **)&goalstate->itemweightconfig)
#define LTG_IWI (*(int **)&goalstate->itemweightindex)
#endif

  if ( !LTG_IWC )
    return 0;
  hazard = DK_OriginInHazard(origin);  /* dkbot */
  if ( hazard )
    travelflags |= TFL_LAVA | TFL_SLIME;
  areanum = BotReachabilityArea(origin, !AAS_Swimming(origin));
  if ( !areanum )
    return 0;
  if ( !AAS_AreaReachability(areanum) )
    return 0;
  ic = itemconfig;
  if ( !itemconfig )
    return 0;
  bestweight = 0.0;
  bestitem = 0;
  memset(&goal, 0, sizeof(goal));
  for ( li = levelitems; li; li = li->next )
  {
    if ( BotAvoidGoalTime(goalstate, li->number) <= 0.0f )
    {
      v9 = li->areanum;
      if ( v9 && !(hazard && DK_GOAL_IN_TOXIC(v9)) && !dk_goal_unready(ic, li) )
      {
        iteminfo = &ic->items[li->iteminfo];
        weightnum = LTG_IWI[iteminfo->number];
        if ( weightnum >= 0 )
        {
          weight = FuzzyWeightUndecided(inventory, &LTG_IWC->weights[weightnum]);
          if ( weight > 0.0f )
          {
            t = (unsigned __int16)AAS_AreaTravelTimeToGoalArea(areanum, v9, travelflags);
            if ( t > 0 )
            {
              v19 = weight;
              v13 = v19 / ((float)t * 0.01);
              if ( li->timeout != 0.0f )
                v13 = v13 + 20.0f;
              if ( v13 > bestweight )
              {
                bestweight = v13;
                VectorCopy(li->goalorigin, goal.origin);
                VectorCopy(iteminfo->mins, goal.mins);
                VectorCopy(iteminfo->maxs, goal.maxs);
                goal.areanum = v9;
                goal.entitynum = li->entitynum;
                goal.number = li->number;
                goal.flags = GFL_ITEM;
                goal.iteminfo = li->iteminfo;
                bestitem = li;
              }
            }
          }
        }
      }
    }
  }
  if ( !bestitem )
  {
    /* Thunk 0x10001A64 goes to the 4-arg AAS_RandomGoalArea, NOT to
     * BotFindWayPoint. */
    if ( AAS_RandomGoalArea(areanum, travelflags, (_DWORD *)&goal.areanum, goal.origin) )
    {
      goal.mins[0] = -15;
      goal.mins[1] = -15;
      goal.mins[2] = -15;
      goal.maxs[0] = 15;
      goal.maxs[1] = 15;
      goal.maxs[2] = 15;
      goal.entitynum = 0;
      goal.number = 0;
      goal.flags = GFL_ROAM;
      goal.iteminfo = 0;
      BotPushGoal(goalstate, &goal);
      return 1;
    }
    return 0;
  }
  avoidtime = ic->items[bestitem->iteminfo].respawntime;
  if ( avoidtime == 0.0f )
    avoidtime = 30.0f;
  BotAddToAvoidGoals(goalstate, bestitem->number, avoidtime);
  BotPushGoal(goalstate, &goal);
  return 1;
}

#undef LTG_IWC
#undef LTG_IWI
// gladiator.dll: 10030260..1003053B
// gladi386.so:   0003F7E0..0003FC58
int __cdecl BotChooseNBGItem(bot_goalstate_t *goalstate, vec3_t origin, char *inventory, int travelflags, bot_goal_t *ltg, float maxtime)
{
  int hazard;  /* dkbot */
  /* Spill-slot order, measured with slotmap.py --refs: gcc 2.7 assigns the spill slots
   * of register-candidate scalars in DECLARATION order, and the ELF original has them
   * areanum > ltg_time > bestweight > avoidtime > ic > li > bestitem.  The remaining
   * locals never leave registers, so their position is inert; `goal` is in the
   * address-taken group and is the only member of it. */
  int areanum; // [esp+18h] [ebp-40h]
  int ltg_time; // [esp+60h] [ebp+8h]
  float bestweight; // [esp+Ch] [ebp-4Ch]
  float avoidtime; // [esp+68h] [ebp+10h]
  itemconfig_t *ic; // 64-bit fix - typed copy of itemconfig
  levelitem_t *li; // esi (was v10)
  levelitem_t *bestitem; // [esp+10h] [ebp-48h]
  int v9; // esi
  int v11; // ebx
  iteminfo_t *iteminfo; // edi (was _DWORD *) - 64-bit fix
  int weightnum; // eax
  float weight; // st7
  float v15; // st7
  int v16; // eax
  int t; // [esp+1Ch] [ebp-3Ch]
  bot_goal_t goal; // [esp+20h] [ebp-38h] BYREF
  /* 64-bit fix: see BotChooseLTGItem — itemweightconfig/itemweightindex live in sideband. */
#if BOTLIB_NEED_SIDEBAND
  bot_state_t *bs = (bot_state_t *)((char *)goalstate - offsetof(bot_state_t, goalstate));
  weightconfig_t *p0 = (weightconfig_t *)BotGoalP0(bs);
  int *p1 = (int *)BotGoalP1(bs);
#define NBG_IWC p0
#define NBG_IWI p1
#else
#define NBG_IWC (*(weightconfig_t **)&goalstate->itemweightconfig)
#define NBG_IWI (*(int **)&goalstate->itemweightindex)
#endif

  if ( !NBG_IWC )
    return 0;
  hazard = DK_OriginInHazard(origin);  /* dkbot */
  if ( hazard )
    travelflags |= TFL_LAVA | TFL_SLIME;
  areanum = BotReachabilityArea(origin, !AAS_Swimming(origin));
  v9 = areanum;
  if ( !areanum )
    return 0;
  if ( !AAS_AreaReachability(areanum) )
    return 0;
  //get the long term goal travel time
  if ( ltg ) ltg_time = (unsigned __int16)AAS_AreaTravelTimeToGoalArea(v9, ltg->areanum, travelflags);
  else ltg_time = 99999;
  ic = itemconfig;
  if ( !ic )
    return 0;
  bestweight = 0.0;
  bestitem = 0;
  memset(&goal, 0, sizeof(goal));
  for ( li = levelitems; li; li = li->next )
  {
          if ( BotAvoidGoalTime(goalstate, li->number) <= 0.0f )
          {
            v11 = li->areanum;
            if ( v11 && !(hazard && DK_GOAL_IN_TOXIC(v11)) && !dk_goal_unready(ic, li) )
            {
              iteminfo = &ic->items[li->iteminfo];
              weightnum = NBG_IWI[iteminfo->number];
              if ( weightnum >= 0 )
              {
                weight = FuzzyWeightUndecided(inventory, &NBG_IWC->weights[weightnum]);
                if ( weight > 0.0f )
                {
                  t = (unsigned __int16)AAS_AreaTravelTimeToGoalArea(areanum, v11, travelflags);
                  if ( t > 0 )
                  {
                    v15 = (float)t;
                    if ( v15 < maxtime )
                    {
                      weight = weight / (v15 * 0.01);
                      if ( li->timeout != 0.0f )
                        weight = weight + 20.0f;
                      if ( weight > bestweight )
                      {
                        v16 = 0;
                        if ( ltg )
                          v16 = (unsigned __int16)AAS_AreaTravelTimeToGoalArea(v11, ltg->areanum, travelflags);
                        if ( v16 <= ltg_time )
                        {
                          /* The two scalar stores are NOT interleaved into the vec copy in
                           * the source — the interleaved emission is MSVC6 filling
                           * load-ahead bubbles, since both read the same `li`/`weight`
                           * the copy uses. */
                          bestweight = weight;
                          VectorCopy(li->goalorigin, goal.origin);
                          VectorCopy(iteminfo->mins, goal.mins);
                          VectorCopy(iteminfo->maxs, goal.maxs);
                          goal.areanum = v11;
                          goal.entitynum = li->entitynum;
                          goal.number = li->number;
                          goal.flags = GFL_ITEM;
                          goal.iteminfo = li->iteminfo;
                          bestitem = li;
                        }
                      }
                    }
                  }
                }
              }
            }
          }
  }
  if ( !bestitem )
    return 0;
  avoidtime = ic->items[bestitem->iteminfo].respawntime;
  if ( avoidtime == 0.0f )
    avoidtime = 30.0f;
  BotAddToAvoidGoals(goalstate, bestitem->number, avoidtime);
  BotPushGoal(goalstate, &goal);
  return 1;
}

#undef NBG_IWC
#undef NBG_IWI
// gladiator.dll: 10030600..10030715
// gladi386.so:   0003FC58..0003FDE2
int __cdecl BotTouchingGoal(vec3_t origin, float *goal)
{
  int i; // edx
  vec3_t boxmins;     // [esp+14h] [ebp-24h] BYREF
  vec3_t boxmaxs;     // [esp+20h] [ebp-18h] BYREF
  vec3_t absmins;     // [esp+2Ch] [ebp-Ch] BYREF
  vec3_t absmaxs;     // merged by MSVC6 with the same stack home pattern as Q3
  vec3_t safety_maxs; // keep the Gladiator binary's safety shrink values, not Q3's later zeroes
  vec3_t safety_mins;

  safety_maxs[0] = 4.0f;
  safety_maxs[1] = 4.0f;
  safety_maxs[2] = 10.0f;
  safety_mins[0] = -4.0f;
  safety_mins[1] = -4.0f;
  safety_mins[2] = 0.0f;
#if GLAD_SERVERFIX /* GLAD_SERVERFIX(aas-presence-bbox-swapped) */
  /* The 1999 PRESENCE_CROUCH got the standing box only through the swapped mapping.
   * That is the box Q3 asks for here, with PRESENCE_NORMAL, so ask for it the same way. */
  AAS_PresenceTypeBoundingBox(PRESENCE_NORMAL, boxmins, boxmaxs);
#else
  AAS_PresenceTypeBoundingBox(PRESENCE_CROUCH, boxmins, boxmaxs);
#endif
  VectorSubtract((goal + 4), boxmaxs, absmins);
  VectorSubtract((goal + 7), boxmins, absmaxs);
  VectorAdd(absmins, goal, absmins);
  VectorAdd(absmaxs, goal, absmaxs);
  VectorSubtract(absmaxs, safety_maxs, absmaxs);
  VectorSubtract(absmins, safety_mins, absmins);
  for ( i = 0; i < 3; i++ )
  {
    if ( origin[i] < absmins[i] || origin[i] > absmaxs[i] )
      return 0;
  }
  return 1;
}

// gladiator.dll: 10030770..10030873
// gladi386.so:   0003FDE4..0003FECA
/* True when the goal item's bbox centre is visible from the bot's eye but the goal
 * entity is not currently being updated by the engine.  `trace` and `entinfo` have
 * non-overlapping lifetimes, so MSVC6 coalesces them into one stack slot.
 *
 * Three divergences from Q3, all faithful to the binary:
 *   - `entitynum <= 0` returns 1, where Q3 returns qfalse;
 *   - the "not updated" test is the older `if (!entinfo.valid)`, which survives
 *     commented out in Q3, whose live code uses `ltime < AAS_Time() - 0.5`;
 *   - the content mask is Q2's MASK_SOLID, CONTENTS_SOLID|CONTENTS_WINDOW, where
 *     Q3 passes CONTENTS_SOLID.
 * `viewangles` is unused, present only to match the engine call signature. */
BOOL __cdecl BotItemGoalInVisButNotVisible(int viewer, vec3_t eye, vec3_t viewangles, bot_goal_t *goal)
{
  aas_entityinfo_t entinfo; // [ebp-7Ch] BYREF — coalesced with `trace`
  bsp_trace_t trace;        // [ebp-7Ch] BYREF
  vec3_t middle;            // [ebp-88h] world-space goal centre

  if ( !(goal->flags & GFL_ITEM) )
    return 0;
  VectorAdd(goal->mins, goal->mins, middle);
  VectorScale(middle, 0.5, middle);
  VectorAdd(goal->origin, middle, middle);
  trace = AAS_Trace(eye, NULL, NULL, middle, viewer, MASK_SOLID);
  if ( trace.fraction >= 1.0f )
  {
    if ( goal->entitynum <= 0 )
      return 1;
    entinfo = AAS_EntityInfo(goal->entitynum);
    if ( !entinfo.valid )
      return 1;
  }
  return 0;
}

// gladiator.dll: 100308D0..10030921
// gladi386.so:   0003FECC..0003FFBB
int __cdecl BotLoadItemWeights(bot_goalstate_t *goalstate, char *filename)
{
  //load the weight configuration
  BotGoalHandleP0(goalstate) = ReadWeightConfig(filename);
  if ( !BotGoalHandleP0(goalstate) )
  {
    botimport.Print(PRT_FATAL, "couldn't load weights\n");
    return BLERR_CANNOTLOADITEMWEIGHTS;
  }
  //if there's no item configuration
  if ( !itemconfig )
    return BLERR_CANNOTLOADITEMWEIGHTS;
  //create the item weight index
  BotGoalHandleP1(goalstate) = ItemWeightIndex(BotGoalHandleP0(goalstate), itemconfig);
  //everything went ok
  return BLERR_NOERROR;
}

// gladiator.dll: 10030950..10030974
// gladi386.so:   0003FFBC..0003FFF0
void __cdecl BotFreeItemWeights(bot_goalstate_t *goalstate)
{
  if ( BotGoalHandleP0(goalstate) )
    FreeWeightConfig2((weightconfig_t *)BotGoalHandleP0(goalstate));
  if ( BotGoalHandleP1(goalstate) )
    FreeMemory(BotGoalHandleP1(goalstate));
}

// gladiator.dll: 10030990..100309B2
// gladi386.so:   0003FFF0..0004004B
void __cdecl BotResetGoalState(bot_goalstate_t *goalstate)
{
  memset(goalstate->goalstack, 0, sizeof(goalstate->goalstack));
  goalstate->goalstacktop = 0;
  BotResetAvoidGoals(goalstate);
}

// gladiator.dll: 100309D0..10030A0A
// gladi386.so:   0004004C..000400A8
int BotSetupGoalAI()
{
  char *filename; // eax

  filename = LibVarString("itemconfig", (char *)"items.c");
  itemconfig = LoadItemConfig(filename);
  if ( !itemconfig )
  {
    botimport.Print(PRT_FATAL, "couldn't load item config\n");
    return BLERR_CANNOTLOADITEMCONFIG;
  }
  return BLERR_NOERROR;
}

// gladiator.dll: 10030A20..10030A3D
// gladi386.so:   000400A8..000400D8
void BotShutdownGoalAI(void)
{
  if ( itemconfig )
    FreeMemory(itemconfig);
  itemconfig = 0;
}
