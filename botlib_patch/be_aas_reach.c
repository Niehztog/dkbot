/*
 * be_aas_reach.c — Gladiator Bot v0.96 botlib (Mr. Elusive, 1999), reconstructed
 * from the Windows gladiator.dll.  DLL extent 0x10010F60..0x10018C70.
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
#include "be_aas_reach.h"
#include "be_aas_bspq2.h"
#include "be_aas_entity.h"
#include "be_aas_main.h"
#include "be_aas_move.h"
#include "be_aas_sample.h"
#include "be_interface.h"
#include "l_libvar.h"
#include "l_log.h"
#include "l_memory.h"

/* STATIC, and the two names were swapped.  Real reaches both of these GOTOFF
 * (0x5d558 / 0x5d55c, past the end of the GOT and inside .bss) with no
 * `R_386_GLOB_DAT` and no .dynsym entry, which is what a file-static looks
 * like under -fPIC; ours exported them, which is a 2-instruction different
 * access form -- the same class as game/'s LEGACY_STATIC case.  The two names
 * were also swapped against the cvars they cache: the variable called
 * `...reachabilitydelay` held LibVar("framereachability") and was used as the
 * per-cycle AREA COUNT, while `...framereachability` held
 * LibVar("reachability_delay") and was used as the millisecond budget.  Names
 * exchanged; behaviour unchanged.  (globwiring.py, 2026-08-16.) */
static libvar_t *libvar_framereachability;  /* LibVar("framereachability", "20") */
static libvar_t *libvar_reachabilitydelay;  /* LibVar("reachability_delay", "100") */
aas_reachabilitynode_t **areareachability;   /* per-area linked-list-head array */
int reach_ladder; // weak
int reach_elevator; // weak
aas_reachabilitynode_t *reachabilityheap; // pool base
int reach_jump; // weak
int reach_grapple; // weak
int reach_waterjump; // weak
int reach_teleport; // weak
int reach_barrier; // weak
int reach_swim; // weak
int reach_equalfloor; // weak
aas_reachabilitynode_t *nextreachability; // free-list head
int reach_walkoffledge; // weak
int reach_rocketjump; // weak
int reach_step; // weak
int reach_walk; // weak
/* The four reachability-type counters Gladiator never increments.  Recovered
 * from gladi386.so's .dynsym, which exports all four as 4-byte .bss OBJECTs
 * (reach_rampjump @0x60918, reach_bfgjump @0x6091c, reach_doublejump
 * @0x60920, reach_strafejump @0x60930) interleaved with the ones above.  They
 * have NO reference anywhere in the real image either -- the generators for
 * those four travel types are the Q3-era ones Gladiator does not have -- so
 * only the name, size and neighbourhood are evidence; the `int` type is by
 * analogy with the eleven siblings, every one of which is a counter here.
 * (globwiring.py / dataaudit.py, 2026-08-16.)
 *
 * Their ORDER is evidence too.  Being unreferenced, each reaches gladi386.so's
 * .dynsym only through its `.comm` line, which gcc 2.7 writes in definition order at
 * the end of the TU -- and the real image lists doublejump, rampjump, strafejump,
 * bfgjump: Q3's be_aas_reach.c order for exactly these four.  (Their .bss addresses
 * are ld's common allocation, not definition order.) */
int reach_doublejump;  // weak -- unreferenced in BOTH images
int reach_rampjump;    // weak -- unreferenced in BOTH images
int reach_strafejump;  // weak -- unreferenced in BOTH images
int reach_bfgjump;     // weak -- unreferenced in BOTH images

// gladiator.dll: 10010F60..10010FA3
// gladi386.so:   0001DAFC..0001DBC0
/* The reach free-list: 65536 fixed-size nodes, 48 B each with the next-ptr at
 * +44 on 32-bit (56 B / +48 on 64-bit).  Q3's AAS_SetupReachabilityHeap
 * verbatim over typed pointers -- gcc 2.7.2.3 -O6 -funroll-loops unrolls the
 * loop 5x around the i counter, and MSVC6 /O2 eliminates the counter into the
 * byte offset, which is exactly the pair of reference loops. */
#define AAS_REACHABILITYHEAP_NODES 65536

void AAS_SetupReachabilityHeap(void)
{
  int i;

  reachabilityheap = (aas_reachabilitynode_t *) GetClearedMemory(
            AAS_REACHABILITYHEAP_NODES * sizeof(aas_reachabilitynode_t));
  for ( i = 0; i < AAS_REACHABILITYHEAP_NODES - 1; i++ )
  {
    reachabilityheap[i].next = &reachabilityheap[i+1];
  }
  reachabilityheap[AAS_REACHABILITYHEAP_NODES - 1].next = NULL;
  nextreachability = reachabilityheap;
}

// gladiator.dll: 10010FD0..10010FDD
// gladi386.so:   0001DBC0..0001DBE0
void AAS_ShutDownReachabilityHeap()
{
  FreeMemory(reachabilityheap);
}
/* Pop a node off the reach free chain, raising AAS_MAX_REACHABILITYSIZE when the
 * successor is NULL.  A DIFFERENT free list from the entity-link one at
 * aasworld.freelinks (16-byte stride, link at +8). */
int numlreachabilities;               /* 0x1006677C reachabilities allocated (be_aas_reach.c; was dword_1006677C) */

// gladiator.dll: 10010FF0..1001102A
// gladi386.so:   0001DBE0..0001DC2E
void *AAS_AllocReachability(void)
{
  aas_reachabilitynode_t *head;

  if ( !nextreachability )
    return NULL;
  if ( !nextreachability->next )
    AAS_Error("AAS_MAX_REACHABILITYSIZE");
  /* Original re-reads head here in case AAS_Error trashed eax. */
  head = nextreachability;
  nextreachability = head->next;
  ++numlreachabilities;
  return head;
}

// gladiator.dll: 10011040..10011075
// gladi386.so:   0001DC30..0001DC7C
int __cdecl AAS_AreaReachability(int areanum)
{
  if ( areanum < 0 || areanum >= aasworld.numareas )
  {
    AAS_Error("AAS_AreaReachability: areanum %d out of range", areanum);
    return 0;
  }
  return aasworld.areasettings[areanum].numreachableareas;
}

// gladiator.dll: 10011090..100111D0
// gladi386.so:   0001DC7C..0001DE37
float __cdecl AAS_FaceArea(aas_face_t *face)
{
  int i;
  int edgenum;
  int side;
  float total;
  vec_t *v;
  vec3_t d1;
  vec3_t d2;
  vec3_t cross;
  aas_edge_t *edge;

  edgenum = aasworld.edgeindex[face->firstedge];
  side = edgenum < 0;
  edge = &aasworld.edges[abs(edgenum)];
  v = aasworld.vertexes[edge->v[side]];

  total = 0.0f;
  for ( i = 1; i < face->numedges - 1; i++ )
  {
    edgenum = aasworld.edgeindex[face->firstedge + i];
    side = edgenum < 0;
    edge = &aasworld.edges[abs(edgenum)];
    VectorSubtract(aasworld.vertexes[edge->v[side]], v, d1);
    VectorSubtract(aasworld.vertexes[edge->v[!side]], v, d2);
    CrossProduct(d1, d2, cross);
    total += 0.5 * VectorLength(cross);
  }
  return total;
}

// gladiator.dll: 10011220..10011319
// gladi386.so:   0001DE38..0001DF70
float __cdecl AAS_AreaVolume(int areanum)
{
  aas_area_t *area;
  aas_face_t *face;
  aas_edge_t *edge;
  aas_plane_t *plane;
  int i;
  int facenum;
  int edgenum;
  float d;
  float a;
  vec3_t corner;
  float volume;

  area = &aasworld.areas[areanum];
  facenum = aasworld.faceindex[area->firstface];
  face = &aasworld.faces[abs(facenum)];
  edgenum = aasworld.edgeindex[face->firstedge];
  edge = &aasworld.edges[abs(edgenum)];
  VectorCopy(aasworld.vertexes[edge->v[0]], corner);
  volume = 0.0f;
  for ( i = 0; i < area->numfaces; ++i )
  {
    facenum = abs(aasworld.faceindex[area->firstface + i]);
    face = &aasworld.faces[facenum];
    plane = &aasworld.planes[face->planenum];
    d = -(DotProduct(corner, plane->normal)
          - plane->dist);
    a = AAS_FaceArea(face);
    volume += d * a;
  }
  return volume * 0.33333334f;
}

// gladiator.dll: 10011360..100113CE
// gladi386.so:   0001DF70..0001DFF0
float __cdecl AAS_AreaGroundFaceArea(int areanum)
{
  float total; // st7
  int i; // edi
  aas_area_t *area; // esi
  aas_face_t *face;

  total = 0.0f;
  area = &aasworld.areas[areanum];
  for ( i = 0; i < area->numfaces; i++ )
  {
    face = &aasworld.faces[abs(aasworld.faceindex[area->firstface + i])];
    if ( !(face->faceflags & 4) )
      continue;
    total += AAS_FaceArea(face);
  }
  return total;
}

// gladiator.dll: 100113F0..100114D7
// gladi386.so:   0001DFF0..0001E12B
void __cdecl AAS_FaceCenter(int facenum, vec3_t center)
{
  int i; // esi
  aas_face_t *face; // edi
  aas_edge_t *edge;
  float scale; // [esp+0h] [ebp-10h]

  face = &aasworld.faces[facenum];
  VectorClear(center);
  for ( i = 0; i < face->numedges; i++ )
  {
    edge = &aasworld.edges[abs(aasworld.edgeindex[face->firstedge + i])];
    VectorAdd(center, aasworld.vertexes[edge->v[0]], center);
    VectorAdd(center, aasworld.vertexes[edge->v[1]], center);
  }
  scale = 0.5 / face->numedges;
  VectorScale((float *)center, scale, (float *)center);
}

// gladiator.dll: 10011520..10011546
// gladi386.so:   0001E12C..0001E18E
int AAS_FallDamageDistance()
{
  float maxzvelocity, gravity, t;

  maxzvelocity = sqrt(30 * 10000);
  gravity = libvar_sv_gravity->value;
  t = maxzvelocity / gravity;
  return 0.5 * gravity * t * t;
}

// gladiator.dll: 10011560..1001157F
// gladi386.so:   0001E190..0001E1CD
float __cdecl AAS_MaxJumpHeight(float phys_jumpvel)
{
  float phys_gravity;

  phys_gravity = libvar_sv_gravity->value;
  return 0.5 * phys_gravity * (phys_jumpvel / phys_gravity) * (phys_jumpvel / phys_gravity);
}

// gladiator.dll: 10011590..100115BC
// gladi386.so:   0001E1D0..0001E223
float __cdecl AAS_MaxJumpDistance(float phys_jumpvel)
{
  float phys_gravity, phys_maxvelocity, t;

  phys_gravity = libvar_sv_gravity->value;
  phys_maxvelocity = libvar_sv_maxvelocity->value;
  t = sqrt(450.0 / (0.5 * phys_gravity));
  return phys_maxvelocity * (t + phys_jumpvel / phys_gravity);
}

// gladiator.dll: 100115D0..100115F2
// gladi386.so:   0001E224..0001E25B
int __cdecl AAS_AreaCrouch(int areanum)
{
  if ( !(aasworld.areasettings[areanum].presencetype & PRESENCE_NORMAL) )
    return 1;
  else
    return 0;
}

// gladiator.dll: 10011610..1001162F
// gladi386.so:   0001E25C..0001E293
int __cdecl AAS_AreaSwim(int areanum)
{
  /* Keep the if/else 0-or-1 form (Q3's `if (areaflags & AREA_LIQUID)`): MSVC's
   * bit-test idiom emits a byte-narrowed test, whereas an arithmetic `(x&4)>>2`
   * gets reassociated to `(x>>2)&1` on a full DWORD load. */
  if ( aasworld.areasettings[areanum].areaflags & 4 )
    return 1;
  else
    return 0;
}

// gladiator.dll: 10011640..1001165F
// gladi386.so:   0001E294..0001E2CB
/* Q3's AAS_AreaLiquid — a byte-identical duplicate of AAS_AreaSwim above, as
 * in be_aas_reach.c, where both share the same AREA_LIQUID body. */
int __cdecl AAS_AreaLiquid(int areanum)
{
  if ( aasworld.areasettings[areanum].areaflags & 4 )
    return 1;
  else
    return 0;
}

// gladiator.dll: 10011670..1001168B
// gladi386.so:   0001E2CC..0001E2FA
int __cdecl AAS_AreaGrounded(int areanum)
{
  return aasworld.areasettings[areanum].areaflags & 1;
}

// gladiator.dll: 100116A0..100116BB
// gladi386.so:   0001E2FC..0001E32A
int __cdecl AAS_AreaLadder(int areanum)
{
  return aasworld.areasettings[areanum].areaflags & 2;
}

// gladiator.dll: 100116D0..100116EE
// gladi386.so:   0001E32C..0001E37E
/* Q3 be_aas_reach.c's AAS_BarrierJumpTravelTime -- jumpvel / (gravity * 0.1), with
 * Gladiator's sv_ libvars where Q3 has aassettings -- in the same slot, right before
 * AAS_ReachabilityExists.  DEAD in Gladiator, preserved by /INCREMENTAL. */
unsigned __int16 __cdecl AAS_BarrierJumpTravelTime(void)
{
  return libvar_sv_jumpvel->value / (libvar_sv_gravity->value * 0.1);
}

// gladiator.dll: 10011700..10011729
// gladi386.so:   0001E380..0001E3BB
qboolean __cdecl AAS_ReachabilityExists(int area1num, int area2num)
{
  aas_reachabilitynode_t *r;

  r = areareachability[area1num];
  while ( r )
  {
    if ( *(_DWORD *)r == area2num )
      return 1;
    r = r->next;
  }
  return 0;
}

// gladiator.dll: 10011740..10011814
// gladi386.so:   0001E3BC..0001E4B3
BOOL __cdecl AAS_NearbySolidOrGap(vec3_t start, vec3_t end)
{
  int areanum; // eax
  /* One vec3_t, so VectorMA's writes land in the same slot the original bumps by
   * 16.0 after the first AAS_PointAreaNum. */
  vec3_t dir; // [esp+10h] [ebp-Ch] BYREF
  vec3_t testpoint; // [esp+4h] [ebp-18h] BYREF

  dir[0] = *end - *start;
  dir[1] = end[1] - start[1];
  dir[2] = 0;
  VectorNormalize(dir);
  VectorMA(end, 48.0f, dir, testpoint);
  areanum = AAS_PointAreaNum(testpoint);
  if ( !areanum )
  {
    testpoint[2] = testpoint[2] + 16.0f;
    areanum = AAS_PointAreaNum(testpoint);
    if ( !areanum )
      return 1;
  }
  VectorMA(end, 64.0, dir, testpoint);
  areanum = AAS_PointAreaNum(testpoint);
  if ( areanum && !AAS_AreaSwim(areanum) && !AAS_AreaGrounded(areanum) )
    return 1;
  return 0;
}

// gladiator.dll: 10011860..10011A51
// gladi386.so:   0001E4B4..0001E778
int __cdecl AAS_Reachability_Swim(int area1num, int area2num)
{
  int i;
  int j;
  int face1num;
  int face2num;
  int side1;
  aas_area_t *area1;
  aas_area_t *area2;
  aas_areasettings_t *areasettings;
  aas_reachabilitynode_t *lreach;
  aas_face_t *face1;
  aas_plane_t *plane;
  vec3_t start;

  if ( !AAS_AreaSwim(area1num) || !AAS_AreaSwim(area2num) )
    return 0;
  if ( (aasworld.areasettings[area2num].presencetype & PRESENCE_NORMAL) == 0 )
    return 0;
  area1 = &aasworld.areas[area1num];
  area2 = &aasworld.areas[area2num];
  for ( i = 0; i < 3; ++i )
  {
    if ( area1->mins[i] > area2->maxs[i] + 10.0f )
      return 0;
    if ( area1->maxs[i] < area2->mins[i] - 10.0f )
      return 0;
  }
  for ( i = 0; i < area1->numfaces; ++i )
  {
    face1num = aasworld.faceindex[area1->firstface + i];
    side1 = face1num < 0;
    face1num = abs(face1num);
    for ( j = 0; j < area2->numfaces; ++j )
    {
      face2num = abs(aasworld.faceindex[area2->firstface + j]);
      if ( face1num == face2num )
      {
        AAS_FaceCenter(face1num, start);
        if ( AAS_PointContents(start) & 0x38 )   /* water-edge contents check */
        {
          face1 = &aasworld.faces[face1num];
          areasettings = &aasworld.areasettings[area1num];
          lreach = (aas_reachabilitynode_t *)AAS_AllocReachability();
          if ( !lreach )
            return 0;
          lreach->reach.areanum = area2num;
          lreach->reach.facenum = face1num;
          lreach->reach.edgenum = 0;
          VectorCopy(start, lreach->reach.start);
          /* Indexed, not float* arithmetic — aas_plane_t's stride is 20 bytes. */
          plane = &aasworld.planes[face1->planenum ^ side1];
          VectorMA(lreach->reach.start, 2.0f, plane->normal, lreach->reach.end);
          lreach->reach.traveltype = TRAVEL_SWIM;
          lreach->reach.traveltime = 1;
          if ( AAS_AreaVolume(area2num) < 800.0f )
            lreach->reach.traveltime += 200;
          lreach->next = areareachability[area1num];
          areareachability[area1num] = lreach;
          ++reach_swim;
          return 1;
        }
      }
    }
  }
  return 0;
}

// gladiator.dll: 10011AE0..10012081
// gladi386.so:   0001E778..0001EF99
int __cdecl AAS_Reachability_EqualFloorHeight(int area1num, int area2num)
{
  int i;
  int j;
  int edgenum;
  int edgenum1;
  int edgenum2;
  int foundreach;
  int side;
  float height;
  float bestheight;
  float length;
  float bestlength;
  vec3_t start;
  vec3_t end;
  vec3_t normal;
  vec3_t invgravity;
  vec3_t down = { 0, 0, -1 };
  vec3_t edgevec;
  aas_area_t *area1;
  aas_area_t *area2;
  aas_face_t *face1;
  aas_face_t *face2;
  aas_edge_t *edge;
  aas_plane_t *plane2;
  /* Q3's local aas_lreachability_t included the next pointer; keeping the
   * 48-byte node shape preserves the original 32-bit frame layout here. */
  aas_reachabilitynode_t lr;
  aas_reachabilitynode_t *lreach;

  if ( !AAS_AreaGrounded(area1num) || !AAS_AreaGrounded(area2num) )
    return 0;

  area1 = &aasworld.areas[area1num];
  area2 = &aasworld.areas[area2num];

  for ( i = 0; i < 2; ++i )
  {
    if ( area1->mins[i] > area2->maxs[i] + 10.0f )
      return 0;
    if ( area1->maxs[i] < area2->mins[i] - 10.0f )
      return 0;
  }

  if ( area2->mins[2] > area1->maxs[2] )
    return 0;

  VectorCopy(down, invgravity);
  VectorInverse(invgravity);

  bestheight = 99999.0f;
  bestlength = 0.0f;
  foundreach = 0;

  for ( i = 0; i < area1->numfaces; ++i )
  {
    face1 = &aasworld.faces[abs(aasworld.faceindex[area1->firstface + i])];
    if ( !(face1->faceflags & 4) )
      continue;

    for ( j = 0; j < area2->numfaces; ++j )
    {
      face2 = &aasworld.faces[abs(aasworld.faceindex[area2->firstface + j])];
      if ( !(face2->faceflags & 4) )
        continue;

      for ( edgenum1 = 0; edgenum1 < face1->numedges; ++edgenum1 )
      {
        for ( edgenum2 = 0; edgenum2 < face2->numedges; ++edgenum2 )
        {
          if ( abs(aasworld.edgeindex[face1->firstedge + edgenum1]) !=
               abs(aasworld.edgeindex[face2->firstedge + edgenum2]) )
            continue;

          edgenum = aasworld.edgeindex[face1->firstedge + edgenum1];
          side = edgenum < 0;
          edge = &aasworld.edges[abs(edgenum)];

          VectorAdd(aasworld.vertexes[edge->v[0]],
                    aasworld.vertexes[edge->v[1]], start);
          length = VectorLength(start);
          VectorScale(start, 0.5f, start);
          VectorCopy(start, end);

          VectorSubtract(aasworld.vertexes[edge->v[side]],
                         aasworld.vertexes[edge->v[!side]], edgevec);
          plane2 = &aasworld.planes[face2->planenum];
          CrossProduct(edgevec, plane2->normal, normal);
          VectorNormalize(normal);
          VectorMA(end, 5.0f, normal, end);
          VectorMA(start, 0.1f, normal, start);
          end[2] += 0.125;

          height = DotProduct(invgravity, start);
          if ( !AAS_NearbySolidOrGap(start, end) )
            height += 200.0f;

          if ( height < bestheight || (height < bestheight + 1.0f && length > bestlength) )
          {
            bestheight = height;
            bestlength = length;
            lr.reach.areanum = area2num;
            lr.reach.facenum = 0;
            lr.reach.edgenum = edgenum;
            VectorCopy(start, lr.reach.start);
            VectorCopy(end, lr.reach.end);
            lr.reach.traveltype = TRAVEL_WALK;
            lr.reach.traveltime = 1;
            foundreach = 1;
          }
        }
      }
    }
  }

  /* Q3's `if (foundreach) { … return qtrue; } return qfalse;` — the trailing
   * `return 0` is its own exit block in the original.  Written as the negative
   * guard `if (!foundreach) return 0;` gcc cross-jumps that exit into the
   * `!lreach` one and the function comes out 7 instructions short. */
  if ( foundreach )
  {
    lreach = (aas_reachabilitynode_t *)AAS_AllocReachability();
    if ( !lreach )
      return 0;

    lreach->reach.areanum = lr.reach.areanum;
    lreach->reach.facenum = lr.reach.facenum;
    lreach->reach.edgenum = lr.reach.edgenum;
    VectorCopy(lr.reach.start, lreach->reach.start);
    VectorCopy(lr.reach.end, lreach->reach.end);
    lreach->reach.traveltype = lr.reach.traveltype;
    lreach->reach.traveltime = lr.reach.traveltime;
    lreach->next = areareachability[area1num];
    areareachability[area1num] = lreach;

    if ( !AAS_AreaCrouch(area1num) && AAS_AreaCrouch(area2num) )
      lreach->reach.traveltime += 300;
    if ( !AAS_NearbySolidOrGap(lreach->reach.start, lreach->reach.end) )
      lreach->reach.traveltime += 100;
    if ( AAS_AreaGroundFaceArea(lreach->reach.areanum) < 500.0f )
      lreach->reach.traveltime += 100;

    ++reach_equalfloor;
    return 1;
  }
  return 0;
}

// gladiator.dll: 10012200..1001367A
// gladi386.so:   0001EF9C..00020BB5
int __cdecl AAS_Reachability_Step_Barrier_WaterJump_WalkOffLedge(int area1num, int area2num)
{
  /* Q3's text and declarations, less Q3's later additions (the zeroed bestedge
   * numbers, rs_maxfallheight, the cluster-portal trace and fall-damage traveltime),
   * with Gladiator's own step penalties -- the NearbySolidOrGap and small-area +400s
   * Q3 later commented out -- and its fall-damage gate on the walk off ledge.  IDA's
   * version (hoisted comma-expression plane, int-view vector copies, merged
   * temporaries) cost ELF 582 insn-diffs and PE 492 lines; this matches both. */
  int i, j, k, l, edge1num, edge2num;
  int ground_bestarea2groundedgenum, ground_foundreach;
  int water_bestarea2groundedgenum, water_foundreach;
  int side1, area1swim, faceside1, groundface1num;
  float dist, dist1, dist2, diff, invgravitydot, ortdot;
  float x1, x2, x3, x4, y1, y2, y3, y4, tmp, y;
  float length, ground_bestlength, water_bestlength, ground_bestdist, water_bestdist;
  vec3_t v1, v2, v3, v4, tmpv, p1area1, p1area2, p2area1, p2area2;
  vec3_t normal, ort, edgevec, start, end, dir;
  vec3_t ground_beststart, ground_bestend, ground_bestnormal;
  vec3_t water_beststart, water_bestend, water_bestnormal;
  vec3_t invgravity = {0, 0, 1};
  vec3_t testpoint;
  aas_plane_t *plane;
  aas_area_t *area1, *area2;
  aas_face_t *groundface1, *groundface2, *ground_bestface1, *water_bestface1;
  aas_edge_t *edge1, *edge2;
  aas_reachabilitynode_t *lreach;
  aas_trace_t trace;

  //must be able to walk or swim in the first area
  if (!AAS_AreaGrounded(area1num) && !AAS_AreaSwim(area1num)) return 0;
  //
  if (!AAS_AreaGrounded(area2num) && !AAS_AreaSwim(area2num)) return 0;
  //
  area1 = &aasworld.areas[area1num];
  area2 = &aasworld.areas[area2num];
  //if the first area contains a liquid
  area1swim = AAS_AreaSwim(area1num);
  //if the areas are not near anough in the x-y direction
  for (i = 0; i < 2; i++)
  {
    if (area1->mins[i] > area2->maxs[i] + 10) return 0;
    if (area1->maxs[i] < area2->mins[i] - 10) return 0;
  } //end for
  //
  ground_foundreach = 0;
  ground_bestdist = 99999;
  ground_bestlength = 0;
  //
  water_foundreach = 0;
  water_bestdist = 99999;
  water_bestlength = 0;
  //
  for (i = 0; i < area1->numfaces; i++)
  {
    groundface1num = aasworld.faceindex[area1->firstface + i];
    faceside1 = groundface1num < 0;
    groundface1 = &aasworld.faces[abs(groundface1num)];
    //if this isn't a ground face
    if (!(groundface1->faceflags & 4))
    {
      //if we can swim in the first area
      if (area1swim)
      {
        //face plane must be more or less horizontal
        plane = &aasworld.planes[groundface1->planenum ^ (!faceside1)];
        if (DotProduct(plane->normal, invgravity) < 0.7) continue;
      } //end if
      else
      {
        //if we can't swim in the area it must be a ground face
        continue;
      } //end else
    } //end if
    //
    for (k = 0; k < groundface1->numedges; k++)
    {
      edge1num = aasworld.edgeindex[groundface1->firstedge + k];
      side1 = (edge1num < 0);
      //NOTE: for water faces we must take the side area 1 is
      // on into account because the face is shared and doesn't
      // have to be oriented correctly
      if (!(groundface1->faceflags & 4)) side1 = (side1 == faceside1);
      edge1num = abs(edge1num);
      edge1 = &aasworld.edges[edge1num];
      //vertexes of the edge
      VectorCopy(aasworld.vertexes[edge1->v[!side1]], v1);
      VectorCopy(aasworld.vertexes[edge1->v[side1]], v2);
      //get a vertical plane through the edge
      //NOTE: normal is pointing into area 2 because the
      //face edges are stored counter clockwise
      VectorSubtract(v2, v1, edgevec);
      CrossProduct(edgevec, invgravity, normal);
      VectorNormalize(normal);
      dist = DotProduct(normal, v1);
      //check the faces from the second area
      for (j = 0; j < area2->numfaces; j++)
      {
        groundface2 = &aasworld.faces[abs(aasworld.faceindex[area2->firstface + j])];
        //must be a ground face
        if (!(groundface2->faceflags & 4)) continue;
        //check the edges of this ground face
        for (l = 0; l < groundface2->numedges; l++)
        {
          edge2num = abs(aasworld.edgeindex[groundface2->firstedge + l]);
          edge2 = &aasworld.edges[edge2num];
          //vertexes of the edge
          VectorCopy(aasworld.vertexes[edge2->v[0]], v3);
          VectorCopy(aasworld.vertexes[edge2->v[1]], v4);
          //check the distance between the two points and the vertical plane
          //through the edge of area1
          diff = DotProduct(normal, v3) - dist;
          if (diff < -0.1 || diff > 0.1) continue;
          diff = DotProduct(normal, v4) - dist;
          if (diff < -0.1 || diff > 0.1) continue;
          //
          //project the two ground edges into the step side plane
          //and calculate the shortest distance between the two
          //edges if they overlap in the direction orthogonal to
          //the gravity direction
          CrossProduct(invgravity, normal, ort);
          invgravitydot = DotProduct(invgravity, invgravity);
          ortdot = DotProduct(ort, ort);
          //projection into the step plane
          //NOTE: since gravity is vertical this is just the z coordinate
          y1 = v1[2];//DotProduct(v1, invgravity) / invgravitydot;
          y2 = v2[2];//DotProduct(v2, invgravity) / invgravitydot;
          y3 = v3[2];//DotProduct(v3, invgravity) / invgravitydot;
          y4 = v4[2];//DotProduct(v4, invgravity) / invgravitydot;
          //
          x1 = DotProduct(v1, ort) / ortdot;
          x2 = DotProduct(v2, ort) / ortdot;
          x3 = DotProduct(v3, ort) / ortdot;
          x4 = DotProduct(v4, ort) / ortdot;
          //
          if (x1 > x2)
          {
            tmp = x1; x1 = x2; x2 = tmp;
            tmp = y1; y1 = y2; y2 = tmp;
            VectorCopy(v1, tmpv); VectorCopy(v2, v1); VectorCopy(tmpv, v2);
          } //end if
          if (x3 > x4)
          {
            tmp = x3; x3 = x4; x4 = tmp;
            tmp = y3; y3 = y4; y4 = tmp;
            VectorCopy(v3, tmpv); VectorCopy(v4, v3); VectorCopy(tmpv, v4);
          } //end if
          //if the two projected edge lines have no overlap
          if (x2 <= x3 || x4 <= x1)
          {
            continue;
          } //end if
          //if the two lines fully overlap
          if ((x1 - 0.5 < x3 && x4 < x2 + 0.5) &&
              (x3 - 0.5 < x1 && x2 < x4 + 0.5))
          {
            dist1 = y3 - y1;
            dist2 = y4 - y2;
            VectorCopy(v1, p1area1);
            VectorCopy(v2, p2area1);
            VectorCopy(v3, p1area2);
            VectorCopy(v4, p2area2);
          } //end if
          else
          {
            //if the points are equal
            if (x1 > x3 - 0.1 && x1 < x3 + 0.1)
            {
              dist1 = y3 - y1;
              VectorCopy(v1, p1area1);
              VectorCopy(v3, p1area2);
            } //end if
            else if (x1 < x3)
            {
              y = y1 + (x3 - x1) * (y2 - y1) / (x2 - x1);
              dist1 = y3 - y;
              VectorCopy(v3, p1area1);
              p1area1[2] = y;
              VectorCopy(v3, p1area2);
            } //end if
            else
            {
              y = y3 + (x1 - x3) * (y4 - y3) / (x4 - x3);
              dist1 = y - y1;
              VectorCopy(v1, p1area1);
              VectorCopy(v1, p1area2);
              p1area2[2] = y;
            } //end if
            //if the points are equal
            if (x2 > x4 - 0.1 && x2 < x4 + 0.1)
            {
              dist2 = y4 - y2;
              VectorCopy(v2, p2area1);
              VectorCopy(v4, p2area2);
            } //end if
            else if (x2 < x4)
            {
              y = y3 + (x2 - x3) * (y4 - y3) / (x4 - x3);
              dist2 = y - y2;
              VectorCopy(v2, p2area1);
              VectorCopy(v2, p2area2);
              p2area2[2] = y;
            } //end if
            else
            {
              y = y1 + (x4 - x1) * (y2 - y1) / (x2 - x1);
              dist2 = y4 - y;
              VectorCopy(v4, p2area1);
              p2area1[2] = y;
              VectorCopy(v4, p2area2);
            } //end else
          } //end else
          //if both distances are pretty much equal
          //then we take the middle of the points
          if (dist1 > dist2 - 1 && dist1 < dist2 + 1)
          {
            dist = dist1;
            VectorAdd(p1area1, p2area1, start);
            VectorScale(start, 0.5, start);
            VectorAdd(p1area2, p2area2, end);
            VectorScale(end, 0.5, end);
          } //end if
          else if (dist1 < dist2)
          {
            dist = dist1;
            VectorCopy(p1area1, start);
            VectorCopy(p1area2, end);
          } //end else if
          else
          {
            dist = dist2;
            VectorCopy(p2area1, start);
            VectorCopy(p2area2, end);
          } //end else
          //get the length of the overlapping part of the edges of the two areas
          VectorSubtract(p2area2, p1area2, dir);
          length = VectorLength(dir);
          //
          if (groundface1->faceflags & 4)
          {
            //if the vertical distance is smaller
            if (dist < ground_bestdist ||
                //or the vertical distance is pretty much the same
                //but the overlapping part of the edges is longer
                (dist < ground_bestdist + 1 && length > ground_bestlength))
            {
              ground_bestdist = dist;
              ground_bestlength = length;
              ground_foundreach = 1;
              ground_bestarea2groundedgenum = edge1num;
              ground_bestface1 = groundface1;
              //best point towards area1
              VectorCopy(start, ground_beststart);
              //normal is pointing into area2
              VectorCopy(normal, ground_bestnormal);
              //best point towards area2
              VectorCopy(end, ground_bestend);
            } //end if
          } //end if
          else
          {
            //if the vertical distance is smaller
            if (dist < water_bestdist ||
                //or the vertical distance is pretty much the same
                //but the overlapping part of the edges is longer
                (dist < water_bestdist + 1 && length > water_bestlength))
            {
              water_bestdist = dist;
              water_bestlength = length;
              water_foundreach = 1;
              water_bestarea2groundedgenum = edge1num;
              water_bestface1 = groundface1;
              //best point towards area1
              VectorCopy(start, water_beststart);
              //normal is pointing into area2
              VectorCopy(normal, water_bestnormal);
              //best point towards area2
              VectorCopy(end, water_bestend);
            } //end if
          } //end else
        } //end for
      } //end for
    } //end for
  } //end for
  //
  // NOTE: swim reachabilities are already filtered out
  //
  //check for a step reachability
  if (ground_foundreach)
  {
    //if area2 is higher but lower than the maximum step height
    //NOTE: ground_bestdist >= 0 also catches equal floor reachabilities
    if (ground_bestdist >= 0 && ground_bestdist < libvar_sv_step->value)
    {
      //create walk reachability from area1 to area2
      lreach = AAS_AllocReachability();
      if (!lreach) return 0;
      lreach->reach.areanum = area2num;
      lreach->reach.facenum = 0;
      lreach->reach.edgenum = ground_bestarea2groundedgenum;
      VectorMA(ground_beststart, 0.1f, ground_bestnormal, lreach->reach.start);
      VectorMA(ground_bestend, 5.0f, ground_bestnormal, lreach->reach.end);
      lreach->reach.traveltype = TRAVEL_WALK;
      lreach->reach.traveltime = 1;
      //if going into a crouch area
      if (!AAS_AreaCrouch(area1num) && AAS_AreaCrouch(area2num))
      {
        lreach->reach.traveltime += 300;
      } //end if
      lreach->next = areareachability[area1num];
      areareachability[area1num] = lreach;
      //NOTE: if there's nearby solid or a gap area after this area
      if (!AAS_NearbySolidOrGap(lreach->reach.start, lreach->reach.end))
      {
        lreach->reach.traveltime += 400;
      } //end if
      //avoid rather small areas
      if (AAS_AreaGroundFaceArea(lreach->reach.areanum) < 500) lreach->reach.traveltime += 400;
      //
      reach_step++;
      return 1;
    } //end if
  } //end if
  //
  //check for a waterjump reachability
  if (water_foundreach)
  {
    //get a test point a little bit towards area1
    VectorMA(water_bestend, -2, water_bestnormal, testpoint);
    //go down the maximum waterjump height
    testpoint[2] -= libvar_sv_maxwaterjump->value;
    //if there IS water the sv_maxwaterjump height below the bestend point
    if (aasworld.areasettings[AAS_PointAreaNum(testpoint)].areaflags & 4)
    {
      //don't create rediculous water jump reachabilities from areas very far below
      //the water surface
      if (water_bestdist < libvar_sv_maxwaterjump->value + 24)
      {
        //waterjumping from or towards a crouch only area is not possible in Quake2
        if ((aasworld.areasettings[area1num].presencetype & PRESENCE_NORMAL) &&
            (aasworld.areasettings[area2num].presencetype & PRESENCE_NORMAL))
        {
          //create water jump reachability from area1 to area2
          lreach = AAS_AllocReachability();
          if (!lreach) return 0;
          lreach->reach.areanum = area2num;
          lreach->reach.facenum = 0;
          lreach->reach.edgenum = water_bestarea2groundedgenum;
          VectorCopy(water_beststart, lreach->reach.start);
          VectorMA(water_bestend, 15, water_bestnormal, lreach->reach.end);
          lreach->reach.traveltype = TRAVEL_WATERJUMP;
          lreach->reach.traveltime = 700;
          lreach->next = areareachability[area1num];
          areareachability[area1num] = lreach;
          //we've got another waterjump reachability
          reach_waterjump++;
          return 1;
        } //end if
      } //end if
    } //end if
  } //end if
  //
  //check for a barrier jump reachability
  if (ground_foundreach)
  {
    //if area2 is higher but lower than the maximum barrier jump height
    if (ground_bestdist > 0 && ground_bestdist < libvar_sv_maxbarrier->value)
    {
      //if no water in area1 or a very thin layer of water on the ground
      if (!water_foundreach || (ground_bestdist - water_bestdist < 16))
      {
        //cannot perform a barrier jump towards or from a crouch area in Quake2
        if (!AAS_AreaCrouch(area1num) && !AAS_AreaCrouch(area2num))
        {
          //create barrier jump reachability from area1 to area2
          lreach = AAS_AllocReachability();
          if (!lreach) return 0;
          lreach->reach.areanum = area2num;
          lreach->reach.facenum = 0;
          lreach->reach.edgenum = ground_bestarea2groundedgenum;
          VectorMA(ground_beststart, 0.1f, ground_bestnormal, lreach->reach.start);
          VectorMA(ground_bestend, 5.0f, ground_bestnormal, lreach->reach.end);
          lreach->reach.traveltype = TRAVEL_BARRIERJUMP;
          lreach->reach.traveltime = 400;
          lreach->next = areareachability[area1num];
          areareachability[area1num] = lreach;
          //we've got another barrierjump reachability
          reach_barrier++;
          return 1;
        } //end if
      } //end if
    } //end if
  } //end if
  //
  //check for a walk or walk off ledge reachability
  if (ground_foundreach)
  {
    if (ground_bestdist < 0)
    {
      if (ground_bestdist > -libvar_sv_step->value)
      {
        //create walk reachability from area1 to area2
        lreach = AAS_AllocReachability();
        if (!lreach) return 0;
        lreach->reach.areanum = area2num;
        lreach->reach.facenum = 0;
        lreach->reach.edgenum = ground_bestarea2groundedgenum;
        VectorMA(ground_beststart, 0.1f, ground_bestnormal, lreach->reach.start);
        VectorMA(ground_bestend, 5.0f, ground_bestnormal, lreach->reach.end);
        lreach->reach.traveltype = TRAVEL_WALK;
        lreach->reach.traveltime = 1;
        lreach->next = areareachability[area1num];
        areareachability[area1num] = lreach;
        //we've got another walk reachability
        reach_walk++;
        return 1;
      } //end if
      //if not falling from too high or falling into water
      if (ground_bestdist > -AAS_FallDamageDistance() || AAS_AreaSwim(area2num))
      {
        //trace a bounding box vertically to check for solids
        VectorMA(ground_bestend, 2, ground_bestnormal, ground_bestend);
        VectorCopy(ground_bestend, start);
        start[2] = ground_beststart[2];
        VectorCopy(ground_bestend, end);
        end[2] += 4;
        trace = AAS_TraceClientBBox(start, end, PRESENCE_NORMAL, -1);
        //if no solids were found
        if (!trace.startsolid && trace.fraction >= 1.0)
        {
          //the trace end point must be in the goal area
          trace.endpos[2] += 1;
          if (AAS_PointAreaNum(trace.endpos) == area2num)
          {
            //create a walk off ledge reachability from area1 to area2
            lreach = AAS_AllocReachability();
            if (!lreach) return 0;
            lreach->reach.areanum = area2num;
            lreach->reach.facenum = 0;
            lreach->reach.edgenum = ground_bestarea2groundedgenum;
            VectorCopy(ground_beststart, lreach->reach.start);
            VectorCopy(ground_bestend, lreach->reach.end);
            lreach->reach.traveltype = TRAVEL_WALKOFFLEDGE;
            lreach->reach.traveltime = 100;
            lreach->next = areareachability[area1num];
            areareachability[area1num] = lreach;
            //
            reach_walkoffledge++;
            //NOTE: don't create a weapon (rl, bfg) jump reachability here
            //because it interferes with other reachabilities
            //like the ladder reachability
            return 1;
          } //end if
        } //end if
      } //end if
    } //end else
  } //end if
  return 0;
}

// gladiator.dll: 10013BA0..10013BD5
// gladi386.so:   00020BB8..00020BFE
/* Euclidean distance |v2 - v1|: a tail-call to VectorLength, returning what it
 * leaves on ST(0).  Not void — the decompiler read it that way because the body
 * looks like a fire-and-forget call. */
float __cdecl VectorDistance(vec3_t v1, vec3_t v2)
{
  vec3_t dir; // [esp+0h] [ebp-Ch] BYREF

  VectorSubtract(v2, v1, dir);
  return VectorLength(dir);
}

// gladiator.dll: 10013BF0..10013C46
// gladi386.so:   00020C00..00020C67
/* Returns 1 iff (v - v1) . (v - v2) <= 0, i.e. v lies within the segment [v1,v2],
 * endpoints included.  AAS_Reachability_Jump uses it to decide whether a
 * projected vertex lands inside the OTHER edge. */
int __cdecl VectorBetweenVectors(vec3_t v, vec3_t v1, vec3_t v2)
{
  float ab[3], ac[3];

  VectorSubtract(v, v1, ab);
  VectorSubtract(v, v2, ac);
  return DotProduct(ab, ac) <= 0.0f;
}

// gladiator.dll: 10013C70..10013CA4
// gladi386.so:   00020C68..00020CAA
void __cdecl VectorMiddle(vec3_t v1, vec3_t v2, vec3_t middle)
{
  VectorAdd(v1, v2, middle);
  VectorScale((float *)middle, 0.5, (float *)middle);
}

// gladiator.dll: 10013CC0..10014AC7
// gladi386.so:   00020CAC..0002243A
int AAS_Reachability_Jump(int area1num, int area2num)
{
  /* Q3's text and declarations, with the edge search written inline as the OLDER
   * AAS_ClosestEdgePoints that Q3's be_aas_reach.c still carries commented out (one
   * beststart/bestend pair, the +-0.5 VectorMiddle rule, founddist).  Gladiator lacks
   * Q3's later walk-off-ledge shortcut, sideward prediction tries, cluster-portal test
   * and fall-damage traveltime; it has its own fall-height gate, one prediction with
   * the scaled direction as cmdmove, and the 0..32 step-back probe.  The .so's frame
   * (arrays top-down in declaration order) puts p1..p4, dir1, dir2 above beststart and
   * its spill slots put bestdist first among the floats.  IDA's version -- __int64 abs()
   * temporaries, flattened guards -- cost ELF 2166 insn-diffs and PE 349 lines.  The
   * PE keeps one slot swap: cl.exe homes area1 where the DLL homes area2 (same-sized
   * spills; declaration order, comparison order and a named `height` are all inert). */
  int i, j, k, l, face1num, face2num, edge1num, edge2num, traveltype;
  float bestdist, phys_jumpvel, maxjumpdistance, maxjumpheight, speed;
  vec_t *v1, *v2, *v3, *v4;
  vec3_t p1, p2, p3, p4, dir1, dir2;
  vec3_t beststart, bestend;
  vec3_t teststart, testend, dir, cmdmove, up = {0, 0, 1};
  /* The edge-projection temporaries live in x87 registers at 80-bit precision in both
   * 1999 builds.  Where float expressions already evaluate on an x87 stack (MSVC6, i386
   * gcc without SSE math -- both oracles) plain `float` reproduces that; elsewhere
   * (aarch64, x86-64 SSE) `long double` keeps the wide intermediates so the generated
   * reachabilities stay the original's.  gcc 2.7.2.3 cannot take `long double` here:
   * its reg-stack pass aborts on this many live 80-bit temporaries. */
#if defined(_MSC_VER) || (defined(__i386__) && !defined(__SSE_MATH__))
  float a1, a2, b1, b2, dist;
#else
  long double a1, a2, b1, b2, dist;
#endif
  int founddist;
  aas_area_t *area1, *area2;
  aas_face_t *face1, *face2;
  aas_edge_t *edge1, *edge2;
  aas_plane_t *plane1, *plane2, *plane;
  aas_trace_t trace;
  aas_clientmove_t move;
  aas_reachabilitynode_t *lreach;

  if (!AAS_AreaGrounded(area1num) || !AAS_AreaGrounded(area2num)) return 0;
  //cannot jump from or to a crouch area
  if (AAS_AreaCrouch(area1num) || AAS_AreaCrouch(area2num)) return 0;
  //
  area1 = &aasworld.areas[area1num];
  area2 = &aasworld.areas[area2num];
  //
  phys_jumpvel = libvar_sv_jumpvel->value;
  //maximum distance a player can jump
  maxjumpdistance = AAS_MaxJumpDistance(phys_jumpvel);
  //maximum height a player can jump with the given initial z velocity
  maxjumpheight = AAS_MaxJumpHeight(phys_jumpvel);

  //if the areas are not near anough in the x-y direction
  for (i = 0; i < 2; i++)
  {
    if (area1->mins[i] > area2->maxs[i] + maxjumpdistance) return 0;
    if (area1->maxs[i] < area2->mins[i] - maxjumpdistance) return 0;
  } //end for
  //if area2 is way to high to jump up to
  if (area2->mins[2] > area1->maxs[2] + maxjumpheight) return 0;
  //
  bestdist = 999999;
  //
  for (i = 0; i < area1->numfaces; i++)
  {
    face1num = aasworld.faceindex[area1->firstface + i];
    face1 = &aasworld.faces[abs(face1num)];
    //if not a ground face
    if (!(face1->faceflags & 4)) continue;
    //
    for (j = 0; j < area2->numfaces; j++)
    {
      face2num = aasworld.faceindex[area2->firstface + j];
      face2 = &aasworld.faces[abs(face2num)];
      //if not a ground face
      if (!(face2->faceflags & 4)) continue;
      //
      for (k = 0; k < face1->numedges; k++)
      {
        edge1num = abs(aasworld.edgeindex[face1->firstedge + k]);
        edge1 = &aasworld.edges[edge1num];
        for (l = 0; l < face2->numedges; l++)
        {
          edge2num = abs(aasworld.edgeindex[face2->firstedge + l]);
          edge2 = &aasworld.edges[edge2num];
          //calculate the minimum distance between the two edges
          v1 = aasworld.vertexes[edge1->v[0]];
          v2 = aasworld.vertexes[edge1->v[1]];
          v3 = aasworld.vertexes[edge2->v[0]];
          v4 = aasworld.vertexes[edge2->v[1]];
          //edge vectors
          VectorSubtract(v2, v1, dir1);
          VectorSubtract(v4, v3, dir2);
          //get the horizontal directions
          dir1[2] = 0;
          dir2[2] = 0;
          //
          // p1 = point on an edge vector of area2 closest to v1
          // p2 = point on an edge vector of area2 closest to v2
          // p3 = point on an edge vector of area1 closest to v3
          // p4 = point on an edge vector of area1 closest to v4
          //
          if (dir2[0])
          {
            a2 = dir2[1] / dir2[0];
            b2 = v3[1] - a2 * v3[0];
            //point on the edge vector of area2 closest to v1
            p1[0] = (DotProduct(v1, dir2) - (a2 * dir2[0] + b2 * dir2[1])) / dir2[0];
            p1[1] = a2 * p1[0] + b2;
            //point on the edge vector of area2 closest to v2
            p2[0] = (DotProduct(v2, dir2) - (a2 * dir2[0] + b2 * dir2[1])) / dir2[0];
            p2[1] = a2 * p2[0] + b2;
          } //end if
          else
          {
            //point on the edge vector of area2 closest to v1
            p1[0] = v3[0];
            p1[1] = v1[1];
            //point on the edge vector of area2 closest to v2
            p2[0] = v3[0];
            p2[1] = v2[1];
          } //end else
          //
          if (dir1[0])
          {
            //
            a1 = dir1[1] / dir1[0];
            b1 = v1[1] - a1 * v1[0];
            //point on the edge vector of area1 closest to v3
            p3[0] = (DotProduct(v3, dir1) - (a1 * dir1[0] + b1 * dir1[1])) / dir1[0];
            p3[1] = a1 * p3[0] + b1;
            //point on the edge vector of area1 closest to v4
            p4[0] = (DotProduct(v4, dir1) - (a1 * dir1[0] + b1 * dir1[1])) / dir1[0];
            p4[1] = a1 * p4[0] + b1;
          } //end if
          else
          {
            //point on the edge vector of area1 closest to v3
            p3[0] = v1[0];
            p3[1] = v3[1];
            //point on the edge vector of area1 closest to v4
            p4[0] = v1[0];
            p4[1] = v4[1];
          } //end else
          //start with zero z-coordinates
          p1[2] = 0;
          p2[2] = 0;
          p3[2] = 0;
          p4[2] = 0;
          //get the ground planes
          plane1 = &aasworld.planes[face1->planenum];
          plane2 = &aasworld.planes[face2->planenum];
          //calculate the z-coordinates from the ground planes
          p1[2] = (plane2->dist - DotProduct(plane2->normal, p1)) / plane2->normal[2];
          p2[2] = (plane2->dist - DotProduct(plane2->normal, p2)) / plane2->normal[2];
          p3[2] = (plane1->dist - DotProduct(plane1->normal, p3)) / plane1->normal[2];
          p4[2] = (plane1->dist - DotProduct(plane1->normal, p4)) / plane1->normal[2];
          //
          founddist = 0;
          //
          if (VectorBetweenVectors(p1, v3, v4))
          {
            dist = VectorDistance(v1, p1);
            if (dist > bestdist - 0.5 && dist < bestdist + 0.5)
            {
              VectorMiddle(beststart, v1, beststart);
              VectorMiddle(bestend, p1, bestend);
            } //end if
            else if (dist < bestdist)
            {
              bestdist = dist;
              VectorCopy(v1, beststart);
              VectorCopy(p1, bestend);
            } //end if
            founddist = 1;
          } //end if
          if (VectorBetweenVectors(p2, v3, v4))
          {
            dist = VectorDistance(v2, p2);
            if (dist > bestdist - 0.5 && dist < bestdist + 0.5)
            {
              VectorMiddle(beststart, v2, beststart);
              VectorMiddle(bestend, p2, bestend);
            } //end if
            else if (dist < bestdist)
            {
              bestdist = dist;
              VectorCopy(v2, beststart);
              VectorCopy(p2, bestend);
            } //end if
            founddist = 1;
          } //end else if
          if (VectorBetweenVectors(p3, v1, v2))
          {
            dist = VectorDistance(v3, p3);
            if (dist > bestdist - 0.5 && dist < bestdist + 0.5)
            {
              VectorMiddle(beststart, p3, beststart);
              VectorMiddle(bestend, v3, bestend);
            } //end if
            else if (dist < bestdist)
            {
              bestdist = dist;
              VectorCopy(p3, beststart);
              VectorCopy(v3, bestend);
            } //end if
            founddist = 1;
          } //end else if
          if (VectorBetweenVectors(p4, v1, v2))
          {
            dist = VectorDistance(v4, p4);
            if (dist > bestdist - 0.5 && dist < bestdist + 0.5)
            {
              VectorMiddle(beststart, p4, beststart);
              VectorMiddle(bestend, v4, bestend);
            } //end if
            else if (dist < bestdist)
            {
              bestdist = dist;
              VectorCopy(p4, beststart);
              VectorCopy(v4, bestend);
            } //end if
            founddist = 1;
          } //end else if
          //if no shortest distance was found the shortest distance
          //is between one of the vertexes of edge1 and one of edge2
          if (!founddist)
          {
            dist = VectorDistance(v1, v3);
            if (dist < bestdist)
            {
              bestdist = dist;
              VectorCopy(v1, beststart);
              VectorCopy(v3, bestend);
            } //end if
            dist = VectorDistance(v1, v4);
            if (dist < bestdist)
            {
              bestdist = dist;
              VectorCopy(v1, beststart);
              VectorCopy(v4, bestend);
            } //end if
            dist = VectorDistance(v2, v3);
            if (dist < bestdist)
            {
              bestdist = dist;
              VectorCopy(v2, beststart);
              VectorCopy(v3, bestend);
            } //end if
            dist = VectorDistance(v2, v4);
            if (dist < bestdist)
            {
              bestdist = dist;
              VectorCopy(v2, beststart);
              VectorCopy(v4, bestend);
            } //end if
          } //end if
        } //end for
      } //end for
    } //end for
  } //end for
  if (bestdist > 4 && bestdist < maxjumpdistance)
  {
    //don't fall from too high
    if (beststart[2] - bestend[2] > AAS_FallDamageDistance()) return 0;
    //
    if (AAS_HorizontalVelocityForJump(0, beststart, bestend, &speed))
    {
      speed *= 1.2;
      traveltype = TRAVEL_WALKOFFLEDGE;
    } //end if
    else
    {
      //get the horizontal speed for the jump, if it isn't possible to calculate this
      //speed (the jump is not possible) then there's no jump reachability created
      if (!AAS_HorizontalVelocityForJump(phys_jumpvel, beststart, bestend, &speed))
        return 0;
      traveltype = TRAVEL_JUMP;
    } //end else
    //
    //NOTE: test if the horizontal distance isn't too small
    VectorSubtract(bestend, beststart, dir);
    dir[2] = 0;
    if (VectorLength(dir) < 10) return 0;
    //
    VectorSubtract(bestend, beststart, dir);
    VectorNormalize(dir);
    VectorMA(beststart, 1, dir, teststart);
    //
    VectorCopy(teststart, testend);
    testend[2] -= 100;
    trace = AAS_TraceClientBBox(teststart, testend, PRESENCE_NORMAL, -1);
    //
    if (trace.startsolid) return 0;
    if (trace.fraction < 1)
    {
      plane = &aasworld.planes[trace.planenum];
      if (DotProduct(plane->normal, up) >= 0.7)
      {
        if (teststart[2] - trace.endpos[2] <= libvar_sv_maxbarrier->value) return 0;
      } //end if
    } //end if
    //
    VectorMA(bestend, -1, dir, teststart);
    //
    VectorCopy(teststart, testend);
    testend[2] -= 100;
    trace = AAS_TraceClientBBox(teststart, testend, PRESENCE_NORMAL, -1);
    //
    if (trace.startsolid) return 0;
    if (trace.fraction < 1)
    {
      plane = &aasworld.planes[trace.planenum];
      if (DotProduct(plane->normal, up) >= 0.7)
      {
        if (teststart[2] - trace.endpos[2] <= libvar_sv_maxbarrier->value) return 0;
      } //end if
    } //end if
    //
    VectorSubtract(bestend, beststart, dir);
    dir[2] = 0;
    VectorNormalize(dir);
    VectorScale(dir, speed, cmdmove);
    if (traveltype == TRAVEL_JUMP) cmdmove[2] = libvar_sv_jumpvel->value;
    else cmdmove[2] = 0;
    //
    move = AAS_ClientMovementPrediction(-1, beststart, PRESENCE_NORMAL, 1, vec3_origin, cmdmove, 3, 30, 0.1, 61, 0);
    //if prediction time wasn't enough to fully predict the movement
    if (move.frames >= 30) return 0;
    //don't enter slime or lava and don't fall from too high
    if (move.stopevent & 0x38) return 0;
    //the end position should be in area2, also test a little bit back
    //because the predicted jump could have rushed through the area
    for (i = 0; i <= 32; i += 8)
    {
      VectorMA(move.endpos, -i, dir, teststart);
      teststart[2] += 0.125;
      if (AAS_PointAreaNum(teststart) == area2num) break;
    } //end for
    if (i > 32) return 0;
    //create a new reachability link
    lreach = AAS_AllocReachability();
    if (!lreach) return 0;
    lreach->reach.areanum = area2num;
    lreach->reach.facenum = 0;
    lreach->reach.edgenum = 0;
    VectorCopy(beststart, lreach->reach.start);
    VectorCopy(bestend, lreach->reach.end);
    lreach->reach.traveltype = traveltype;
    lreach->reach.traveltime = 600 + VectorDistance(bestend, beststart) * 240 / libvar_sv_maxwalkvelocity->value;
    lreach->next = areareachability[area1num];
    areareachability[area1num] = lreach;
    //
    reach_jump++;
  } //end if
  return 0;
}

// gladiator.dll: 10014E60..100158FD
// gladi386.so:   0002243C..00023415
int AAS_Reachability_Ladder(int area1num, int area2num)
{
  /* Q3's text and declarations, less Q3's "make compiler happy" zeroings, which
   * neither binary has.  DO-NOT-REVERT: the facenum stores are SIGNED, as Q3 writes
   * them -- the sign encodes face orientation and every consumer abs()es it itself
   * (the six stores at 1001531e/100153a5/10015443/100154dd/100157db/10015861 use the
   * raw value).  IDA's version (gotos, v-number aliases, the dot products folded to
   * `normal[2]`) cost ELF 1199 insn-diffs and PE 491 lines; this matches both. */

  int i, j, k, l, edge1num, edge2num, sharededgenum, lowestedgenum;
  int face1num, face2num, ladderface1num, ladderface2num;
  int ladderface1vertical, ladderface2vertical, firstv;
  float face1area, face2area, bestface1area, bestface2area;
  float phys_jumpvel, maxjumpheight;
  vec3_t area1point, area2point, v1, v2, up = {0, 0, 1};
  vec3_t mid, lowestpoint, start, end, sharededgevec, dir;
  aas_area_t *area1, *area2;
  aas_face_t *face1, *face2, *ladderface1, *ladderface2;
  aas_plane_t *plane1, *plane2;
  aas_edge_t *sharededge, *edge1;
  aas_reachabilitynode_t *lreach;
  aas_trace_t trace;

  if (!AAS_AreaLadder(area1num) || !AAS_AreaLadder(area2num)) return 0;
  //
  phys_jumpvel = libvar_sv_jumpvel->value;
  //maximum height a player can jump with the given initial z velocity
  maxjumpheight = AAS_MaxJumpHeight(phys_jumpvel);

  area1 = &aasworld.areas[area1num];
  area2 = &aasworld.areas[area2num];
  //
  ladderface1 = NULL;
  ladderface2 = NULL;
  bestface1area = -9999;
  bestface2area = -9999;
  //
  for (i = 0; i < area1->numfaces; i++)
  {
    face1num = aasworld.faceindex[area1->firstface + i];
    face1 = &aasworld.faces[abs(face1num)];
    //if not a ladder face
    if (!(face1->faceflags & 2)) continue;
    //
    for (j = 0; j < area2->numfaces; j++)
    {
      face2num = aasworld.faceindex[area2->firstface + j];
      face2 = &aasworld.faces[abs(face2num)];
      //if not a ladder face
      if (!(face2->faceflags & 2)) continue;
      //check if the faces share an edge
      for (k = 0; k < face1->numedges; k++)
      {
        edge1num = aasworld.edgeindex[face1->firstedge + k];
        for (l = 0; l < face2->numedges; l++)
        {
          edge2num = aasworld.edgeindex[face2->firstedge + l];
          if (abs(edge1num) == abs(edge2num))
          {
            //get the face with the largest area
            face1area = AAS_FaceArea(face1);
            face2area = AAS_FaceArea(face2);
            if (face1area > bestface1area && face2area > bestface2area)
            {
              bestface1area = face1area;
              bestface2area = face2area;
              ladderface1 = face1;
              ladderface2 = face2;
              ladderface1num = face1num;
              ladderface2num = face2num;
              sharededgenum = edge1num;
            } //end if
            break;
          } //end if
        } //end for
        if (l != face2->numedges) break;
      } //end for
    } //end for
  } //end for
  //
  if (ladderface1 && ladderface2)
  {
    //get the middle of the shared edge
    sharededge = &aasworld.edges[abs(sharededgenum)];
    firstv = sharededgenum < 0;
    //
    VectorCopy(aasworld.vertexes[sharededge->v[firstv]], v1);
    VectorCopy(aasworld.vertexes[sharededge->v[!firstv]], v2);
    VectorAdd(v1, v2, area1point);
    VectorScale(area1point, 0.5, area1point);
    VectorCopy(area1point, area2point);
    //
    //if the face plane in area 1 is pretty much vertical
    plane1 = &aasworld.planes[ladderface1->planenum ^ (ladderface1num < 0)];
    plane2 = &aasworld.planes[ladderface2->planenum ^ (ladderface2num < 0)];
    //
    //get the points really into the areas
    VectorSubtract(v2, v1, sharededgevec);
    CrossProduct(plane1->normal, sharededgevec, dir);
    VectorNormalize(dir);
    //NOTE: 32 because that's larger than 16 (bot bbox x,y)
    VectorMA(area1point, -32, dir, area1point);
    VectorMA(area2point, 32, dir, area2point);
    //
    ladderface1vertical = abs(DotProduct(plane1->normal, up)) < 0.1;
    ladderface2vertical = abs(DotProduct(plane2->normal, up)) < 0.1;
    //there's only reachability between vertical ladder faces
    if (!ladderface1vertical && !ladderface2vertical) return 0;
    //if both vertical ladder faces
    if (ladderface1vertical && ladderface2vertical
          //and the ladder faces do not make a sharp corner
          && DotProduct(plane1->normal, plane2->normal) > 0.7
          //and the shared edge is not too vertical
          && abs(DotProduct(sharededgevec, up)) < 0.7)
    {
      //create a new reachability link
      lreach = AAS_AllocReachability();
      if (!lreach) return 0;
      lreach->reach.areanum = area2num;
      lreach->reach.facenum = ladderface1num;
      lreach->reach.edgenum = abs(sharededgenum);
      VectorCopy(area1point, lreach->reach.start);
      //VectorCopy(area2point, lreach->end);
      VectorMA(area2point, -3, plane1->normal, lreach->reach.end);
      lreach->reach.traveltype = TRAVEL_LADDER;
      lreach->reach.traveltime = 10;
      lreach->next = areareachability[area1num];
      areareachability[area1num] = lreach;
      //
      reach_ladder++;
      //create a new reachability link
      lreach = AAS_AllocReachability();
      if (!lreach) return 0;
      lreach->reach.areanum = area1num;
      lreach->reach.facenum = ladderface2num;
      lreach->reach.edgenum = abs(sharededgenum);
      VectorCopy(area2point, lreach->reach.start);
      //VectorCopy(area1point, lreach->end);
      VectorMA(area1point, -3, plane1->normal, lreach->reach.end);
      lreach->reach.traveltype = TRAVEL_LADDER;
      lreach->reach.traveltime = 10;
      lreach->next = areareachability[area2num];
      areareachability[area2num] = lreach;
      //
      reach_ladder++;
      //
      return 1;
    } //end if
    //if the second ladder face is also a ground face
    //create ladder end (just ladder) reachability and
    //walk off a ladder (ledge) reachability
    if (ladderface1vertical && (ladderface2->faceflags & 4))
    {
      //create a new reachability link
      lreach = AAS_AllocReachability();
      if (!lreach) return 0;
      lreach->reach.areanum = area2num;
      lreach->reach.facenum = ladderface1num;
      lreach->reach.edgenum = abs(sharededgenum);
      VectorCopy(area1point, lreach->reach.start);
      VectorCopy(area2point, lreach->reach.end);
      lreach->reach.end[2] += 16;
      VectorMA(lreach->reach.end, -15, plane1->normal, lreach->reach.end);
      lreach->reach.traveltype = TRAVEL_LADDER;
      lreach->reach.traveltime = 10;
      lreach->next = areareachability[area1num];
      areareachability[area1num] = lreach;
      //
      reach_ladder++;
      //create a new reachability link
      lreach = AAS_AllocReachability();
      if (!lreach) return 0;
      lreach->reach.areanum = area1num;
      lreach->reach.facenum = ladderface2num;
      lreach->reach.edgenum = abs(sharededgenum);
      VectorCopy(area2point, lreach->reach.start);
      VectorCopy(area1point, lreach->reach.end);
      lreach->reach.traveltype = TRAVEL_WALKOFFLEDGE;
      lreach->reach.traveltime = 10;
      lreach->next = areareachability[area2num];
      areareachability[area2num] = lreach;
      //
      reach_walkoffledge++;
      //
      return 1;
    } //end if
    //
    if (ladderface1vertical)
    {
      //find lowest edge of the ladder face
      lowestpoint[2] = 99999;
      for (i = 0; i < ladderface1->numedges; i++)
      {
        edge1num = abs(aasworld.edgeindex[ladderface1->firstedge + i]);
        edge1 = &aasworld.edges[edge1num];
        //
        VectorCopy(aasworld.vertexes[edge1->v[0]], v1);
        VectorCopy(aasworld.vertexes[edge1->v[1]], v2);
        //
        VectorAdd(v1, v2, mid);
        VectorScale(mid, 0.5, mid);
        //
        if (mid[2] < lowestpoint[2])
        {
          VectorCopy(mid, lowestpoint);
          lowestedgenum = edge1num;
        } //end if
      } //end for
      //
      plane1 = &aasworld.planes[ladderface1->planenum];
      //trace down in the middle of this edge
      VectorMA(lowestpoint, 5, plane1->normal, start);
      VectorCopy(start, end);
      start[2] += 5;
      end[2] -= 100;
      //trace without entity collision
      trace = AAS_TraceClientBBox(start, end, PRESENCE_NORMAL, -1);
      //
      trace.endpos[2] += 1;
      area2num = AAS_PointAreaNum(trace.endpos);
      //
      area2 = &aasworld.areas[area2num];
      for (i = 0; i < area2->numfaces; i++)
      {
        face2num = aasworld.faceindex[area2->firstface + i];
        face2 = &aasworld.faces[abs(face2num)];
        //
        if (face2->faceflags & 2)
        {
          plane2 = &aasworld.planes[face2->planenum];
          if (abs(DotProduct(plane2->normal, up)) < 0.1) break;
        } //end if
      } //end for
      //if from another area without vertical ladder faces
      if (i >= area2->numfaces && area2num != area1num &&
            area2num &&  /* dkbot: the ladder's foot may be solid, area 0 */
            //the reachabilities shouldn't exist already
            !AAS_ReachabilityExists(area1num, area2num) &&
            !AAS_ReachabilityExists(area2num, area1num))
      {
        //if the height is jumpable
        if (start[2] - trace.endpos[2] < maxjumpheight)
        {
          //create a new reachability link
          lreach = AAS_AllocReachability();
          if (!lreach) return 0;
          lreach->reach.areanum = area2num;
          lreach->reach.facenum = ladderface1num;
          lreach->reach.edgenum = lowestedgenum;
          VectorCopy(lowestpoint, lreach->reach.start);
          VectorCopy(trace.endpos, lreach->reach.end);
          lreach->reach.traveltype = TRAVEL_LADDER;
          lreach->reach.traveltime = 10;
          lreach->next = areareachability[area1num];
          areareachability[area1num] = lreach;
          //
          reach_ladder++;
          //create a new reachability link
          lreach = AAS_AllocReachability();
          if (!lreach) return 0;
          lreach->reach.areanum = area1num;
          lreach->reach.facenum = ladderface1num;
          lreach->reach.edgenum = lowestedgenum;
          VectorCopy(trace.endpos, lreach->reach.start);
          //get the end point a little bit into the ladder
          VectorMA(lowestpoint, -5, plane1->normal, lreach->reach.end);
          //get the end point a little higher
          lreach->reach.end[2] += 10;
          lreach->reach.traveltype = TRAVEL_JUMP;
          lreach->reach.traveltime = 10;
          lreach->next = areareachability[area2num];
          areareachability[area2num] = lreach;
          //
          reach_jump++;
          //
          return 1;
        } //end if
      } //end if
    } //end if
  } //end if
  return 0;
}

// gladiator.dll: 10015BB0..10015FCC
// gladi386.so:   00023418..000238CD
void AAS_Reachability_Teleport(void)
{
  /* Q3's loop shape and names over Gladiator's Q2 misc_teleporter search and entity
   * list.  `void`, as Q3 declares it: IDA's `int` was the tail call to
   * AAS_FreeBSPEntities.  IDA's `v0`/`v26` pair around an `if` put the list head and
   * the cursor in each other's spill slots (ELF 26 insn-diffs); this matches both. */
  int area1num, area2num;
  char *target, *targetname, *classname;
  bsp_entity_t *entities, *ent, *dest;
  /* vec3 order is the ELF original's frame layout: gcc 2.7 fills the address-taken
   * group top-down in DECLARATION order. */
  vec3_t origin, destorigin, mins, maxs, end, bbmins, bbmaxs;
  aas_reachabilitynode_t *lreach;
  aas_trace_t trace;
  aas_link_t *areas, *link;
  /* dkbot: also Daikatana's trigger_teleport brush */
  vec3_t noangles = { 0, 0, 0 };
  char *model;
  int brush, k, notspawnflags = (int)LibVarValue("notspawnflags", (char *)"2048");

  entities = AAS_ParseBSPEntities();
  for (ent = entities; ent; ent = ent->next)
  {
    classname = AAS_ValueForBSPEpairKey(ent, "classname");
    if (!classname) continue;
    model = AAS_ValueForBSPEpairKey(ent, "model");
    brush = !strcmp(classname, "trigger_teleport") && model && model[0] == '*'
            && !(AAS_IntForBSPEpairKey(ent, "spawnflags") & notspawnflags);
    if (brush)
    {
      AAS_BSPModelMinsMaxsOrigin(atoi(model + 1), noangles, mins, maxs, NULL);
      VectorAdd(mins, maxs, origin);
      VectorScale(origin, 0.5, origin);
      VectorSubtract(mins, origin, mins);
      VectorSubtract(maxs, origin, maxs);
    }
    else if (strcmp(classname, "misc_teleporter")) continue;
    /* end dkbot */
    else if (!AAS_VectorForBSPEpairKey(ent, "origin", origin))
    {
      /* `target` still holds the PREVIOUS entity's target string here (garbage on
       * the first teleporter): both originals print it from its own slot. */
      botimport.Print(PRT_ERROR, "teleporter (%s) without origin\n", target);
      continue;
    } //end if
    target = AAS_ValueForBSPEpairKey(ent, "target");
    if (!target)
    {
      botimport.Print(PRT_ERROR, "teleporter at %1.0f %1.0f %1.0f without target\n",
                      origin[0], origin[1], origin[2]);
      continue;
    } //end if
    for (dest = entities; dest; dest = dest->next)
    {
      classname = AAS_ValueForBSPEpairKey(dest, "classname");
      if (!classname) continue;
      if (!strcmp(classname, "misc_teleporter_dest")
          || !strcmp(classname, "info_teleport_destination"))  /* dkbot */
      {
        targetname = AAS_ValueForBSPEpairKey(dest, "targetname");
        if (!targetname) continue;
        if (!strcmp(targetname, target))
        {
          break;
        } //end if
      } //end if
    } //end for
    if (!dest)
    {
      botimport.Print(PRT_ERROR, "teleporter without destination (%s)\n", target);
      continue;
    } //end if
    if (!AAS_VectorForBSPEpairKey(dest, "origin", destorigin))
    {
      botimport.Print(PRT_ERROR, "teleporter destination (%s) without origin\n", target);
      continue;
    } //end if
    destorigin[2] += 24;
    VectorCopy(destorigin, end);
    end[2] -= 100;
    trace = AAS_TraceClientBBox(destorigin, end, PRESENCE_CROUCH, -1);
    if (trace.startsolid)
    {
      botimport.Print(PRT_ERROR, "teleporter destination (%s) in solid\n", target);
      continue;
    } //end if
    VectorCopy(trace.endpos, destorigin);
    area2num = AAS_PointAreaNum(destorigin);
    //the bounding box of the teleporter trigger
    if (!brush)  /* dkbot */
    {
    VectorSet(mins, -8, -8, 8);
    VectorSet(maxs, 8, 8, 24);
    }
#if GLAD_SERVERFIX /* GLAD_SERVERFIX(aas-presence-bbox-swapped) */
    /* The 1999 PRESENCE_CROUCH got the standing box only through the swapped mapping,
     * and the standing box is the right one here: the grown trigger box then holds every
     * origin from which a standing bot touches the trigger, and each grounded area it
     * links gets a reachability.  Q3 passes PRESENCE_CROUCH at this call, but it links a
     * trigger_teleport brush and keeps only AAS_AreaTeleporter areas, which the box need
     * only hit.  So ask for the 1999 box by name: teleporter reachabilities stay the
     * ones the 1999 code builds. */
    AAS_PresenceTypeBoundingBox(PRESENCE_NORMAL, bbmins, bbmaxs);
#else
    AAS_PresenceTypeBoundingBox(PRESENCE_CROUCH, bbmins, bbmaxs);
#endif
    /* `origin` is the FIRST argument: it is the operand that stays on the x87 stack
     * (`fld [origin]; fld st(0); fadd [mins]`) and is reused for the maxs add.  With
     * mins/maxs first gcc reloads them and adds st(1) instead. */
    VectorAdd(origin, mins, mins);
    VectorAdd(origin, maxs, maxs);
    //add bounding box size
    VectorSubtract(mins, bbmaxs, mins);
    VectorSubtract(maxs, bbmins, maxs);
    //link an invalid (-1) entity
    areas = AAS_AASLinkEntity(mins, maxs, -1);
    for (link = areas; link; link = link->next_area)
    {
      if (!AAS_AreaGrounded(link->areanum)) continue;
      /* dkbot: past slanted planes AAS_AASLinkEntity also takes areas the box never meets */
      for (k = 0; k < 3; k++)
        if (aasworld.areas[link->areanum].mins[k] > maxs[k]
            || aasworld.areas[link->areanum].maxs[k] < mins[k]) break;
      if (k < 3) continue;
      //
      area1num = link->areanum;
      //create a new reachability link
      lreach = AAS_AllocReachability();
      if (!lreach) break;
      lreach->reach.areanum = area2num;
      lreach->reach.facenum = 0;
      lreach->reach.edgenum = 0;
      VectorCopy(origin, lreach->reach.start);
      VectorCopy(destorigin, lreach->reach.end);
      lreach->reach.traveltype = TRAVEL_TELEPORT;
      lreach->reach.traveltime = 50;
      lreach->next = areareachability[area1num];
      areareachability[area1num] = lreach;
      //
      reach_teleport++;
    } //end for
    //unlink the invalid entity
    AAS_UnlinkFromAreas(areas);
  } //end for
  AAS_FreeBSPEntities(entities);
}

// gladiator.dll: 100160E0..1001696B
// gladi386.so:   000238D0..0002454E
void AAS_Reachability_Elevator()
{
  /* Q3's text and declarations with Gladiator's entity API (the AAS_ParseBSPEntities
   * list and the value-returning Value/FloatForBSPEpairKey), less Q3's later
   * "origin" key lookup and team travel flags, and with Gladiator's traveltime floor
   * of 50 for rs_startelevator.  IDA had merged Q3's `j` into `k` and split `k`'s
   * two loops apart; the .so keeps both of `k`'s loops in esi (ELF 143 insn-diffs). */
  int area1num, area2num, modelnum, i, j, k, l, n, p;
  float lip, height, speed;
  char *model, *classname;
  bsp_entity_t *v0, *ent;
  vec3_t mins, maxs, origin, angles = {0, 0, 0};
  vec3_t pos1, pos2, mids, platbottom, plattop;
  vec3_t bottomorg, toporg, start, end, dir;
  vec_t xvals[8], yvals[8], xvals_top[8], yvals_top[8];
  aas_reachabilitynode_t *lreach;
  aas_trace_t trace;

  v0 = AAS_ParseBSPEntities();
  for (ent = v0; ent; ent = ent->next)
  {
    classname = AAS_ValueForBSPEpairKey(ent, "classname");
    if (!classname) continue;
    if (!strcmp(classname, "func_plat"))
    {
      model = AAS_ValueForBSPEpairKey(ent, "model");
      if (!model)
      {
        botimport.Print(PRT_ERROR, "func_plat without model\n");
        continue;
      } //end if
      //get the model number, and skip the leading *
      /* `++model`, not Q3's `model+1`: the .so increments in place (`inc eax; push
       * eax`).  This function's unsigned traveltime store is a DImode fix, which
       * makes gcc 2.7's local-alloc try edx first (order_regs_for_local_alloc), so
       * a separate `model+1` temporary lands in edx.  The DLL does `inc eax` either
       * way. */
      modelnum = atoi(++model);
      if (modelnum <= 0)
      {
        botimport.Print(PRT_ERROR, "func_plat with invalid model number\n");
        continue;
      } //end if
      //get the mins, maxs and origin of the model
      //NOTE: the origin is usually (0,0,0) and the mins and maxs
      //      are the absolute mins and maxs
      AAS_BSPModelMinsMaxsOrigin(modelnum, angles, mins, maxs, origin);
      //pos1 is the top position, pos2 is the bottom
      VectorCopy(origin, pos1);
      VectorCopy(origin, pos2);
      //get the lip of the plat
      lip = AAS_FloatForBSPEpairKey(ent, "lip");
      if (!lip) lip = 8;
      //get the movement height of the plat
      height = AAS_FloatForBSPEpairKey(ent, "height");
      if (!height) height = (maxs[2] - mins[2]) - lip;
      //get the speed of the plat
      speed = AAS_FloatForBSPEpairKey(ent, "speed");
      if (!speed) speed = 200;
      //get bottom position below pos1
      pos2[2] -= height;
      //
      //get a point just above the plat in the bottom position
      VectorAdd(mins, maxs, mids);
      VectorMA(pos2, 0.5, mids, platbottom);
      platbottom[2] = maxs[2] - (pos1[2] - pos2[2]) + 2;
      //get a point just above the plat in the top position
      VectorAdd(mins, maxs, mids);
      VectorMA(pos2, 0.5, mids, plattop);
      plattop[2] = maxs[2] + 2;
      //get the mins and maxs a little larger
      for (i = 0; i < 3; i++)
      {
        mins[i] -= 1;
        maxs[i] += 1;
      } //end for
      //
      VectorAdd(mins, maxs, mids);
      VectorScale(mids, 0.5, mids);
      //
      xvals[0] = mins[0]; xvals[1] = mids[0]; xvals[2] = maxs[0]; xvals[3] = mids[0];
      yvals[0] = mids[1]; yvals[1] = maxs[1]; yvals[2] = mids[1]; yvals[3] = mins[1];
      //
      xvals[4] = mins[0]; xvals[5] = maxs[0]; xvals[6] = maxs[0]; xvals[7] = mins[0];
      yvals[4] = maxs[1]; yvals[5] = maxs[1]; yvals[6] = mins[1]; yvals[7] = mins[1];
      //find adjacent areas around the bottom of the plat
      for (i = 0; i < 9; i++)
      {
        if (i < 8) //check at the sides of the plat
        {
          bottomorg[0] = origin[0] + xvals[i];
          bottomorg[1] = origin[1] + yvals[i];
          bottomorg[2] = platbottom[2] + 16;
          //get a grounded or swim area near the plat in the bottom position
          area1num = AAS_PointAreaNum(bottomorg);
          for (k = 0; k < 16; k++)
          {
            if (area1num)
            {
              if (AAS_AreaGrounded(area1num) || AAS_AreaSwim(area1num)) break;
            } //end if
            bottomorg[2] += 4;
            area1num = AAS_PointAreaNum(bottomorg);
          } //end if
          //if in solid
          if (k >= 16)
          {
            continue;
          } //end if
        } //end if
        else //at the middle of the plat
        {
          VectorCopy(plattop, bottomorg);
          bottomorg[2] += 24;
          area1num = AAS_PointAreaNum(bottomorg);
          if (!area1num) continue;
          VectorCopy(platbottom, bottomorg);
          bottomorg[2] += 24;
        } //end else
        //look at adjacent areas around the top of the plat
        //make larger steps to outside the plat everytime
        for (n = 0; n < 3; n++)
        {
          for (k = 0; k < 3; k++)
          {
            mins[k] -= 4;
            maxs[k] += 4;
          } //end for
          xvals_top[0] = mins[0]; xvals_top[1] = mids[0]; xvals_top[2] = maxs[0]; xvals_top[3] = mids[0];
          yvals_top[0] = mids[1]; yvals_top[1] = maxs[1]; yvals_top[2] = mids[1]; yvals_top[3] = mins[1];
          //
          xvals_top[4] = mins[0]; xvals_top[5] = maxs[0]; xvals_top[6] = maxs[0]; xvals_top[7] = mins[0];
          yvals_top[4] = maxs[1]; yvals_top[5] = maxs[1]; yvals_top[6] = mins[1]; yvals_top[7] = mins[1];
          //
          for (j = 0; j < 8; j++)
          {
            toporg[0] = origin[0] + xvals_top[j];
            toporg[1] = origin[1] + yvals_top[j];
            toporg[2] = plattop[2] + 16;
            //get a grounded or swim area near the plat in the top position
            area2num = AAS_PointAreaNum(toporg);
            for (l = 0; l < 16; l++)
            {
              if (area2num)
              {
                if (AAS_AreaGrounded(area2num) || AAS_AreaSwim(area2num))
                {
                  VectorCopy(plattop, start);
                  start[2] += 32;
                  VectorCopy(toporg, end);
                  end[2] += 1;
                  trace = AAS_TraceClientBBox(start, end, PRESENCE_CROUCH, -1);
                  if (trace.fraction >= 1) break;
                } //end if
              } //end if
              toporg[2] += 4;
              area2num = AAS_PointAreaNum(toporg);
            } //end if
            //if in solid
            if (l >= 16) continue;
            //never create a reachability in the same area
            if (area2num == area1num) continue;
            //if the area isn't grounded
            if (!AAS_AreaGrounded(area2num)) continue;
            //if there already exists reachability between the areas
            if (AAS_ReachabilityExists(area1num, area2num)) continue;
            //if the reachability start is within the elevator bounding box
            VectorSubtract(bottomorg, platbottom, dir);
            VectorNormalize(dir);
            dir[0] = bottomorg[0] + 24 * dir[0];
            dir[1] = bottomorg[1] + 24 * dir[1];
            dir[2] = bottomorg[2];
            //
            for (p = 0; p < 3; p++)
              if (dir[p] < origin[p] + mins[p] || dir[p] > origin[p] + maxs[p]) break;
            if (p >= 3) continue;
            //create a new reachability link
            lreach = AAS_AllocReachability();
            if (!lreach) continue;
            lreach->reach.areanum = area2num;
            //the facenum is the model number
            lreach->reach.facenum = modelnum;
            //the edgenum is the height
            lreach->reach.edgenum = (int) height;
            //
            VectorCopy(dir, lreach->reach.start);
            VectorCopy(toporg, lreach->reach.end);
            lreach->reach.traveltype = TRAVEL_ELEVATOR;
            lreach->reach.traveltime = height * 100 / speed;
            if (!lreach->reach.traveltime) lreach->reach.traveltime = 50;
            lreach->next = areareachability[area1num];
            areareachability[area1num] = lreach;
            //don't go any further to the outside
            n = 9999;
            //
            reach_elevator++;
          } //end for
        } //end for
      } //end for
    } //end if
  } //end for
  AAS_FreeBSPEntities(v0);
}

// gladiator.dll: 10016BA0..100171BC
// gladi386.so:   00024550..00024DE6
int __cdecl AAS_Reachability_Grapple(int area1num, int area2num)
{
  /* Q3's text and declarations with Gladiator's constants, less Q3's later
   * AAS_TraceAreas cluster-portal test and with 500 for rs_startgrapple.  The
   * traveltime store needs no cast: `traveltime` is Q3's `unsigned short`, which
   * gcc 2.7 converts through fixuns_truncdfsi2 -- the .so's `fistp QWORD` that
   * IDA's `(__int64)` imitated.  IDA's inverted guards cost ELF 70 insn-diffs. */
  int face2num, i, areanum;
  float mingrappleangle, z, hordist;
  bsp_trace_t bsptrace;
  aas_trace_t trace;
  aas_face_t *face2;
  aas_area_t *area1, *area2;
  aas_reachabilitynode_t *lreach;
  vec3_t areastart, facecenter, start, end, dir, down = {0, 0, -1};
  vec_t *v;

  //only grapple when on the ground or swimming
  if (!AAS_AreaGrounded(area1num) && !AAS_AreaSwim(area1num)) return 0;
  //don't grapple from a crouch area
  if (!(AAS_AreaPresenceType(area1num) & PRESENCE_NORMAL)) return 0;
  //NOTE: disabled area swim it doesn't work right
  if (AAS_AreaSwim(area1num)) return 0;
  //
  area1 = &aasworld.areas[area1num];
  area2 = &aasworld.areas[area2num];
  //don't grapple towards way lower areas
  if (area2->maxs[2] < area1->mins[2]) return 0;
  //
  VectorCopy(aasworld.areas[area1num].center, start);
  //if not a swim area
  if (!AAS_AreaSwim(area1num))
  {
    if (!AAS_PointAreaNum(start)) Log_Write("area %d center %f %f %f in solid?", area1num,
                start[0], start[1], start[2]);
    VectorCopy(start, end);
    end[2] -= 1000;
    trace = AAS_TraceClientBBox(start, end, PRESENCE_CROUCH, -1);
    if (trace.startsolid) return 0;
    VectorCopy(trace.endpos, areastart);
  } //end if
  else
  {
    if (!(AAS_PointContents(start) & 0x38)) return 0;
  } //end else
  //
  //start is now the start point
  //
  for (i = 0; i < area2->numfaces; i++)
  {
    face2num = aasworld.faceindex[area2->firstface + i];
    face2 = &aasworld.faces[abs(face2num)];
    //if it is not a solid face
    if (!(face2->faceflags & 1)) continue;
    //direction towards the first vertex of the face
    v = aasworld.vertexes[aasworld.edges[abs(aasworld.edgeindex[face2->firstedge])].v[0]];
    VectorSubtract(v, areastart, dir);
    //if the face plane is facing away
    if (DotProduct(aasworld.planes[face2->planenum].normal, dir) > 0) continue;
    //get the center of the face
    AAS_FaceCenter(face2num, facecenter);
    //only go higher up with the grapple
    if (facecenter[2] < areastart[2] + 64) continue;
    //only use vertical faces or downward facing faces
    if (DotProduct(aasworld.planes[face2->planenum].normal, down) < 0) continue;
    //direction towards the face center
    VectorSubtract(facecenter, areastart, dir);
    //
    z = dir[2];
    dir[2] = 0;
    hordist = VectorLength(dir);
    if (!hordist) continue;
    //if too far
    if (hordist > 2000) continue;
    //check the minimal angle of the movement
    mingrappleangle = 15; //15 degrees
    if (z / hordist < tan(2 * M_PI * mingrappleangle / 360)) continue;
    //
    VectorCopy(facecenter, start);
    VectorMA(facecenter, -500, aasworld.planes[face2->planenum].normal, end);
    //
    bsptrace = AAS_Trace(start, NULL, NULL, end, 0, 100663299);
    //the grapple won't stick to the sky and the grapple point should be near the AAS wall
    if ((bsptrace.surface.flags & 4) || (bsptrace.fraction * 500 >= 32)) continue;
    //trace a full bounding box from the area center on the ground to
    //the center of the face
    VectorSubtract(facecenter, areastart, dir);
    VectorNormalize(dir);
    VectorMA(areastart, 4, dir, start);
    VectorCopy(bsptrace.endpos, end);
    trace = AAS_TraceClientBBox(start, end, PRESENCE_NORMAL, -1);
    VectorSubtract(trace.endpos, facecenter, dir);
    if (VectorLength(dir) > 24) continue;
    //
    VectorCopy(trace.endpos, start);
    VectorCopy(trace.endpos, end);
    end[2] -= AAS_FallDamageDistance();
    trace = AAS_TraceClientBBox(start, end, PRESENCE_NORMAL, -1);
    if (trace.fraction >= 1) continue;
    //area to end in
    areanum = AAS_PointAreaNum(trace.endpos);
    //if not in lava or slime
    if (aasworld.areasettings[areanum].contents & 6)
    {
      continue;
    } //end if
    //do not go the the source area
    if (areanum == area1num) continue;
    //don't create reachabilities if they already exist
    if (AAS_ReachabilityExists(area1num, areanum)) continue;
    //only end in areas we can stand
    if (!AAS_AreaGrounded(areanum)) continue;
    //create a new reachability link
    lreach = AAS_AllocReachability();
    if (!lreach) return 0;
    lreach->reach.areanum = areanum;
    lreach->reach.facenum = face2num;
    lreach->reach.edgenum = 0;
    VectorCopy(areastart, lreach->reach.start);
    VectorCopy(bsptrace.endpos, lreach->reach.end);
    lreach->reach.traveltype = TRAVEL_GRAPPLEHOOK;
    VectorSubtract(lreach->reach.end, lreach->reach.start, dir);
    lreach->reach.traveltime = 500 + VectorLength(dir) * 0.25;
    lreach->next = areareachability[area1num];
    areareachability[area1num] = lreach;
    //
    reach_grapple++;
  } //end for
  //
  return 0;
}

// gladiator.dll: 10017350..10017ABC
// gladi386.so:   00024DE8..000251E4
int AAS_SetWeaponJumpAreaFlags()
{
  bsp_entity_t *v0; // ebx — entity list head
  bsp_entity_t *ent; // ebp — current entity
  const char *v2; // eax
  const char *classname; // edi
  int areanum; // eax
  vec3_t mins; // BYREF
  vec3_t maxs; // BYREF
  vec3_t origin; // BYREF

  /* Float literals, not the decompiler's raw ±15.0f bit patterns: as int literals
   * assigned to a float[3] they convert to ~±1.1e9, leaving the bounds effectively
   * unbounded so AAS_BestReachableArea flags the wrong areas. */
  mins[0] = -15.0f;
  mins[1] = -15.0f;
  mins[2] = -15.0f;
  maxs[0] = 15.0f;
  maxs[1] = 15.0f;
  maxs[2] = 15.0f;
  v0 = AAS_ParseBSPEntities();
  ent = v0;
  if ( v0 )
  {
    do
    {
      v2 = (const char *)AAS_ValueForBSPEpairKey(ent, "classname");
      classname = v2;
      if ( v2
        && (!strcmp(v2, "item_armor_body")
            || !strcmp(classname, "item_armor_combat")
            || !strcmp(classname, "item_power_screen")
            || !strcmp(classname, "item_power_shield")
            || !strcmp(classname, "weapon_grenadelauncher")
            || !strcmp(classname, "weapon_rocketlauncher")
            || !strcmp(classname, "weapon_hyperblaster")
            || !strcmp(classname, "weapon_railgun")
            || !strcmp(classname, "weapon_bfg")
            || !strcmp(classname, "weapon_boomer")
            || !strcmp(classname, "weapon_phalanx")
            || !strcmp(classname, "item_quadfire")
            || !strcmp(classname, "weapon_etf_rifle")
            || !strcmp(classname, "weapon_proxlauncher")
            || !strcmp(classname, "weapon_plasmabeam")
            || !strcmp(classname, "weapon_chainfist")
            || !strcmp(classname, "weapon_disintegrator")
            || !strcmp(classname, "item_ir_goggles")
            || !strcmp(classname, "item_double")
            || !strcmp(classname, "item_compass")
            || !strcmp(classname, "item_sphere_vengeance")
            || !strcmp(classname, "item_sphere_hunter")
            || !strcmp(classname, "item_sphere_defender")
            || !strcmp(classname, "item_doppleganger")
            || !strcmp(classname, "dm_tag_token")
            || !strcmp(classname, "dm_tag_token")
            || !strcmp(classname, "item_health_mega")
            || !strcmp(classname, "item_quad")
            || !strcmp(classname, "item_invulnerability"))
        && AAS_VectorForBSPEpairKey(ent, "origin", (float *)origin) )
      {
        if ( !AAS_DropToFloor((float *)origin, (float *)mins, (float *)maxs) )
          botimport.Print(
            1,
            "%s in solid at (%1.1f %1.1f %1.1f)\n",
            classname,
            *(float *)origin,
            origin[1],
            origin[2]);
        areanum = AAS_BestReachableArea(origin, (float *)mins, (float *)maxs, (float *)origin);
        aasworld.areasettings[areanum].areaflags |= 0x2000u;
      }
      ent = ent->next;
    }
    while ( ent );
  }
  return ((int (__cdecl *)(bsp_entity_t *))AAS_FreeBSPEntities)(v0);
}

// gladiator.dll: 10017CA0..100180B1
// gladi386.so:   000251E4..00025707
int __cdecl AAS_Reachability_WeaponJump(int area1num, int area2num)
{
  /* Q3's text and declarations -- with the `teststart` Q3 later commented out of the
   * vec3 list, which the .so's frame still has -- and Gladiator's own rules: speed
   * under 270, one prediction with 3 command frames, the 0..32 step-back probe and a
   * flat traveltime of 500.  The height test is `1.6 * facecenter[2] - areastart[2]`
   * as both binaries compute it; Q3 later parenthesised it and then commented it
   * out.  IDA's version (flag-and-break exits, split probe counters) cost ELF 248
   * insn-diffs. */
  int face2num, i, j, n, ret;
  float speed, zvel, hordist;
  aas_face_t *face2;
  aas_area_t *area1, *area2;
  aas_reachabilitynode_t *lreach;
  vec3_t areastart, facecenter, start, end, dir, cmdmove, teststart;
  vec3_t velocity;
  aas_clientmove_t move;
  aas_trace_t trace;

  if (!AAS_AreaGrounded(area1num) || AAS_AreaSwim(area1num)) return 0;
  if (!AAS_AreaGrounded(area2num)) return 0;
  //NOTE: only weapon jump towards areas with an interesting item in it??
  if (!(aasworld.areasettings[area2num].areaflags & 0x2000)) return 0;
  //
  area1 = &aasworld.areas[area1num];
  area2 = &aasworld.areas[area2num];
  //don't weapon jump towards way lower areas
  if (area2->maxs[2] < area1->mins[2]) return 0;
  //
  VectorCopy(aasworld.areas[area1num].center, start);
  //if not a swim area
  if (!AAS_PointAreaNum(start)) Log_Write("area %d center %f %f %f in solid?", area1num,
              start[0], start[1], start[2]);
  VectorCopy(start, end);
  end[2] -= 1000;
  trace = AAS_TraceClientBBox(start, end, PRESENCE_CROUCH, -1);
  if (trace.startsolid) return 0;
  VectorCopy(trace.endpos, areastart);
  //
  //areastart is now the start point
  //
  for (i = 0; i < area2->numfaces; i++)
  {
    face2num = aasworld.faceindex[area2->firstface + i];
    face2 = &aasworld.faces[abs(face2num)];
    //if it is not a solid face
    if (!(face2->faceflags & 4)) continue;
    //get the center of the face
    AAS_FaceCenter(face2num, facecenter);
    //only go higher up with weapon jumps
    if (facecenter[2] < areastart[2] + 64) continue;
    //NOTE: set to 2 to allow bfg jump reachabilities
    for (n = 0; n < 1/*2*/; n++)
    {
      //get the rocket jump z velocity
      if (n) zvel = AAS_BFGJumpZVelocity(areastart);
      else zvel = AAS_RocketJumpZVelocity(areastart);
      //get the horizontal speed for the jump, if it isn't possible to calculate this
      //speed (the jump is not possible) then there's no jump reachability created
      ret = AAS_HorizontalVelocityForJump(zvel, areastart, facecenter, &speed);
      if (ret && speed < 270)
      {
        //direction towards the face center
        VectorSubtract(facecenter, areastart, dir);
        dir[2] = 0;
        hordist = VectorNormalize(dir);
        //NOTE: no parentheses; Q3 later has 1.6 * (facecenter[2] - areastart[2])
        if (hordist < 1.6 * facecenter[2] - areastart[2])
        {
          //get command movement
          VectorScale(dir, speed, cmdmove);
          VectorSet(velocity, 0, 0, zvel);
          //
          move = AAS_ClientMovementPrediction(-1, areastart, PRESENCE_NORMAL, 1, velocity, cmdmove, 3, 30, 0.1f, 61, 0);
          //if prediction time wasn't enough to fully predict the movement
          //don't enter slime or lava and don't fall from too high
          if (move.frames < 30 && !(move.stopevent & 0x38))
          {
            //the end position should be in area2, also test a little bit back
            for (j = 0; j <= 32; j += 8)
            {
              VectorMA(move.endpos, -j, dir, teststart);
              teststart[2] += 0.125;
              if (AAS_PointAreaNum(teststart) == area2num) break;
            } //end for
            if (j <= 32)
            {
              //create a rocket or bfg jump reachability from area1 to area2
              lreach = AAS_AllocReachability();
              if (!lreach) return 0;
              lreach->reach.areanum = area2num;
              lreach->reach.facenum = 0;
              lreach->reach.edgenum = 0;
              VectorCopy(areastart, lreach->reach.start);
              VectorCopy(facecenter, lreach->reach.end);
              if (n) lreach->reach.traveltype = TRAVEL_BFGJUMP;
              else lreach->reach.traveltype = TRAVEL_ROCKETJUMP;
              lreach->reach.traveltime = 500;
              lreach->next = areareachability[area1num];
              areareachability[area1num] = lreach;
              //
              reach_rocketjump++;
              return 1;
            } //end if
          } //end if
        } //end if
      } //end if
    } //end for
  } //end for
  //
  return 0;
}

// gladiator.dll: 100181D0..1001869C
// gladi386.so:   00025708..00025D6C
void __cdecl AAS_Reachability_WalkOffLedge(int areanum)
{
  /* Q3's text and declarations with Gladiator's constants, less Q3's later
   * additions (the AAS_TraceAreas cluster-portal test, rs_maxfallheight); the
   * traveltime is Gladiator's own fall-damage rule.  IDA's version -- abs(face2num)
   * cached across the gap scan, the edge compare inverted into a `continue` --
   * cost ELF 255 insn-diffs and PE 8 bytes; this matches both. */
  int i, j, k, l, m, n;
  int face1num, face2num, face3num, edge1num, edge2num, edge3num;
  int otherareanum, gap, reachareanum, side;
  aas_area_t *area, *area2;
  aas_face_t *face1, *face2, *face3;
  aas_edge_t *edge;
  aas_plane_t *plane;
  vec_t *v1, *v2;
  vec3_t sharededgevec, mid, dir, testend;
  aas_reachabilitynode_t *lreach;
  aas_trace_t trace;

  if ( !AAS_AreaGrounded(areanum) || AAS_AreaSwim(areanum) ) return;
  area = &aasworld.areas[areanum];
  for ( i = 0; i < area->numfaces; i++ )
  {
    face1num = aasworld.faceindex[area->firstface + i];
    face1 = &aasworld.faces[abs(face1num)];
    /* face 1 must be a ground face */
    if ( !(face1->faceflags & 4) ) continue;
    /* go through all the edges of this ground face */
    for ( k = 0; k < face1->numedges; k++ )
    {
      edge1num = aasworld.edgeindex[face1->firstedge + k];
      /* find another not ground face using this same edge */
      for ( j = 0; j < area->numfaces; j++ )
      {
        face2num = aasworld.faceindex[area->firstface + j];
        face2 = &aasworld.faces[abs(face2num)];
        /* face 2 may not be a ground face */
        if ( face2->faceflags & 4 ) continue;
        /* compare all the edges */
        for ( l = 0; l < face2->numedges; l++ )
        {
          edge2num = aasworld.edgeindex[face2->firstedge + l];
          if ( abs(edge1num) == abs(edge2num) )
          {
            /* get the area at the other side of the face */
            if ( face2->frontarea == areanum ) otherareanum = face2->backarea;
            else otherareanum = face2->frontarea;
            area2 = &aasworld.areas[otherareanum];
            /* if the other area is grounded! */
            if ( aasworld.areasettings[otherareanum].areaflags & 1 )
            {
              /* check for a possible gap */
              gap = 0;
              for ( n = 0; n < area2->numfaces; n++ )
              {
                face3num = aasworld.faceindex[area2->firstface + n];
                /* may not be the shared face of the two areas */
                if ( abs(face3num) == abs(face2num) ) continue;
                face3 = &aasworld.faces[abs(face3num)];
                /* find an edge shared by all three faces */
                for ( m = 0; m < face3->numedges; m++ )
                {
                  edge3num = aasworld.edgeindex[face3->firstedge + m];
                  /* but the edge should be shared by all three faces */
                  if ( abs(edge3num) == abs(edge1num) )
                  {
                    if ( !(face3->faceflags & 1) )
                    {
                      gap = 1;
                      break;
                    }
                    if ( face3->faceflags & 4 )
                    {
                      gap = 0;
                      break;
                    }
                    gap = 1;
                    break;
                  }
                }
                if ( m < face3->numedges ) break;
              }
              if ( !gap ) break;
            }
            /* check for a walk off ledge reachability */
            edge = &aasworld.edges[abs(edge1num)];
            side = edge1num < 0;
            v1 = aasworld.vertexes[edge->v[side]];
            v2 = aasworld.vertexes[edge->v[!side]];
            plane = &aasworld.planes[face1->planenum];
            /* get the points really into the areas */
            VectorSubtract(v2, v1, sharededgevec);
            CrossProduct(plane->normal, sharededgevec, dir);
            VectorNormalize(dir);
            VectorAdd(v1, v2, mid);
            VectorScale(mid, 0.5, mid);
            VectorMA(mid, 8, dir, mid);
            VectorCopy(mid, testend);
            testend[2] -= 1000;
            trace = AAS_TraceClientBBox(mid, testend, PRESENCE_CROUCH, -1);
            if ( trace.startsolid ) break;
            reachareanum = AAS_PointAreaNum(trace.endpos);
            if ( reachareanum == areanum ) break;
            if ( AAS_ReachabilityExists(areanum, reachareanum) ) break;
            if ( !AAS_AreaGrounded(reachareanum) && !AAS_AreaSwim(reachareanum) ) break;
            if ( aasworld.areasettings[reachareanum].contents & 6 ) break;
            lreach = AAS_AllocReachability();
            if ( !lreach ) break;
            lreach->reach.areanum = reachareanum;
            lreach->reach.facenum = 0;
            lreach->reach.edgenum = edge1num;
            VectorCopy(mid, lreach->reach.start);
            VectorCopy(trace.endpos, lreach->reach.end);
            lreach->reach.traveltype = TRAVEL_WALKOFFLEDGE;
            if ( !AAS_AreaSwim(reachareanum) && mid[2] - trace.endpos[2] > AAS_FallDamageDistance() )
              lreach->reach.traveltime = 3000;
            else
              lreach->reach.traveltime = 100;
            lreach->next = areareachability[areanum];
            areareachability[areanum] = lreach;
            /* we've got another walk off ledge reachability */
            reach_walkoffledge++;
          }
        }
      }
    }
  }
}

// gladiator.dll: 100187E0..100188D1
// gladi386.so:   00025D6C..00025EA2
void AAS_StoreReachability()
{
  int i;
  aas_areasettings_t *areasettings;
  aas_reachabilitynode_t *lreach;
  aas_reachability_t *reach;

  if ( aasworld.reachability )
    FreeMemory(aasworld.reachability);
  aasworld.reachability = (aas_reachability_t *)GetClearedMemory(2883584);
  aasworld.reachabilitysize = 1;
  for ( i = 0; i < aasworld.numareas; i++ )
  {
    areasettings = &aasworld.areasettings[i];
    areasettings->firstreachablearea = aasworld.reachabilitysize;
    areasettings->numreachableareas = 0;
    for ( lreach = areareachability[i]; lreach; lreach = lreach->next )
    {
      /* dkbot: no link into lava or slime from outside it */
      if ( lreach->reach.areanum > 0
        && lreach->reach.areanum < aasworld.numareas
        && (aasworld.areasettings[lreach->reach.areanum].contents & 6)
        && !(aasworld.areasettings[i].contents & 6) )
        continue;
      reach = &aasworld.reachability[areasettings->firstreachablearea +
                                      areasettings->numreachableareas];
      reach->areanum = lreach->reach.areanum;
      reach->facenum = lreach->reach.facenum;
      reach->edgenum = lreach->reach.edgenum;
      VectorCopy(lreach->reach.start, reach->start);
      VectorCopy(lreach->reach.end, reach->end);
      reach->traveltype = lreach->reach.traveltype;
      reach->traveltime = lreach->reach.traveltime;
      areasettings->numreachableareas++;
    }
    aasworld.reachabilitysize += areasettings->numreachableareas;
  }
}

// gladiator.dll: 10018920..10018BB2
// gladi386.so:   00025EA4..00026229
int AAS_ContinueInitReachability(float time)
{
  int todo; // ebx
  libvar_t *v4;
  int i; // esi
  int start_time; // ebp
  int j; // edi

  (void)time; /* caller passes arg; original function ignores it (no ebp frame at 0x10018920) */

  if ( !aasworld.loaded )
    return 0;
  if ( aasworld.numreachabilityareas >= aasworld.numareas )
    return 0;
  /* Through the global, not IDA's `v1`/`v2` aliases: the original re-reads
   * libvar_framereachability for the `->value` load (`mov eax,[GOT]; fld
   * [eax+0x10]`), and the `double v2` copy is what makes our frame 0x14 against
   * real's 0x10. */
  if ( !libvar_framereachability )
  {
    libvar_framereachability = LibVar("framereachability", (char *)"20");
    if ( libvar_framereachability->value <= 0.0f )
      libvar_framereachability->value = 15.0f;
  }
  /* `(int)`, not IDA's `(__int64)`: gladi386.so does the 32-bit `fistp DWORD PTR
   * [esp]` round-to-zero sequence here, and the 64-bit cast costs an 8-byte temp
   * (frame 0x14 vs real's 0x10) plus the QWORD store.  `todo` is an int anyway;
   * MSVC's __ftol leaves the value in eax either way, which is why IDA wrote 64. */
  todo = aasworld.numreachabilityareas + (int)libvar_framereachability->value;
  if ( !libvar_reachabilitydelay )
  {
    v4 = LibVar("reachability_delay", (char *)"100");
    libvar_reachabilitydelay = v4;
    if ( v4->value <= 0.0f )
      v4->value = 200.0f;
  }
  start_time = Sys_MilliSeconds();
  i = aasworld.numreachabilityareas;
  for ( ; i < aasworld.numareas; ++i )
  {
    if ( i >= todo )
      break;
    ++aasworld.numreachabilityareas;
    for ( j = 1; j < aasworld.numareas; ++j )
    {
      if ( i != j
        && !AAS_ReachabilityExists(i, j)
        && !AAS_Reachability_Swim(i, j)
        && !AAS_Reachability_EqualFloorHeight(i, j)
        && !AAS_Reachability_Step_Barrier_WaterJump_WalkOffLedge(i, j)
        && !AAS_Reachability_Ladder(i, j) )
      {
        AAS_Reachability_Jump(i, j);
      }
    }
    for ( j = 1; j < aasworld.numareas; ++j )
    {
      if ( i != j && !AAS_ReachabilityExists(i, j) )
      {
        AAS_Reachability_Grapple(i, j);
        AAS_Reachability_WeaponJump(i, j);
      }
    }
    if ( Sys_MilliSeconds() - start_time > (int)libvar_reachabilitydelay->value )
      break;
  }
  if ( aasworld.numreachabilityareas >= aasworld.numareas )
  {
    for ( i = 1; i < aasworld.numareas; ++i )
      AAS_Reachability_WalkOffLedge(i);
    AAS_Reachability_Teleport();
    AAS_Reachability_Elevator();
    AAS_StoreReachability();
    AAS_ShutDownReachabilityHeap();
    FreeMemory(areareachability);
    botimport.Print(PRT_MESSAGE, "calculating clusters...\n");
  }
  else
  {
    if ( aasworld.numreachabilityareas - (int)libvar_framereachability->value <= 1 )
      botimport.Print(PRT_MESSAGE, "calculating reachability...\n");
    if ( aasworld.numreachabilityareas + (int)libvar_framereachability->value >= aasworld.numareas )
    {
      botimport.Print(PRT_MESSAGE, "\r%6d%%%%", 100);
      botimport.Print(PRT_MESSAGE, "\nplease wait while storing reachability...\n");
    }
    else
    {
      botimport.Print(PRT_MESSAGE, "\r%6d%%%%", 100 * aasworld.numreachabilityareas / aasworld.numareas);
    }
  }
  return 1;
}

// gladiator.dll: 10018C70..10018CD7
// gladi386.so:   0002622C..0002639C
void AAS_InitReachability()
{
  if ( aasworld.loaded )
  {
    if ( aasworld.reachabilitysize && !(unsigned int)(int)LibVarGetValue("forcereachability") )
    {
      aasworld.numreachabilityareas = aasworld.numareas;
    }
    else
    {
      aasworld.savefile = 1;
      aasworld.numreachabilityareas = 1;
      AAS_SetupReachabilityHeap();
      areareachability = (aas_reachabilitynode_t **)GetClearedMemory(aasworld.numareas * sizeof(aas_reachabilitynode_t *));
      AAS_SetWeaponJumpAreaFlags();
    }
  }
}

/* dkbot: bots miss jumps, so one whose fall ends in lava, harmful water or a kill zone is no route */
#define DK_FATAL_R  (0x0008 | 0x0800)
#define DK_FLOOR_R  (0x0001 | 0x0002 | 0x10000)
#define DK_FATAL_HURT     100
#define DK_MAX_KILLZONES  32

static int dk_numkill;
static vec3_t dk_killmins[DK_MAX_KILLZONES], dk_killmaxs[DK_MAX_KILLZONES];

static int dk_fatal_fall(vec3_t point)
{
  vec3_t end, nobox = { 0.0f, 0.0f, 0.0f };
  bsp_trace_t trace;
  int i;

  VectorCopy(point, end);
  end[2] -= 4096.0f;
  trace = AAS_Trace(point, nobox, nobox, end, 0, DK_FLOOR_R | DK_FATAL_R);
  if ( trace.startsolid )
    return 0;
  if ( trace.fraction < 1.0f && (trace.contents & DK_FATAL_R) )
    return 1;
  for ( i = 0; i < dk_numkill; i++ )
    if ( point[0] > dk_killmins[i][0] && point[0] < dk_killmaxs[i][0]
      && point[1] > dk_killmins[i][1] && point[1] < dk_killmaxs[i][1]
      && dk_killmins[i][2] < point[2] && dk_killmaxs[i][2] > trace.endpos[2] )
      return 1;
  return 0;
}

/* dkbot: a trap is a region the AI's travel types enter but never leave */
static int dk_ai_travel(int traveltype)
{
  return traveltype >= 2 && traveltype <= 11;   /* TRAVEL_WALK .. TRAVEL_ELEVATOR */
}

/* Triggers of teleporters into a trap or nowhere: a link that meets one takes it. */
#define DK_MAX_TRAPGATES  16

static int dk_trap_gates(bsp_entity_t *ents, const int *comp, const char *reached, int best,
                         vec3_t *mins, vec3_t *maxs)
{
  vec3_t noangles = { 0.0f, 0.0f, 0.0f };
  bsp_entity_t *ent;
  char *cn, *model;
  int i, n = 0, notspawnflags = (int)LibVarValue("notspawnflags", (char *)"2048");

  for ( ent = ents; ent && n < DK_MAX_TRAPGATES; ent = ent->next )
  {
    cn = AAS_ValueForBSPEpairKey(ent, "classname");
    model = AAS_ValueForBSPEpairKey(ent, "model");
    if ( !cn || strcmp(cn, "trigger_teleport") || !model || model[0] != '*'
      || (AAS_IntForBSPEpairKey(ent, "spawnflags") & notspawnflags) )
      continue;
    AAS_BSPModelMinsMaxsOrigin(atoi(model + 1), noangles, mins[n], maxs[n], NULL);
    for ( i = 1; i < aasworld.reachabilitysize; i++ )
    {
      aas_reachability_t *r = &aasworld.reachability[i];

      if ( r->traveltype == 10 && r->start[0] > mins[n][0] && r->start[0] < maxs[n][0]
        && r->start[1] > mins[n][1] && r->start[1] < maxs[n][1]
        && r->start[2] > mins[n][2] && r->start[2] < maxs[n][2]
        && (r->areanum <= 0 || (reached[r->areanum] && comp[r->areanum] != best)) )
        break;
    }
    if ( i < aasworld.reachabilitysize )
      n++;
  }
  return n;
}

static int dk_box_meets(const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs);

/* The strongly connected components of the AI's travel graph into comp; returns the largest. */
static int dk_components(int *comp)
{
  int n = aasworld.numareas, i, k, top = 0, sp = 0, next = 0, ncomp = 0, best = -1, bestsize = 0;
  int *index = (int *)GetClearedMemory(n * sizeof(int)), *low = (int *)GetClearedMemory(n * sizeof(int));
  int *stack = (int *)GetClearedMemory(n * sizeof(int));
  int *work = (int *)GetClearedMemory(2 * n * sizeof(int)), *size = (int *)GetClearedMemory(n * sizeof(int));
  char *onstack = (char *)GetClearedMemory(n);

  /* iterative Tarjan: index 0 means unvisited, comp holds the component number plus one */
  for ( i = 1; i < n; i++ )
  {
    if ( index[i] )
      continue;
    work[0] = i;
    work[1] = 0;
    top = 1;
    while ( top )
    {
      int v = work[2 * (top - 1)], e = work[2 * (top - 1) + 1];
      aas_areasettings_t *as = &aasworld.areasettings[v];

      if ( !e )
      {
        index[v] = low[v] = ++next;
        stack[sp++] = v;
        onstack[v] = 1;
      }
      for ( ; e < as->numreachableareas; e++ )
      {
        aas_reachability_t *r = &aasworld.reachability[as->firstreachablearea + e];
        int w = r->areanum;

        if ( !dk_ai_travel(r->traveltype) || w <= 0 || w >= n )
          continue;
        if ( !index[w] )
          break;
        if ( onstack[w] && index[w] < low[v] )
          low[v] = index[w];
      }
      if ( e < as->numreachableareas )
      {
        work[2 * (top - 1) + 1] = e + 1;
        work[2 * top] = aasworld.reachability[as->firstreachablearea + e].areanum;
        work[2 * top + 1] = 0;
        top++;
        continue;
      }
      if ( low[v] == index[v] )
      {
        int w;

        ncomp++;
        do
        {
          w = stack[--sp];
          onstack[w] = 0;
          comp[w] = ncomp;
          size[ncomp < n ? ncomp : n - 1]++;
        }
        while ( w != v );
      }
      top--;
      if ( top )
      {
        int u = work[2 * (top - 1)];

        if ( low[v] < low[u] )
          low[u] = low[v];
      }
    }
  }
  for ( k = 1; k <= ncomp && k < n; k++ )
  {
    if ( size[k] > bestsize )
    {
      bestsize = size[k];
      best = k;
    }
  }
  FreeMemory(index);
  FreeMemory(low);
  FreeMemory(stack);
  FreeMemory(work);
  FreeMemory(size);
  FreeMemory(onstack);
  return best;
}

static int dk_forbid_traps(bsp_entity_t *ents)
{
  static vec3_t gatemins[DK_MAX_TRAPGATES], gatemaxs[DK_MAX_TRAPGATES];
  int gates, g;
  int n = aasworld.numareas, i, k, sp = 0, best;
  int *comp = (int *)GetClearedMemory(n * sizeof(int)), *stack = (int *)GetClearedMemory(n * sizeof(int));
  char *reached = (char *)GetClearedMemory(n);
  int forbidden = 0;

  best = dk_components(comp);
  /* what the core reaches but cannot leave again */
  sp = 0;
  for ( i = 1; i < n; i++ )
  {
    if ( comp[i] == best )
    {
      reached[i] = 1;
      stack[sp++] = i;
    }
  }
  while ( sp )
  {
    aas_areasettings_t *as = &aasworld.areasettings[stack[--sp]];

    for ( k = 0; k < as->numreachableareas; k++ )
    {
      aas_reachability_t *r = &aasworld.reachability[as->firstreachablearea + k];

      if ( dk_ai_travel(r->traveltype) && r->areanum > 0 && r->areanum < n && !reached[r->areanum] )
      {
        reached[r->areanum] = 1;
        stack[sp++] = r->areanum;
      }
    }
  }
  /* a trap's own ways stay, but not into a teleporter that leads into one */
  gates = dk_trap_gates(ents, comp, reached, best, gatemins, gatemaxs);
  for ( i = 1; i < n; i++ )
  {
    aas_areasettings_t *as = &aasworld.areasettings[i];
    int inside = reached[i] && comp[i] != best;

    for ( k = 0; k < as->numreachableareas; k++ )
    {
      aas_reachability_t *r = &aasworld.reachability[as->firstreachablearea + k];

      if ( !dk_ai_travel(r->traveltype) )
        continue;
      for ( g = 0; g < gates && !dk_box_meets(r->start, r->end, gatemins[g], gatemaxs[g]); g++ )
        ;
      if ( g < gates || (!inside && r->areanum > 0 && r->areanum < n && reached[r->areanum] && comp[r->areanum] != best) )
      {
        r->traveltype = 1;                 /* TRAVEL_INVALID */
        forbidden++;
      }
    }
  }
  FreeMemory(comp);
  FreeMemory(stack);
  FreeMemory(reached);
  return forbidden;
}

/* dkbot: areas with no way into the largest region, whose bots are out of the game */
static char *dk_trapped;
static unsigned char *dk_fatal_was;       /* jump types to restore as a region's only way out */

int DK_AreaTrapped(int areanum)
{
  return dk_trapped && areanum > 0 && areanum < aasworld.numareas && dk_trapped[areanum];
}

/* An area is trapped when no AI link leads from it to an untrapped one. */
static void dk_spread_escape(int *comp)
{
  int n = aasworld.numareas, i, k, best = dk_components(comp), changed;

  for ( i = 1; i < n; i++ )
    dk_trapped[i] = comp[i] != best;
  do
  {
    changed = 0;
    for ( i = 1; i < n; i++ )
    {
      aas_areasettings_t *as = &aasworld.areasettings[i];

      for ( k = 0; dk_trapped[i] && k < as->numreachableareas; k++ )
      {
        aas_reachability_t *r = &aasworld.reachability[as->firstreachablearea + k];

        if ( dk_ai_travel(r->traveltype) && r->areanum > 0 && r->areanum < n && !dk_trapped[r->areanum] )
        {
          dk_trapped[i] = 0;
          changed = 1;
        }
      }
    }
  }
  while ( changed );
}

static int dk_mark_trapped(int *kept)
{
  int n = aasworld.numareas, i, k, trapped = 0, restored;
  int *comp = (int *)GetClearedMemory(n * sizeof(int));

  if ( dk_trapped )
    FreeMemory(dk_trapped);
  dk_trapped = (char *)GetClearedMemory(n);
  *kept = 0;
  do
  {
    dk_spread_escape(comp);
    restored = 0;
    for ( i = 1; i < n; i++ )
    {
      aas_areasettings_t *as = &aasworld.areasettings[i];

      for ( k = 0; dk_trapped[i] && k < as->numreachableareas; k++ )
      {
        int num = as->firstreachablearea + k;
        aas_reachability_t *r = &aasworld.reachability[num];

        if ( dk_fatal_was[num] && r->areanum > 0 && r->areanum < n && !dk_trapped[r->areanum] )
        {
          r->traveltype = dk_fatal_was[num];
          dk_fatal_was[num] = 0;
          restored++;
        }
      }
    }
    *kept += restored;
  }
  while ( restored );
  for ( i = 1; i < n; i++ )
    trapped += dk_trapped[i] && (aasworld.areasettings[i].areaflags & 1);
  FreeMemory(comp);
  return trapped;
}

/* dkbot: BSPC compiles only the world: no link through a standing func_explosive */
#define DK_MAX_EXPLOSIVES  256

static int dk_box_meets(const vec3_t start, const vec3_t end, const vec3_t mins, const vec3_t maxs)
{
  vec3_t bmins, bmaxs;
  float t0 = 0.0f, t1 = 1.0f, a, b, d, lo, hi;
  int k;

  AAS_PresenceTypeBoundingBox(PRESENCE_NORMAL, bmins, bmaxs);
  for ( k = 0; k < 3; k++ )
  {
    lo = mins[k] - bmaxs[k];
    hi = maxs[k] - bmins[k];
    d = end[k] - start[k];
    if ( fabs(d) < 0.001f )
    {
      if ( start[k] <= lo || start[k] >= hi )
        return 0;
      continue;
    }
    a = (lo - start[k]) / d;
    b = (hi - start[k]) / d;
    if ( a > b )
    {
      d = a;
      a = b;
      b = d;
    }
    if ( a > t0 )
      t0 = a;
    if ( b < t1 )
      t1 = b;
    if ( t0 >= t1 )
      return 0;
  }
  return 1;
}

static int dk_forbid_explosives(bsp_entity_t *ents)
{
  static vec3_t mins[DK_MAX_EXPLOSIVES], maxs[DK_MAX_EXPLOSIVES];
  vec3_t noangles = { 0.0f, 0.0f, 0.0f };
  bsp_entity_t *ent;
  char *cn, *model;
  int i, k, n = 0, forbidden = 0, notspawnflags = (int)LibVarValue("notspawnflags", (char *)"2048");

  for ( ent = ents; ent && n < DK_MAX_EXPLOSIVES; ent = ent->next )
  {
    cn = AAS_ValueForBSPEpairKey(ent, "classname");
    model = AAS_ValueForBSPEpairKey(ent, "model");
    /* spawnflag 1: spawned on trigger, 0x200: not solid */
    if ( !cn || strcmp(cn, "func_explosive") || !model || model[0] != '*'
      || (AAS_IntForBSPEpairKey(ent, "spawnflags") & (0x0001 | 0x0200 | notspawnflags)) )
      continue;
    AAS_BSPModelMinsMaxsOrigin(atoi(model + 1), noangles, mins[n], maxs[n], NULL);
    n++;
  }
  for ( i = 1; i < aasworld.reachabilitysize; i++ )
  {
    aas_reachability_t *r = &aasworld.reachability[i];

    if ( !dk_ai_travel(r->traveltype) )
      continue;
    for ( k = 0; k < n && !dk_box_meets(r->start, r->end, mins[k], maxs[k]); k++ )
      ;
    if ( k < n )
    {
      r->traveltype = 1;                 /* TRAVEL_INVALID */
      forbidden++;
    }
  }
  return forbidden;
}

void DK_ForbidFatalJumps(void)
{
  bsp_entity_t *ents, *ent;
  vec3_t noangles = { 0.0f, 0.0f, 0.0f }, p;
  char *cn, *model;
  int i, k, forbidden = 0, drops = 0, crouched = 0, traps, explosive, trapped, kept;

  ents = AAS_ParseBSPEntities();
  explosive = dk_forbid_explosives(ents);
  /* before the passes below: a region whose way out is only a risky jump is no trap */
  traps = dk_forbid_traps(ents);
  dk_numkill = 0;
  for ( ent = ents; ent && dk_numkill < DK_MAX_KILLZONES; ent = ent->next )
  {
    cn = AAS_ValueForBSPEpairKey(ent, "classname");
    model = AAS_ValueForBSPEpairKey(ent, "model");
    if ( !cn || strcmp(cn, "trigger_hurt") || !model || model[0] != '*'
      || AAS_IntForBSPEpairKey(ent, "dmg") < DK_FATAL_HURT )
      continue;
    AAS_BSPModelMinsMaxsOrigin(atoi(model + 1), noangles, dk_killmins[dk_numkill],
                               dk_killmaxs[dk_numkill], NULL);
    dk_numkill++;
  }
  AAS_FreeBSPEntities(ents);
  if ( dk_fatal_was )
    FreeMemory(dk_fatal_was);
  dk_fatal_was = (unsigned char *)GetClearedMemory(aasworld.reachabilitysize);
  for ( i = 1; i < aasworld.reachabilitysize; i++ )
  {
    aas_reachability_t *r = &aasworld.reachability[i];

    if ( r->traveltype == 7 )            /* TRAVEL_WALKOFFLEDGE */
    {
      /* the fall is at the landing point, from the ledge's height */
      VectorSet(p, r->end[0], r->end[1], r->start[2]);
      if ( dk_fatal_fall(p) )
      {
        r->traveltype = 1;               /* TRAVEL_INVALID */
        drops++;
      }
      continue;
    }
    if ( r->traveltype != 5 )            /* TRAVEL_JUMP */
      continue;
    for ( k = 1; k <= 3; k++ )
    {
      p[0] = r->start[0] + 0.25f * k * (r->end[0] - r->start[0]);
      p[1] = r->start[1] + 0.25f * k * (r->end[1] - r->start[1]);
      p[2] = r->start[2] > r->end[2] ? r->start[2] : r->end[2];
      if ( dk_fatal_fall(p) )
        break;
    }
    if ( k <= 3 )
    {
      dk_fatal_was[i] = r->traveltype;
      r->traveltype = 1;                 /* TRAVEL_INVALID */
      forbidden++;
    }
  }
  /* dkbot: bots climb ladders and take teleporters standing: no crouch-only ones */
  for ( i = 1; i < aasworld.numareas; i++ )
  {
    aas_areasettings_t *as = &aasworld.areasettings[i];

    for ( k = 0; k < as->numreachableareas; k++ )
    {
      aas_reachability_t *r = &aasworld.reachability[as->firstreachablearea + k];

      if ( (r->traveltype == 6 && !(aasworld.areasettings[r->areanum].presencetype & PRESENCE_NORMAL))
        || (r->traveltype == 10 && !(as->presencetype & PRESENCE_NORMAL)) )
      {
        r->traveltype = 1;                 /* TRAVEL_INVALID */
        crouched++;
      }
    }
  }
  trapped = dk_mark_trapped(&kept);
  botimport.Print(PRT_MESSAGE, "%d fatal jumps and %d fatal drops forbidden, %d kill zones, "
                  "%d crouched ladder or teleporter links, %d links through breakables, %d links into traps, "
                  "%d grounded areas without a way back, %d risky jumps kept as the only way out\n",
                  forbidden, drops, dk_numkill, crouched, explosive, traps, trapped, kept);
}
