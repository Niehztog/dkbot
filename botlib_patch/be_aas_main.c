/*
 * be_aas_main.c — Gladiator Bot v0.96 botlib (Mr. Elusive, 1999), reconstructed
 * from the Windows gladiator.dll.  DLL extent 0x1000D7E0..0x1000EE30.
 */

#include "botlib_port.h"
#include <errno.h>  /* the .so's AAS_LoadFiles calls __errno_location; see botlib_port.h */
#ifdef _WIN32
#include <process.h>  /* _spawnl, which sub_1000E140 starts WinBSPC with */
#include <direct.h>   /* _getcwd and _chdir, which sub_1000E430 moves the process with */
#endif
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
#include "be_aas_main.h"
#include "be_aas_bspq2.h"
#include "be_aas_cluster.h"
#include "be_aas_entity.h"
#include "be_aas_file.h"
#include "be_aas_light.h"
#include "be_aas_optimize.h"
#include "be_aas_reach.h"
#include "be_aas_route.h"
#include "be_aas_routealt.h"
#include "be_aas_sample.h"
#include "be_aas_sound.h"
#include "be_interface.h"
#include "l_libvar.h"
#include "l_log.h"
#include "l_memory.h"
#include "l_utils.h"

/* dkbot: the map the engine loaded (dk_maps.c) */
int DK_FindBSP(const char *mapname, char *path, int size);

#if BOTLIB_NEED_SIDEBAND
aas_link_t **aasentity_arealinks;
bsp_link_t **aasentity_bsplinks;
#else
#endif

aas_world_t aasworld;

// gladiator.dll: 1000D7E0..1000D811
// gladi386.so:   000181F8..00018240
int AAS_Error(char *Format, ...)
{
  char Buffer[1024]; // [esp+0h] [ebp-400h] BYREF
  va_list va; // [esp+408h] [ebp+8h] BYREF

  va_start(va, Format);
  vsprintf(Buffer, Format, va);
  return botimport.Print(PRT_FATAL, Buffer);
}

// gladiator.dll: 1000D830..1000D8AF
// gladi386.so:   00018240..000182D7
char *__cdecl AAS_StringFromIndex(const char *indexname, indexlist_t *list, int index)
{
  if ( !aasworld.indexes_loaded )
  {
    botimport.Print(PRT_ERROR, "%s: index %d not setup\n", indexname, index);
    return "";
  }
  if ( index < 0 || index >= list->numindexes )
  {
    botimport.Print(PRT_ERROR, "%s: index %d out of range\n", indexname, index);
    return "";
  }
  if ( !list->indexes[index] )
  {
    if ( index )
    {
      botimport.Print(PRT_ERROR, "%s: reference to unused index %d\n", indexname, index);
    }
    return "";
  }
  return list->indexes[index];
}

// gladiator.dll: 1000D8D0..1000D936
// gladi386.so:   000182D8..00018358
int __cdecl AAS_IndexFromString(const char *indexname, indexlist_t *list, char *String2)
{
  int i; // esi
  const char *v5; // eax

  if ( !aasworld.indexes_loaded )
  {
    botimport.Print(PRT_ERROR, "%s: index not setup \"%s\"\n", indexname, String2);
    return 0;
  }
  for ( i = 0; i < list->numindexes; i++ )
  {
    v5 = list->indexes[i];
    if ( v5 && !_strcmpi(v5, String2) )
      return i;
  }
  return 0;
}

// gladiator.dll: 1000D960..1000D97A
// gladi386.so:   00018358..000183F3
char *__cdecl AAS_ModelFromIndex(int index)
{
  return AAS_StringFromIndex("ModelFromIndex", aasworld.modelindex_table, index);
}

// gladiator.dll: 1000D990..1000D9AA
// gladi386.so:   000183F4..00018478
/* Q3 be_aas_main.c's AAS_IndexFromModel -- Q3 passes the same "IndexFromModel"
 * label (the string is in both originals; keep it). */
int __cdecl AAS_IndexFromModel(char *modelname)
{
  return AAS_IndexFromString("IndexFromModel", aasworld.modelindex_table,
                             modelname);
}

// gladiator.dll: 1000D9C0..1000D9DA
// gladi386.so:   00018478..00018513
// Pushes "SoundFromIndex" + aasworld.soundindex_table,
// tail-calls AAS_StringFromIndex thunk at 0x10001E01 -> 0x1000D830.
char *__cdecl AAS_SoundFromIndex(int index)
{
  return AAS_StringFromIndex("SoundFromIndex", aasworld.soundindex_table, index);
}

// gladiator.dll: 1000D9F0..1000DA0A
// gladi386.so:   00018514..00018598
// Mirror of AAS_IndexFromModel against the
// soundindex_table; tail-calls AAS_IndexFromString thunk at 0x100012C1.
int __cdecl AAS_IndexFromSound(char *String2)
{
  return AAS_IndexFromString("IndexFromSound", aasworld.soundindex_table,
                             String2);
}

// gladiator.dll: 1000DA20..1000DA3A
// gladi386.so:   00018598..00018633
char *__cdecl AAS_ImageFromIndex(int index)
{
  return AAS_StringFromIndex("ImageFromIndex", aasworld.imageindex_table, index);
}

// gladiator.dll: 1000DA50..1000DA6A
// gladi386.so:   00018634..000186B8
// Mirror of AAS_IndexFromModel against the
// imageindex_table; tail-calls AAS_IndexFromString thunk at 0x100012C1.
int __cdecl AAS_IndexFromImage(char *String2)
{
  return AAS_IndexFromString("IndexFromImage", aasworld.imageindex_table,
                             String2);
}

// gladiator.dll: 1000DA80..1000DB06
// gladi386.so:   000186B8..00018A1E
indexlist_t *__cdecl sub_1000DA80(int numindexes, char **names)
{
  indexlist_t *list; // ebp
  int v4; // ebx
  const char *v5; // edi

  list = (indexlist_t *)GetClearedMemory(sizeof(indexlist_t) + numindexes * sizeof(char *));
  list->indexes = (char **)(list + 1);
  list->numindexes = numindexes;
  for ( v4 = 0; v4 < numindexes; ++v4 )
  {
    v5 = names[v4];
    if ( v5 )
    {
      list->indexes[v4] = (char *)GetMemory(strlen(v5) + 1);
      strcpy(list->indexes[v4], names[v4]);
    }
  }
  return list;
}

// gladiator.dll: 1000DB40..1000DBB0
// gladi386.so:   00018A20..00018BCE
void __cdecl sub_1000DB40(indexlist_t *list, int numindexes, char **names)
{
  int i; // ebx
  const char *v5; // edi

  for ( i = 0; i < numindexes; ++i )
  {
    v5 = names[i];
    if ( v5 )
    {
      if ( !list->indexes[i] )
      {
        list->indexes[i] = (char *)GetMemory(strlen(v5) + 1);
        strcpy(list->indexes[i], names[i]);
      }
    }
  }
  { (void)(numindexes); return; }
}

// gladiator.dll: 1000DBD0..1000DC03
// gladi386.so:   00018BD0..00018C11
int __cdecl sub_1000DBD0(indexlist_t *list)
{
  int i; // esi

  for ( i = 0; i < list->numindexes; ++i )
  {
    if ( list->indexes[i] )
      FreeMemory(list->indexes[i]);
  }
  return FreeMemory(list);
}

// gladiator.dll: 1000DC20..1000DCA0
// gladi386.so:   00018C14..000199D5
void __cdecl sub_1000DC20(int a1, char **a2, int a3, char **a4, int a5, char **a6)
{
  if ( aasworld.modelindex_table )
    sub_1000DBD0(aasworld.modelindex_table);
  if ( aasworld.soundindex_table )
    sub_1000DBD0(aasworld.soundindex_table);
  if ( aasworld.imageindex_table )
    sub_1000DBD0(aasworld.imageindex_table);
  aasworld.modelindex_table = sub_1000DA80(a1, a2);
  aasworld.soundindex_table = sub_1000DA80(a3, a4);
  aasworld.imageindex_table = sub_1000DA80(a5, a6);
  aasworld.indexes_loaded = 1;
}

// gladiator.dll: 1000DCC0..1000DD6E
// gladi386.so:   000199D8..0001AE25
void __cdecl sub_1000DCC0(int a1, char **a2, int a3, char **a4, int a5, char **a6)
{
  if ( aasworld.modelindex_table )
    sub_1000DB40(aasworld.modelindex_table, a1, a2);
  else
    aasworld.modelindex_table = sub_1000DA80(a1, a2);
  if ( aasworld.soundindex_table )
    sub_1000DB40(aasworld.soundindex_table, a3, a4);
  else
    aasworld.soundindex_table = sub_1000DA80(a3, a4);
  if ( aasworld.imageindex_table )
    sub_1000DB40(aasworld.imageindex_table, a5, a6);
  else
    aasworld.imageindex_table = sub_1000DA80(a5, a6);
  aasworld.indexes_loaded = 1;
}

// gladiator.dll: 1000DDA0..1000DE97
// gladi386.so:   0001AE28..0001AEE1
/* Q3's AAS_PresenceTypeBoundingBox.  The Q2 player bbox is 32x32 (-16..16), not Q3's
 * 30x30.  PRESENCE_NORMAL (2) and PRESENCE_CROUCH (4) mean what they mean in Q3: the
 * .aas bbox lump stores 2 with maxs z 32 and 4 with maxs z 4 plus the crouch flag,
 * BotEntityInfo sets PRESENCE_CROUCH only for PMF_DUCKED, and AAS_AreaCrouch calls an
 * area without PRESENCE_NORMAL crouch-only.  This function alone maps them the other
 * way round, in both originals (`cmp eax,4` selects row 1): PRESENCE_CROUCH gets the
 * standing box and PRESENCE_NORMAL the crouch box, so a standing bot is tested with
 * the crouch box.  GLAD_SERVERFIX(aas-presence-bbox-swapped) builds Q3's mapping.
 * Declared int (unlike Q3's void) but never returns a value; it falls off the end,
 * leaving whatever the index*12 array-offset arithmetic last left in eax. */
int __cdecl AAS_PresenceTypeBoundingBox(int presencetype, vec3_t mins, vec3_t maxs)
{
  int    index;
  vec3_t boxmins[3] = { {0.0f, 0.0f, 0.0f}, {-16.0f, -16.0f, -24.0f}, {-16.0f, -16.0f, -24.0f} };
  vec3_t boxmaxs[3] = { {0.0f, 0.0f, 0.0f}, { 16.0f,  16.0f,  32.0f}, { 16.0f,  16.0f,   8.0f} };

#if GLAD_SERVERFIX /* GLAD_SERVERFIX(aas-presence-bbox-swapped) */
  if ( presencetype == PRESENCE_NORMAL )
    index = 1;
  else if ( presencetype == PRESENCE_CROUCH )
    index = 2;
#else
  if ( presencetype == PRESENCE_CROUCH )
    index = 1;
  else if ( presencetype == PRESENCE_NORMAL )
    index = 2;
#endif
  else
  {
    botimport.Print(PRT_FATAL,
             "AAS_PresenceTypeBoundingBox: unknown presence type\n");
    index = 2;
  }
  VectorCopy(boxmins[index], mins);
  VectorCopy(boxmaxs[index], maxs);
}

// gladiator.dll: 1000DEE0..1000DEE6
// gladi386.so:   0001AEE4..0001AEFC
int AAS_Initialized()
{
  return aasworld.initialized;
}

// gladiator.dll: 1000DF00..1000DF1B
// gladi386.so:   0001AEFC..0001AF2F
// Sets aasworld.initialized = 1, then prints "AAS initialized." at level 1.  Called
// from AAS_ContinueInit's tail.  Returns bi_Print's value; Q3 declares it void.
int __cdecl AAS_SetInitialized(void)
{
  aasworld.initialized = 1;
  return botimport.Print(PRT_MESSAGE, "AAS initialized.\n");
}

// gladiator.dll: 1000DF30..1000DFD4
// gladi386.so:   0001AF30..0001B057
/* void, and three guard clauses -- NOT IDA's `int` with a `result` alias and
 * nested ifs.  Real sets no return value on ANY path: all three early exits
 * jump to one shared `pop ebx; pop esi; add esp,8; ret` with eax untouched,
 * and the inlined AAS_SetInitialized tail falls into the same block.  The one
 * caller (AAS_StartFrame, below) discards the value.  IDA's `result =
 * aasworld.loaded;` also forces a LOAD where real compares in memory
 * (`cmp [eax],0x0`), which is 2 of the 7 instructions this cost; the surplus
 * exit block is the other 5.  (2026-08-16; both are catalogued classes --
 * invented non-void return, and the IDA `result` alias.) */
void AAS_ContinueInit(float time)
{
  if ( !aasworld.loaded )
    return;
  if ( aasworld.initialized )
    return;
  if ( AAS_ContinueInitReachability(time) )
    return;
  AAS_InitClustering();
  if ( aasworld.savefile || (unsigned int)(int)LibVarGetValue("forcewrite") )
  {
    if ( !(unsigned int)(int)LibVarGetValue("nooptimize") )
      AAS_Optimize();
    if ( AAS_WriteAASFile(aasworld.filename) )
      botimport.Print(PRT_MESSAGE, "%s written succesfully\n", aasworld.filename);
    else
      botimport.Print(PRT_ERROR, "couldn't write %s\n", aasworld.filename);
  }
  AAS_InitRouting();
  AAS_SetInitialized();
}

// gladiator.dll: 1000E010..1000E0D6
// gladi386.so:   0001B058..0001B150
int AAS_StartFrame(float time)
{
  aasworld.time = time;
  AAS_InvalidateEntities();
  sub_1001CFA0(time);
  sub_1000D4E0(time);
  AAS_ContinueInit(time);
  aasworld.frameroutingupdates = 0;
  if ( LibVarGetValue("showcacheupdates") != 0.0f )
  {
    AAS_RoutingInfo();
    LibVarSet("showcacheupdates", (char *)"0");
  }
  if ( LibVarGetValue("showmemoryusage") != 0.0f )
  {
    PrintUsedMemorySize();
    LibVarSet("showmemoryusage", (char *)"0");
  }
  if ( LibVarGetValue("memorydump") != 0.0f )
  {
    PrintMemoryLabels();
    LibVarSet("memorydump", (char *)"0");
  }
  return 0;
}

// gladiator.dll: 1000E120..1000E127
// gladi386.so:   0001B150..0001B168
float AAS_Time()
{
  return aasworld.time;
}

#ifdef _WIN32  /* ---- winbspc spawn + aasN.zip search: Windows-only ----
                * The Linux botlib has neither: AAS_LoadFiles loads .aas directly and, on
                * failure, just reports "no AAS file available". */
// gladiator.dll: 1000E140..1000E38A
// gladi386.so:   absent
intptr_t __cdecl sub_1000E140(char *Source)
{
  intptr_t result; // eax
  char FileName[144]; // [esp+8h] [ebp-240h] BYREF
  char Arguments[144]; // [esp+98h] [ebp-1B0h] BYREF
  char Destination[144]; // [esp+128h] [ebp-120h] BYREF
  char Buffer[144]; // [esp+1B8h] [ebp-90h] BYREF

  strncpy(Destination, Source, 0x90u);
  strncat(Destination, ".bsp", 144 - strlen(Destination));
  strncpy(FileName, (const char *)LibVarGetString("basedir"), 0x90u);
  AppendPathSeperator(FileName, 144 - strlen(FileName));
  strncat(FileName, (const char *)LibVarGetString("gamedir"), 144 - strlen(FileName));
  AppendPathSeperator(FileName, 144 - strlen(FileName));
  strncpy(Arguments, FileName, 0x90u);
  strncat(FileName, "maps", 144 - strlen(FileName));
  if ( _access(FileName, 4) )
    FileName[strlen(FileName) - 4] = 0;  /* maps dir not accessible: strip the "maps" just appended */
  else
    AppendPathSeperator(FileName, 144 - strlen(FileName));
  strncat(FileName, Source, 144 - strlen(FileName));
  strncat(FileName, ".aas", 144 - strlen(FileName));
  strncat(Arguments, "winbspc.exe", 144 - strlen(Arguments));
  Log_Write("spawning \"%s\"", Arguments);
  sprintf(Buffer, "bsp2aas(%s,%s);", Destination, FileName);
  /* The CRT's own _spawnl (0x10052E44 in the DLL), called directly: mode, the
   * program, argv[0] (the program again), the bsp2aas script, terminator. */
  result = _spawnl(_P_NOWAIT, Arguments, Arguments, Buffer, NULL);
  if ( result < 0 )
    return botimport.Print(PRT_ERROR, "can't execute WinBSPC\n");
  return result;
}

// gladiator.dll: 1000E430..1000E79C
// gladi386.so:   absent
int __cdecl sub_1000E430(char *Source)
{
  const char *v1; // esi
  const char *v2; // ebx
  int v3; // ebp
  char *v4; // ebx
  int i; // esi
  int v7; // esi
  /* Each of these buffers is 144 bytes, so the strncpy/strncat bounds of 144
   * cannot overrun. */
  char Destination[144]; // [esp+10h] [ebp-360h] BYREF
  char ArgList[144]; // [esp+A0h] [ebp-2D0h] BYREF
  char Path[144]; // [esp+130h] [ebp-240h] BYREF
  /* The three search-dir buffers are ONE contiguous char[3][144] array, walked by a
   * char* stepping 144.  All three rows are zero-inited even though only dirs[0]
   * (gamedir) and dirs[1] ("baseq2") are ever populated or searched.  It MUST be a real
   * array, not three named locals: the escaping address is what keeps /O2 from
   * dead-eliminating the unused dirs[2]. */
  char dirs[3][144]; // [esp+1C0h] [ebp-1B0h] BYREF

  strcpy(dirs[0], "");
  strcpy(Destination, "");
  memset(&dirs[0][1], 0, 143);
  strcpy(dirs[1], "");
  memset(&dirs[1][1], 0, 143);
  strcpy(dirs[2], "");
  memset(&dirs[2][1], 0, 143);
  memset(&Destination[1], 0, 143);
  v1 = (const char *)LibVarGetString("basedir");
  v2 = (const char *)LibVarGetString("gamedir");
  if ( v1 && strlen(v1) )
  {
    strncat(Destination, v1, 0x90u);
    AppendPathSeperator(Destination, 144);
  }
  if ( v2 && strlen(v2) )
  {
    strncat(Destination, v2, 144 - strlen(Destination));
    AppendPathSeperator(Destination, 144);
  }
  _getcwd(Path, 144);
  _chdir(Destination);
  if ( v2 )
    strncpy(dirs[0], v2, 0x90u);
  strncpy(dirs[1], "baseq2", 0x90u);
  strncpy(ArgList, Source, 144 - strlen(ArgList));
  strncat(ArgList, ".aas", 144 - strlen(ArgList));
  v3 = 0;
  v4 = dirs[0];
  for ( ; v3 < 2; ++v3, v4 += 144 )
  {
    for ( i = 0; i < 10; ++i )
    {
      strcpy(Destination, "..");
      AppendPathSeperator(Destination, 144);
      if ( strlen(v4) )
      {
        strncat(Destination, v4, 144 - strlen(Destination));
        AppendPathSeperator(Destination, 144);
      }
      sprintf(&Destination[strlen(Destination)], "aas%d.zip", i);
      if ( !_access(Destination, 4) )
      {
        Log_Write("searching %s in %s", ArgList, Destination); /* "searching %s in %s" */
        if ( sub_10041240(Destination, ArgList, 0) )
        {
          v7 = AAS_LoadAASFile(ArgList, 0, 0);
          errno = v7;
          if ( errno )
#if GLAD_SERVERFIX /* GLAD_SERVERFIX(aaszip-cwd-leak) */
          {
            /* The 1999 exit skips the _chdir(Path) both other exits make, leaving the
             * whole process in basedir\gamedir.  Restore it first; return the saved
             * status, since a failing _chdir would overwrite errno. */
            _chdir(Path);
            return v7;
          }
#else
            return errno;
#endif
          remove(ArgList);
          botimport.Print(PRT_MESSAGE, "loaded %s\\%s\n", Destination, ArgList);
          Log_Write("found %s in %s", ArgList, Destination); /* "found %s in %s" */
          _chdir(Path);
          return 0;
        }
        Log_Write("could not find %s in %s", ArgList, Destination); /* "could not find %s in %s" */
      }
    }
  }
  /* v4 walks the next dir row (gamedir -> "baseq2"); array contiguity makes this portable */
  _chdir(Path);
  return BLERR_NOAASFILE;
}

#endif /* _WIN32 — winbspc spawn + aasN.zip search */
/* ------------------------------------------------------------------------
 * Present in gladi386.so, ABSENT from gladiator.dll.  See the identical note in
 * be_aas_route.c for why these are gated rather than added outright.
 * ------------------------------------------------------------------------ */
#ifndef _WIN32
/* F184 @ 0x0001b168, 6 bytes — `mov eax,5; ret`, nothing else.  No callers anywhere in
 * the image and no string, constant or call to identify it by, so there is no name to
 * recover and none is invented: the identifier is the symbol gladi386.so ships.  It
 * sits between AAS_Time (F183) and AAS_LoadFiles (F185) — and it is placed there,
 * not at the end of the TU, because gladi386.so's address order records the
 * original definition order. */
// gladiator.dll: absent
// gladi386.so:   0001B168..0001B16E
int __cdecl F184(void)
{
  return 5;
} //end of the function F184

#endif /* !_WIN32 -- gladi386.so-only */

// gladiator.dll: 1000E880..1000EBE2
// gladi386.so:   0001B170..0001B531
/* Q3 be_aas_main.c's AAS_LoadFiles: record the map name, reset the entity links,
 * load the BSP, then the AAS, and print "loaded ...".  Gladiator adds the pak
 * search and the Win32 aasN.zip / winbspc fallbacks. */
int AAS_LoadFiles(char *mapname)
{
  /* Shape read off gladi386.so, and cl.exe produces its own original from it
   * too: the bsp lookup is an if/else whose error arm returns, with the rest
   * AFTER it (not nested in the success arm), and the aas search does its
   * load-and-return INSIDE the `for`, the give-up path after the loop (IDA's
   * `break` out of a while(1) plus the load afterwards matched neither image
   * -- the .so at OUR-30, the DLL at 522 bytes).  Declaration order is the
   * .so's frame: aasfile, bspfile, then the file ref. */
  int i;
  char aasfile[144];
  char bspfile[144];
  bot_fileref_t file_ref;

  strcpy(aasworld.mapname, mapname);
  AAS_ResetEntityLinks();
  memset(&file_ref, 0, sizeof(file_ref));
  strncpy(bspfile, "maps\\", 0x90u);
  strncat(bspfile, mapname, 144 - strlen(bspfile));
  strncat(bspfile, ".bsp", 144 - strlen(bspfile));
  if ( DK_FindBSP(mapname, file_ref.path, sizeof file_ref.path) || FindQuakeFile(bspfile, &file_ref) )  /* dkbot */
  {
    errno = AAS_LoadBSPFile(file_ref.path, file_ref.fileofs, file_ref.filelen);
    if ( errno )
      return errno;
    if ( file_ref.filelen )
      botimport.Print(PRT_MESSAGE, "loaded %s\\%s\n", file_ref.path, bspfile);
    else
      botimport.Print(PRT_MESSAGE, "loaded %s\n", bspfile);
  }
  else
  {
    botimport.Print(PRT_FATAL, "couldn't find the bsp file %s\n", bspfile);
    return BLERR_NOBSPFILE;
  }
  memset(&file_ref, 0, sizeof(file_ref));
  for ( i = 0; i < 2; i++ )
  {
    if ( i )
      strncpy(aasfile, "maps\\", 0x90u);
    else
      strncpy(aasfile, "", 0x90u);
    strncat(aasfile, mapname, 144 - strlen(aasfile));
    strncat(aasfile, ".aas", 144 - strlen(aasfile));
    if ( FindQuakeFile(aasfile, &file_ref) )
    {
      errno = AAS_LoadAASFile(file_ref.path, file_ref.fileofs, file_ref.filelen);
      if ( errno )
        return errno;
      if ( file_ref.fileofs )
        botimport.Print(PRT_MESSAGE, "loaded %s\\%s\n", file_ref.path, aasfile);
      else
        botimport.Print(PRT_MESSAGE, "loaded %s\n", file_ref.path);
      if ( file_ref.fileofs )
        strncpy(aasworld.filename, aasfile, 0x90u);
      else
        strncpy(aasworld.filename, file_ref.path, 0x90u);
      return BLERR_NOERROR;
    }
  }
#ifdef _WIN32
  errno = sub_1000E430(mapname);
  if ( !errno )
    return BLERR_NOERROR;
  if ( LibVarValue("autolaunchbspc", (char *)"0") != 0 )
  {
    sub_1000E140(mapname);
    botimport.Print(
      5,
      "\n"
      "creating AAS for %s...\n"
      "\n"
      "This may take several minutes\n"
      "\n"
      "You cannot play the map %s with\n"
      "bots before AAS (%s.aas) has been\n"
      "created.\n"
      "\n"
      "You probably want to close Quake2 now\n"
      "to free up processing power for the\n"
      "tool which creates the AAS file.\n"
      "\n",
      mapname,
      mapname,
      mapname);
  }
#else
  /* Faithful Linux give-up path: the Linux botlib has no UNZIP32/ZIP32 windll and
   * no winbspc spawn, so it never tries the aasN.zip fallback -- it sets errno=5 and
   * the autolaunchbspc branch only reports that BSPC is a Win32 program. */
  errno = 5;
  if ( LibVarValue("autolaunchbspc", (char *)"0") != 0 )
    botimport.Print(PRT_MESSAGE, "the BSPC tool is a Win32 program\n");
#endif
  botimport.Print(PRT_FATAL, "no AAS file available\n");
  return BLERR_NOAASFILE;
}

// gladiator.dll: 1000ECD0..1000ED81
// gladi386.so:   0001B534..0001B5EB
/* Q3 be_aas_main.c's AAS_LoadMap, step for step: a NULL map name only refreshes the
 * string indexes, otherwise clear `initialized`, free the routing caches,
 * AAS_LoadFiles, then the link heap, linked entities, reachability and alternative
 * routing.  Gladiator's extra arguments are the model/sound/image index tables. */
int __cdecl AAS_LoadMap(char *mapname, int a2, char **a3, int a4, char **a5, int a6, char **a7)
{
  if ( !mapname )
  {
    sub_1000DCC0(a2, a3, a4, a5, a6, a7);
    return BLERR_NOERROR;
  }
  aasworld.initialized = 0;
  sub_1000DC20(a2, a3, a4, a5, a6, a7);
  AAS_FreeRoutingCaches();
  errno = AAS_LoadFiles(mapname);
  if ( errno )
  {
    aasworld.loaded = 0;
    return errno;
  }
  AAS_InitAASLinkHeap();
  AAS_InitAASLinkedEntities();
  AAS_InitReachability();
  sub_1001D140();
  AAS_InitAlternativeRouting();
  return BLERR_NOERROR;
}

// gladiator.dll: 1000EDC0..1000EE0C
// gladi386.so:   0001B5EC..0001B663
/* Q3 be_aas_main.c's AAS_Setup: size and clear the entity array, invalidate every
 * entity, return BLERR_NOERROR -- called from the library setup right after the
 * maxclients/maxentities libvars are read, with `if (err) return err`, exactly Q3's
 * call site.  Gladiator takes the two counts as arguments (Q3 reads the libvars
 * itself) and also initialises the sound pool. */
int __cdecl AAS_Setup(int maxentities, int maxclients)
{
  aasworld.numentities = maxentities;
  aasworld.aas_maxclients = maxclients;
  if ( aasworld.entities )
    FreeMemory(aasworld.entities);
  aasworld.entities = (aas_entity_t *)GetClearedMemory(sizeof(aas_entity_t) * maxentities);
#if BOTLIB_NEED_SIDEBAND
  if ( aasentity_arealinks )
    FreeMemory(aasentity_arealinks);
  aasentity_arealinks = (aas_link_t **)GetClearedMemory(sizeof(aas_link_t *) * maxentities);
  if ( aasentity_bsplinks )
    FreeMemory(aasentity_bsplinks);
  aasentity_bsplinks = (bsp_link_t **)GetClearedMemory(sizeof(bsp_link_t *) * maxentities);
#endif
  sub_1001D260();
  AAS_InvalidateEntities();
  return 0;
}

// gladiator.dll: 1000EE30..1000EE81
// gladi386.so:   0001B664..0001B6DC
int AAS_Shutdown()
{
  AAS_FreeRoutingCaches();
  sub_1001D290();
  AAS_FreeAASLinkHeap();
  AAS_FreeAASLinkedEntities();
  AAS_DumpAASData();
  if ( aasworld.entities )
    FreeMemory(aasworld.entities);
  /* Zero the whole 676-byte state; sizeof(aasworld) == the original's 0x2A4. */
  memset(&aasworld, 0, sizeof(aasworld));
  aasworld.initialized = 0;
  return botimport.Print(PRT_MESSAGE, "AAS shutdown.\n");
}
