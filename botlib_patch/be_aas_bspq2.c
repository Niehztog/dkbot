/*
 * be_aas_bspq2.c — Gladiator Bot v0.96 botlib (Mr. Elusive, 1999), reconstructed
 * from the Windows gladiator.dll.  DLL extent 0x10003010..0x100085F0.
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
#include "be_aas_bspq2.h"
#include "be_aas_entity.h"
#include "be_aas_light.h"
#include "be_aas_main.h"
#include "be_interface.h"
#include "l_libvar.h"
#include "l_memory.h"
#include "l_script.h"
#include "l_utils.h"

/* dkbot: the engine's map, converted in memory (dk_maps.c) */
FILE *DK_OpenBSP(const char *name);

/* Type + offset asserts live in be_aas_bspq2.h, the single instance here. */
bspworld_t bspworld;

// gladiator.dll: 10003010..10003056
// gladi386.so:   0000A3F8..0000A442
/* Returns bsp_trace_t BY VALUE; MSVC lowers that to a hidden caller-allocated
 * return buffer.
 *
 * THE ENGINE TRACE, on both platforms.  It used to be `#ifdef _WIN32`-split,
 * with this body on Windows and a direct `AAS_TraceBSPModel(0, ...)` body on
 * Linux, on the reading that "the two 1999 images have genuinely different
 * bodies here".  That reading was WRONG, and the correction is worth
 * understanding because it was invisible to both audits (they mask call
 * displacements):
 *
 *   - gladi386.so has BOTH bodies.  F663 (0xa3f8, 74 B) is this one -- it
 *     loads a GOT slot that `.rel.got` names `botimport` and calls
 *     `[edi+0xc]` -- and ALL 30 of the .so's trace call sites go to it.
 *   - F680 (0xd0cc, 106 B) is the direct-AAS_TraceBSPModel body, and it is
 *     called by NOBODY.  We already had it, as sub_10005640 below, which the
 *     DLL likewise keeps uncalled at 0x10005640.
 *
 * So the Linux build was routing every bot trace into the world BSP model
 * alone, where the 1999 build went through the engine and therefore saw
 * entities and movers.  That is a behavioural divergence, not only a byte
 * one.  (x87cmp/soannotate/contentsweep, 2026-08-16.) */
bsp_trace_t __cdecl AAS_Trace(vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int passent, int contentmask)
{
  return botimport.Trace(start, mins, maxs, end, passent, contentmask);
}

/* BSP-leaf link heap.  Gladiator carries TWO link families and this is the
 * BSP-leaf one; the AAS-area trio at 0x1001AC00/0x1001AD50/0x1001ADA0 already
 * owns the Q3 cognate names (AAS_InitAASLinkHeap etc.).  They are distinguished
 * at every site: "max_bsplinks" vs "max_aaslinks", "empty bsp link heap" vs
 * "empty aas link heap", link->prev_leaf vs link->prev_area.  A verbatim
 * cognate name is impossible, so this trio stays unnamed — do NOT resolve it by
 * renaming the 0x1001ACxx family. */
// gladiator.dll: 10003080..1000308F
// gladi386.so:   0000A444..0000A466
/* Q3 be_aas_bspq3.c's AAS_PointContents: `return botimport.PointContents(point);`.
 * All 20 of the original's contents queries go through it (codegen_levers.md). */
int __cdecl AAS_PointContents(vec3_t point)
{
  return botimport.PointContents(point);
}

// gladiator.dll: 100030A0..10003162
// gladi386.so:   0000A468..0000A722
void sub_100030A0()
{
  int i;
  int count = bspworld.dword_1006957C;

  if ( !bspworld.dword_10069578 )
  {
    count = (intptr_t)LibVarValue("max_bsplinks", (char *)"4096");
    if ( count < 0 )
      count = 0;
    bspworld.dword_1006957C = count;
    bspworld.dword_10069578 = (bsp_link_t *)GetMemory(sizeof(bsp_link_t) * count);
  }
  /* Doubly-linked free list via next_ent / prev_ent slots. */
  bspworld.dword_10069578[0].prev_ent = NULL;
  bspworld.dword_10069578[0].next_ent = &bspworld.dword_10069578[1];
  for ( i = 1; i < count - 1; ++i )
  {
    bspworld.dword_10069578[i].prev_ent = &bspworld.dword_10069578[i - 1];
    bspworld.dword_10069578[i].next_ent = &bspworld.dword_10069578[i + 1];
  }
  bspworld.dword_10069578[count - 1].prev_ent = &bspworld.dword_10069578[count - 2];
  bspworld.dword_10069578[count - 1].next_ent = NULL;
  bspworld.dword_10069580 = bspworld.dword_10069578;
}

// gladiator.dll: 100031B0..100031DA
// gladi386.so:   0000A724..0000A76B
// Walks the bsp_link freelist headed at dword_10069580 and prints
// "%d free bsp links, %s\n".  DEAD in Gladiator.
void __cdecl sub_100031B0(char *name)
{
  bsp_link_t *node;
  int count;

  count = 0;
  for ( node = bspworld.dword_10069580; node; node = node->next_ent )
    ++count;
  botimport.Print(PRT_MESSAGE, "%d free bsp links, %s\n", count, name);
}

// gladiator.dll: 100031F0..10003221
// gladi386.so:   0000A76C..0000A7BA
bsp_link_t *sub_100031F0()
{
  bsp_link_t *result;
  bsp_link_t *next;

  result = bspworld.dword_10069580;
  if ( !result )
  {
    botimport.Print(PRT_FATAL, "empty bsp link heap\n");
    return NULL;
  }
  next = result->next_ent;
  bspworld.dword_10069580 = next;
  if ( next )
    next->prev_ent = NULL;
  return result;
}

// gladiator.dll: 10003240..1000326B
// gladi386.so:   0000A7BC..0000A80C
void __cdecl sub_10003240(bsp_link_t *a1)
{
  if ( bspworld.dword_10069580 )
    bspworld.dword_10069580->prev_ent = a1;
  a1->prev_ent = NULL;
  a1->next_ent = bspworld.dword_10069580;
  a1->prev_leaf = NULL;
  a1->next_leaf = NULL;
  bspworld.dword_10069580 = a1;
}

// gladiator.dll: 10003280..100032B6
// gladi386.so:   0000A80C..0000A85C
void sub_10003280()  /* InitBSPLinkedEntities */
{
  if ( bspworld.dword_100674C0 )
  {
    if ( bspworld.dword_10069584 )
      FreeMemory(bspworld.dword_10069584);
    bspworld.dword_10069584 = (bsp_link_t **)GetClearedMemory(sizeof(bsp_link_t *) * bspworld.numleafs);
  }
}

// gladiator.dll: 100032D0..10003333
// gladi386.so:   0000A85C..0000A8F3
void sub_100032D0()
{
  if ( bspworld.dword_100674C0 )
  {
    if ( bspworld.dword_1006755C )
      FreeMemory(bspworld.dword_1006755C);
    bspworld.dword_1006755C = GetClearedMemory(4 * bspworld.numareaportals);
    if ( bspworld.dword_10067560 )
      FreeMemory(bspworld.dword_10067560);
    bspworld.dword_10067560 = GetClearedMemory(bspworld.numareas * bspworld.numareas * 4);
  }
}

// gladiator.dll: 10003360..100033E5
// gladi386.so:   0000A8F4..0000A990
/* Q3 engine cognate CM_PointLeafnum_r (qcommon/cm_test.c).  Q3 hardcodes the
 * world model's root; this takes an explicit modelnum and starts from
 * dmodels[modelnum].headnode, so inline sub-models work too. */
int __cdecl CM_PointLeafnum(const vec3_t point, int modelnum)
{
  vec_t d;
  dplane_t *plane;
  int node; // ecx

  if ( !bspworld.dword_100674C0 )
    return 0;
  node = bspworld.dmodels[modelnum].headnode;
  if ( node >= 0 )
  {
    do
    {
      dnode_t *dn = &bspworld.dnodes[node];
      int planenum = dn->planenum;
      plane = &bspworld.dplanes[planenum];
      /* `point` FIRST, not `plane->normal`: with the loop-INVARIANT operand
       * leading, gcc 2.7 emits `fld point[i]` as its own insn and hoists all
       * three out of the descent loop.  MSVC6 canonicalises either spelling.
       * NB not the AAS_PointAreaNum dot-product conflict — that one is about
       * term STAGING order. */
      d = DotProduct(point, plane->normal) - plane->dist;
      if ( d > 0 )
        node = dn->children[0];
      else
        node = dn->children[1];
    }
    while ( node >= 0 );
  }
  return -1 - node;
}

// gladiator.dll: 10003420..10003450
// gladi386.so:   0000A990..0000AA3F
dleaf_t *__cdecl sub_10003420(const vec3_t point, int modelnum)
{
  if ( !bspworld.dword_100674C0 )
    return 0;
  return &bspworld.dleafs[CM_PointLeafnum(point, modelnum)];
}

// gladiator.dll: 10003460..100034AF
// gladi386.so:   0000AA40..0000AAB1
/* `RotatePoint(vec3_t point, float m[3][3])`, declared in Q3's bspc/l_math.h --
 * Mr Elusive's own name, not an invention.
 *
 * This USED to be two functions: `sub_10003460` here, written with scalar
 * x/y/z temporaries, and a second `#ifndef _WIN32`-gated `RotatePoint` at the
 * end of the file carrying the Q3 spelling.  They are one function.  The
 * duplicate was created because the ELF funcmap had F668 (0xa7bc, 80 B) paired
 * with sub_10003460 -- a pairing its own override row already flagged WEAK for
 * "19 real insns vs 33 ours" -- when F668 is actually a linked-list prepend on
 * `bspworld` (+0x20c0), i.e. sub_10003240.  The real RotatePoint is F673
 * (0xaa40, 113 B), which sits between F672=sub_10003420 and F674=AnglesToAxis
 * exactly as 0x10003460 sits between 0x10003420 and 0x100034D0 in the DLL.
 *
 * The `vec3_t tvec` + DotProduct spelling is the one that matters: it is
 * byte-identical to F673 on the ELF, where the scalar form is not.  MSVC6 /O2
 * compiles the two spellings BYTE-IDENTICALLY (probe_cl.sh, 32 insns each), so
 * there is no two-oracle conflict here -- the PE keeps its MATCH either way.
 * (2026-08-16.) */
void __cdecl RotatePoint(vec3_t point, float matrix[3][3])
{
  vec3_t tvec;

  VectorCopy(point, tvec);
  point[0] = DotProduct(matrix[0], tvec);
  point[1] = DotProduct(matrix[1], tvec);
  point[2] = DotProduct(matrix[2], tvec);
} //end of the function RotatePoint

// gladiator.dll: 100034D0..10003616
// gladi386.so:   0000AAB4..0000AC61
/* Build a 3x3 row-major rotation matrix from angles[PITCH,YAW,ROLL]:
 * output = roll_m * pitch_m * yaw_m.
 *
 *   yaw_m   = [[ cy, sy,0],[-sy,cy,0],[0,0,1]]   ; around Z
 *   pitch_m = [[ cp, 0,-sp],[0, 1,0],[sp,0,cp]]  ; around Y
 *   roll_m  = [[ 1, 0, 0],[0,cr,sr],[0,-sr,cr]]  ; around X
 *
 * Degrees->radians uses the unfolded `angle * M_PI * 2 / 360` idiom with no
 * grouping parens: `angle` is a runtime value inside the left-associative parse
 * tree, so gcc cannot constant-fold M_PI*2 and keeps PI resident on the x87
 * stack for the whole function. */
void __cdecl AnglesToAxis(const vec3_t angles, float axis[3][3])
{
  /* Only THREE matrix buffers: the roll matrix is rebuilt in `yaw`, which is dead
     after the first concat.  Both originals say so -- the DLL and the .so each pass
     the yaw buffer as the second R_ConcatRotations' first argument -- and the .so's
     frame (yaw highest, then pitch, then tmp) records this declaration order. */
  float angle;
  float sp, cp;
  float sy, cy;
  float sr, cr;
  float yaw[3][3];
  float pitch[3][3];
  float tmp[3][3];

  /* Row-major, constant cells interleaved in place rather than seeded up front. */
  angle = angles[1] * M_PI*2 / 360;
  sy = (float)sin(angle);
  cy = (float)cos(angle);
  yaw[0][0] = cy;
  yaw[0][1] = sy;
  yaw[0][2] = 0;
  yaw[1][0] = -sy;
  yaw[1][1] = cy;
  yaw[1][2] = 0;
  yaw[2][0] = 0;
  yaw[2][1] = 0;
  yaw[2][2] = 1;

  /* pitch matrix (around Y), same row-major/interleaved shape as yaw. */
  angle = angles[0] * M_PI*2 / 360;
  sp = (float)sin(angle);
  cp = (float)cos(angle);
  pitch[0][0] = cp;
  pitch[0][1] = 0;
  pitch[0][2] = -sp;
  pitch[1][0] = 0;
  pitch[1][1] = 1;
  pitch[1][2] = 0;
  pitch[2][0] = sp;
  pitch[2][1] = 0;
  pitch[2][2] = cp;

  /* tmp = pitch_m * yaw_m.  The roll angle and matrix are computed only AFTER
     this call, so the roll matrix can reuse the now-dead yaw buffer. */
  R_ConcatRotations(pitch, yaw, tmp);

  /* roll matrix (rotation around X), rebuilt in `yaw`, same row-major/interleaved
     shape as yaw and pitch. */
  angle = angles[2] * M_PI*2 / 360;
  sr = (float)sin(angle);
  cr = (float)cos(angle);
  yaw[0][0] = 1;
  yaw[0][1] = 0;
  yaw[0][2] = 0;
  yaw[1][0] = 0;
  yaw[1][1] = cr;
  yaw[1][2] = sr;
  yaw[2][0] = 0;
  yaw[2][1] = -sr;
  yaw[2][2] = cr;

  /* output = roll_m * tmp */
  R_ConcatRotations(yaw, tmp, axis);
}

// gladiator.dll: 10003680..10003AC7
// gladi386.so:   0000AC64..0000B34F
qboolean __cdecl AAS_EntityCollision(int entnum, vec3_t start, vec3_t boxmins, vec3_t boxmaxs, vec3_t end, int contentmask, bsp_trace_t *trace)
{
  /* The box test is Q3's AAS_ClipToBBox, same text and names, inline for SOLID_BBOX
   * entities, with the SOLID_BSP model trace nested under the same hit test.  The
   * .so orders the trace-field stores startsolid, allsolid, fraction, ..., ent.
   * IDA's version (a while(1) axis loop, absmins/absmaxs declared the other way
   * round) cost ELF OUR-68 and PE 148 lines; this matches the DLL.  The .so is still
   * OUR+15: reload spills `trace` out of edi to serve the spilled-parameter init
   * insns because its pseudo has one reference fewer than `start` (36 vs 37 uses,
   * order_regs_for_reload), where the original evidently had the reverse. */
  int i, j, side;
  float front, back, frac, planedist;
  vec3_t absmins, absmaxs, dir;
  vec3_t mid;
  bsp_trace_t enttrace;
  bsp_entdata_t entdata;

  if ( !bspworld.dword_100674C0 )
    return 0;
  AAS_EntityBSPData(entnum, &entdata);
  if ( entdata.solid != 2 && entdata.solid != 3 )
    return 0;
  if ( boxmaxs ) VectorSubtract(entdata.absmins, boxmaxs, absmins);
  else VectorCopy(entdata.absmins, absmins);
  if ( boxmins ) VectorSubtract(entdata.absmaxs, boxmins, absmaxs);
  else VectorCopy(entdata.absmaxs, absmaxs);
  for (i = 0; i < 3; i++)
  {
    if (start[i] < absmins[i] && end[i] < absmins[i]) break;
    if (start[i] > absmaxs[i] && end[i] > absmaxs[i]) break;
  } //end for
  if (i != 3) return 0;
  if ( entdata.solid == 2 )
  {
    /* 0.5 is a DOUBLE (fadd QWORD 0.5).  Keep 0.5, not 0.5f. */
    for (i = 0; i < 3; i++)
    {
      if (start[i] <= absmins[i] + 0.5) break;
      if (start[i] >= absmaxs[i] - 0.5) break;
    } //end for
    /* Relational, not equality: `cmp edx,3; jl`.  `== 3` emits `jne` and drops the
     * skip-jump.  The FIRST bounds loop above does use `!= 3`. */
    if (i >= 3)
    {
      trace->startsolid = 1;
      trace->allsolid = 1;
      trace->fraction = 0.0f;
      trace->contents = 0;
      trace->sidenum = -1;
      trace->ent = entnum;
      /* Keep the memset (an inlined 20-byte one): five scalar `planeints[i] = 0`
       * stores would fold the base into the addressing mode instead. */
      memset(&trace->plane, 0, sizeof(trace->plane));
      VectorCopy(start, trace->endpos);
      return 1;
    } //end if
  } //end if
  //check bounding box collision
  VectorSubtract(end, start, dir);
  for (i = 0; i < 3; i++)
  {
    //get plane to test collision with for the current axis direction
    if (dir[i] > 0) planedist = absmins[i];
    else planedist = absmaxs[i];
    //calculate collision fraction
    front = start[i] - planedist;
    back = end[i] - planedist;
    frac = front / (front-back);
    //check if between bounding planes of next axis
    side = i + 1;
    if (side > 2) side = 0;
    mid[side] = start[side] + dir[side] * frac;
    if (mid[side] > absmins[side] && mid[side] < absmaxs[side])
    {
      //check if between bounding planes of next axis
      side++;
      if (side > 2) side = 0;
      mid[side] = start[side] + dir[side] * frac;
      if (mid[side] > absmins[side] && mid[side] < absmaxs[side])
      {
        mid[i] = planedist;
        break;
      } //end if
    } //end if
  } //end for
  //if there was a collision
  if (i != 3 && frac < trace->fraction)
  {
    if ( entdata.solid == 2 )
    {
      trace->startsolid = 0;
      trace->allsolid = 0;
      trace->fraction = frac;
      trace->sidenum = -1;
      trace->ent = entnum;
      if ( boxmins && boxmaxs )
      {
        if (dir[i] > 0) trace->exp_dist = boxmaxs[i];
        else trace->exp_dist = -boxmins[i];
      } //end if
      //trace endpos
      for (j = 0; j < 3; j++) trace->endpos[j] = start[j] + dir[j] * frac;
      trace->plane.normal[(i + 1) % 3] = 0;
      trace->plane.normal[(i + 2) % 3] = 0;
      if (dir[i] > 0) trace->plane.normal[i] = -1;
      else trace->plane.normal[i] = 1;
      if (dir[i] > 0) planedist = -trace->endpos[i];
      else planedist = trace->endpos[i];
      trace->plane.dist = planedist - trace->exp_dist;
      trace->plane.type = i;
      return 1;
    } //end if
    if ( entdata.solid == 3 )
    {
      enttrace = AAS_TraceBSPModel(entdata.modelnum, entdata.origin, entdata.angles, start, boxmins, boxmaxs, end, 0, contentmask);
      if ( enttrace.fraction < trace->fraction )
      {
        memcpy(trace, &enttrace, sizeof(enttrace));
        return 1;
      } //end if
    } //end if
  } //end if
  return 0;
}

// gladiator.dll: 10003BF0..10003C69
// gladi386.so:   0000B350..0000B3CE
int __cdecl sub_10003BF0(int leafnum, vec3_t start, vec3_t boxmins, vec3_t boxmaxs, vec3_t end, int passent, int contentmask, bsp_trace_t *trace)
{
  bsp_link_t *i; // esi
  int v10; // [esp+0h] [ebp-4h]

  if ( !bspworld.dword_100674C0 )
    return 0;
  v10 = 0;
  for ( i = bspworld.dword_10069584[leafnum]; i; i = i->next_ent )
  {
    if ( i->entnum != passent )
    {
      if ( AAS_EntityCollision(i->entnum, start, boxmins, boxmaxs, end, contentmask, trace) )
        v10 = 1;
    }
  }
  return v10;
}

// gladiator.dll: 10003C90..100041BC
// gladi386.so:   0000B3D0..0000BA06
/* Q3 engine cognate CM_TraceThroughBrush (qcommon/cm_trace.c): iterate a brush's
 * sides, expand each plane by the trace box, track the enter/leave fraction.  No
 * capsule/sphere collision — Q3 added that later. */
int __cdecl CM_TraceThroughBrush(
        dbrush_t *a1,
        float *a2,
        float *a3,
        float *a4,
        float *a5,
        float *a6,
        float *a7,
        float *a8,
        _DWORD *a9,
        float *a10,
        float *a11)
{
  float v59[3][3]; // [esp+68h] [ebp-24h] BYREF
  vec3_t vec; // [esp+5Ch] [ebp-30h] BYREF — line vec (VectorLength input)
  vec3_t dir; // [esp+50h] [ebp-3Ch] BYREF — clipped-distance vec (VectorLength input)
  vec3_t startp; // [esp+2Ch] [ebp-60h] — clipped start point
  vec3_t endp; // [esp+44h] [ebp-48h] — clipped end point
  vec3_t normal; // [esp+38h] [ebp-54h] BYREF — plane normal (RotatePoint input/output)
  int v11; // edi
  /* v16: BSP plane pointer — an `int` would truncate dplanes on 64-bit. */
  dplane_t *v16;
  int v17; // ecx
  float v18; // st7
  float v19; // st7
  float v20; // st6
  float v25; // st5
  float v29; // st7
  float v30; // st7
  float v31; // st7
  float v35; // [esp+10h] [ebp-7Ch]
  int v36; // [esp+14h] [ebp-78h]
  float v37; // [esp+14h] [ebp-78h]
  float v38; // [esp+18h] [ebp-74h]
  int v39; // [esp+1Ch] [ebp-70h]
  int v42; // [esp+24h] [ebp-68h]
  float v41; // [esp+20h] [ebp-6Ch]
  int v40; // [esp+20h] [ebp-6Ch]
  float v43; // [esp+28h] [ebp-64h]

  /* Truth-value assignments: gcc 2.7 expands `x = a || b || c` as "clear x,
   * test, set 1" -- the .so's store-before-test -- while cl.exe sinks the 0
   * into the last test exactly as it does for Q3's if/else `rotated`.  The
   * if/else costs the .so, `x = 0; if (...) x = 1;` costs the DLL. */
  v39 = a3[0] || a3[1] || a3[2];
  if ( v39 )
    AnglesToAxis(a3, v59);
  v40 = a2[0] || a2[1] || a2[2];
  /* One shared zero, hoisted here with the other zero initialisers. */
  v11 = 0;
  v42 = 0;
  v36 = 0;
  VectorCopy(a4, startp);
  VectorCopy(a7, endp);
  if ( a1->numsides > 0 )
  {
    while ( 1 )
    {
      v16 = &bspworld.dplanes[bspworld.dbrushsides[v11 + a1->firstside].planenum];
      if ( v39 )
      {
        VectorCopy(v16->normal, normal);
        RotatePoint(normal, v59);
        v17 = 4;
      }
      else
      {
        v17 = v16->type;
        VectorCopy(v16->normal, normal);
      }
      if ( v40 )
      {
        if ( v17 < 3 )
        {
          if ( normal[v17] > 0.0f )
            v18 = a2[v17] + v16->dist;
          else
            v18 = v16->dist - a2[v17];
          v38 = v18;
        }
        else
        {
          v38 = DotProduct(a2, normal) + v16->dist;
        }
      }
      else
      {
        v38 = v16->dist;
      }
      if ( v17 < 3 )
      {
        if ( a5 )
        {
          if ( a6 )
          {
            /* Positive guard: the `normal>0` arm is the warm fall-through and the
             * `<=0` arm the cold jump target.  Inverting it swaps the blocks. */
            if ( normal[v17] > 0.0f )
            {
              v19 = -a5[v17];
            }
            else
            {
              v19 = a6[v17];
            }
            goto LABEL_30;
          }
        }
        v19 = 0.0;
LABEL_30:
        v20 = v19 + v38;
        /* As above: the un-negated `normal>0` arm is the fall-through. */
        if ( normal[v17] > 0.0f )
        {
          v35 = startp[v17] - v20;
          v25 = endp[v17];
        }
        else
        {
          v35 = -startp[v17] - v20;
          v25 = -endp[v17];
        }
      }
      else
      {
        if ( a5 && a6 )
        {
          /* Per component, select a5 or a6 by the sign of normal[i].  Test
           * positively (the a5 arm is inline) and use two assignment STATEMENTS,
           * not a ternary — a float-valued ternary routes the copy through the
           * x87 stack where the original uses integer movs. */
          {
            int _k;
            for (_k = 0; _k < 3; _k++)
            {
              if ( normal[_k] > 0.0f )
                vec[_k] = a5[_k];
              else
                vec[_k] = a6[_k];
            }
          }
          dir[0] = -normal[0];
          dir[1] = -normal[1];
          dir[2] = -normal[2];
          v11 = v36;
          v19 = DotProduct(vec, dir);
        }
        else
        {
          v19 = 0.0;
        }
        v20 = v19 + v38;
        /* DotProduct, as Q3's d1/d2: the [2]-first spelling IDA shows is only
         * cl.exe's evaluation order, which the macro gives it anyway. */
        v35 = DotProduct(startp, normal) - v20;
        v25 = DotProduct(endp, normal);
      }
      v37 = v25 - v20;
      if ( v35 > -0.005 && v37 > -0.005 )
        return 0;
      if ( v35 >= 0.005 || v37 >= 0.005 )
      {
        if ( v35 > -0.005 )
        {
          /* v42 before v43: the .so computes the side index first, then copies the
           * spilled v19 (gcc 2.7 does not schedule, so this is statement order). */
          v42 = v11 + a1->firstside;
          v43 = v19;
        }
        if ( v35 > 0.005 )
        {
          v29 = v35 / (v35 - v37);
          startp[0] = (endp[0] - startp[0]) * v29 + startp[0];
          startp[1] = (endp[1] - startp[1]) * v29 + startp[1];
          startp[2] = (endp[2] - startp[2]) * v29 + startp[2];
        }
        else if ( v37 > 0.005 )
        {
          v30 = v35 / (v35 - v37);
          endp[0] = (endp[0] - startp[0]) * v30 + startp[0];
          endp[1] = (endp[1] - startp[1]) * v30 + startp[1];
          endp[2] = (endp[2] - startp[2]) * v30 + startp[2];
        }
      }
      v36 = ++v11;
      if ( v11 >= a1->numsides )
      {
        break;
      }
    }
  }
  VectorSubtract(a7, a4, vec);
  VectorSubtract(startp, a4, dir);
  v41 = VectorLength(dir);
  v31 = v41 / VectorLength(vec);
  if ( v31 < *a8 )
  {
    *a8 = v31;
    *a9 = v42;
    *a10 = v43;
    /* ONE grouped vec3 copy.  [1] and [2] are copied with integer movs, which a
     * float-typed local would prevent. */
    VectorCopy(startp, a11);
    return 1;
  }
  return 0;
}

// gladiator.dll: 10004310..1000448D
// gladi386.so:   0000BA08..0000BBBA
/* Q3 engine cognate CM_TraceThroughLeaf (qcommon/cm_trace.c).  No patch/curve
 * surfaces in Q2, so Q3's second leafsurfaces loop is absent. */
int __cdecl CM_TraceThroughLeaf(int leafnum, vec3_t origin, vec3_t angles, vec3_t start, vec3_t boxmins, vec3_t boxmaxs, vec3_t end, int contentmask, bsp_trace_t *trace)
{
  int v9; // ebp
  /* v11: BSP leaf pointer for leaf `a1`. */
  dleaf_t *v11;
  dbrush_t *v13; // edi
  int v16; // ecx
  /* v17: BSP plane pointer for the hit brush side. */
  dplane_t *v17;
  float v20; // [esp+10h] [ebp-10h] BYREF — expanded plane dist filled by CM_TraceThroughBrush
  vec3_t endpos; // [esp+14h] [ebp-Ch] BYREF — endpoint filled by CM_TraceThroughBrush via a11
  int sidenum; // [esp+20h] [ebp+0h]
  dbrush_t *v24; // [esp+24h] [ebp+4h]

  v11 = &bspworld.dleafs[leafnum];
  v9 = 0;
  v24 = 0;
  /* Compare against the loop counter v9 (=0), not a literal 0: that is what
   * yields `cmp WORD,bp; jbe` instead of `je`.  Same skip-when-empty result. */
  if ( v11->numleafbrushes <= (unsigned int)v9 )
    goto fail;
  do
  {
    int brushnum = bspworld.dleafbrushes[v9 + v11->firstleafbrush];
    v13 = &bspworld.dbrushes[brushnum];
    if ( (v13->contents & contentmask) != 0
      && CM_TraceThroughBrush(v13, origin, angles, start, boxmins, boxmaxs, end, &trace->fraction, (_DWORD *)&sidenum, &v20, endpos) )
    {
      v24 = v13;
    }
    ++v9;
  }
  while ( v9 < v11->numleafbrushes );
  /* `if (v24)` rather than `if (!v24) goto fail`, so the fill code is the warm
   * fall-through and the shared return-0 stays a cold tail block. */
  if ( v24 )
  {
  if ( endpos[0] == start[0] && endpos[1] == start[1] && endpos[2] == start[2] )
  {
    trace->allsolid   = 1;
    trace->startsolid = 1;
    trace->fraction   = 0;
  }
  else
  {
    trace->allsolid   = 0;
    trace->startsolid = 0;
  }
  VectorCopy(endpos, trace->endpos);
  v16 = sidenum;
  trace->sidenum = v16;
  v17 = &bspworld.dplanes[bspworld.dbrushsides[v16].planenum];
  VectorCopy(v17->normal, trace->plane.normal);
  trace->plane.dist      = v17->dist;
  trace->plane.type      = v17->type;
  trace->exp_dist = v20;
  trace->contents = v24->contents;
  return 1;
  }
fail:
  /* Both early-out guards share this ONE return-0 epilogue; a `return 0;` at
   * each guard would emit two copies. */
  return 0;
}

typedef struct bsp_model_tracestack_s {
  vec3_t start;
  vec3_t end;
  int nodenum;
  int planenum;
  float planedist;
  int next;   /* raw 32-bit pointer in the original DLL; offset-encoded on 64-bit */
} bsp_model_tracestack_t;

_Static_assert(sizeof(bsp_model_tracestack_t) == 40, "bsp_model_tracestack_t size");
// gladiator.dll: 100044F0..100052B4
// gladi386.so:   0000BBBC..0000D0C9
/* Sweep a box from start to end through BSP model `modelnum` at
 * `modelorigin`/`angles` — Gladiator's own Q2-era BSP-model collision (Q3
 * delegates BSP traces to the engine).  The pieces of the line live in a
 * 128-entry trace stack threaded onto a free list and a trace list; the trace
 * list is kept sorted along the line's major axis so the pieces nearest the start
 * are traced first.  Returns bsp_trace_t by value (hidden return buffer).
 *
 * RESTORED FROM gladi386.so, 2026-09-28 (byte-identical there).  IDA's version,
 * decompiled from the DLL, had folded the source into a goto web and lost the
 * shapes that decide gcc 2.7's code; each of these was read off the .so:
 *   - every other-side index is `!side` written inline (gcc builds each one
 *     with a branch: `xor eax,eax; cmp side,0; jne; mov eax,8`), not a
 *     precomputed `side_b`;
 *   - `sideflags` is int[2][2] [box offset][side] -- the row addressing is what
 *     makes gcc hoist `&sideflags` into a register;
 *   - every push takes a fresh entry off the free list behind its own
 *     `if (!freelist) break;` (IDA had reused the entry just freed);
 *   - the flags are truth values (`rotated = a||b||c`, `side = dir[type] < 0`),
 *     the no-box split has its own `side2`, `positive` is an if/else;
 *   - the negated normal is a real vec3_t, and the dot products put the memory
 *     operand first where the .so reloads it (`plane->dist + DotProduct(...)`,
 *     `DotProduct(cur_start, normal)`);
 *   - declaration order is the .so's frame order, highest address first. */
bsp_trace_t __cdecl AAS_TraceBSPModel(
        int modelnum,
        const vec3_t modelorigin,
        vec3_t angles,
        vec3_t start,
        vec3_t boxmins,
        vec3_t boxmaxs,
        vec3_t end,
        int passent,
        int contentmask)
{
  int sideflags[2][2];
  float axis[3][3];
  vec3_t cur_start, cur_end, mid1, mid2, v1, v2, dir, normal, origin, invnormal;
  bsp_model_tracestack_t tracestack[128];
  bsp_trace_t trace;
  float frontd[2], offsets[2], backd[2];
  int i, leafnum, planenum, sortaxis, positive, rotated, translated, side, side2, nodenum, type;
  float front, back, frac1, frac2, dist, planedist;
  bsp_model_tracestack_t *tstack_p, *tracelist, *freelist, *ts, *prev;
  dnode_t *node;
  dplane_t *plane;
  dleaf_t *leaf;
  /* The trace-stack lists keep their next-pointer in 4-byte int slots.  On 32-bit
   * a pointer fits and TR_ENC/TR_DEC are the identity; on 64-bit each link becomes
   * a byte offset into tracestack, stored as offset+1 so 0 stays NULL.
   * MSVC6 does not define __SIZEOF_POINTER__, hence the `!defined` clause. */
#if !defined(__SIZEOF_POINTER__) || __SIZEOF_POINTER__ == 4
  #define TR_ENC(p) ((int)(intptr_t)(p))
  #define TR_DEC(i) ((bsp_model_tracestack_t *)(intptr_t)(i))
#else
  #define TR_ENC(p) ((p) ? (int)(((intptr_t)(p) - (intptr_t)tracestack) + 1) : 0)
  #define TR_DEC(i) ((i) ? (bsp_model_tracestack_t *)((intptr_t)tracestack + ((unsigned int)(i) - 1u)) : (bsp_model_tracestack_t *)0)
#endif

  memset(&trace, 0, sizeof(trace));
  VectorCopy(end, trace.endpos);
  trace.allsolid = 0;
  trace.startsolid = 0;
  trace.fraction = 1.0f;
  if ( !bspworld.dword_100674C0 )
    return trace;
  VectorSubtract(end, start, dir);
  //the axis along which the pieces of the trace line are sorted
  if ( dir[0] > dir[1] )
  {
    if ( dir[0] > dir[2] ) sortaxis = 0;
    else sortaxis = 2;
  }
  else
  {
    if ( dir[1] > dir[2] ) sortaxis = 1;
    else sortaxis = 2;
  }
  if ( dir[sortaxis] > 0 ) positive = 1;
  else positive = 0;
  rotated = angles[0] || angles[1] || angles[2];
  if ( rotated )
    AnglesToAxis(angles, axis);
  VectorAdd(modelorigin, bspworld.dmodels[modelnum].origin, origin);
  translated = origin[0] || origin[1] || origin[2];
  //initialize the free list
  for ( i = 0; i < 127; i++ )
    tracestack[i].next = TR_ENC(&tracestack[i + 1]);
  tracestack[127].next = 0;
  freelist = tracestack;
  tracelist = NULL;
  //the whole trace line is the first piece on the trace list.  The same take-a-free-
  //entry code as every push below, NULL test included: gcc 2.7 folds the test away
  //(freelist is a frame address), cl.exe keeps it (`lea eax,[esp+...]; test eax,eax`).
  tstack_p = freelist;
  if ( !tstack_p )
  {
    botimport.Print(PRT_ERROR, "AAS_TraceBSPModel: out of trace lines\n");
    return trace;
  }
  freelist = TR_DEC(tstack_p->next);
  VectorCopy(start, tstack_p->start);
  VectorCopy(end, tstack_p->end);
  tstack_p->nodenum = bspworld.dmodels[modelnum].headnode;
  tstack_p->planenum = 0;
  tstack_p->planedist = 0;
  tstack_p->next = TR_ENC(tracelist);
  tracelist = tstack_p;
  while ( 1 )
  {
    //take the first piece off the trace list
    tstack_p = tracelist;
    if ( !tstack_p )
      return trace;
    tracelist = TR_DEC(tstack_p->next);
    if ( tstack_p->planenum < 0 )
      return trace;
    nodenum = tstack_p->nodenum;
    //if the piece is in a leaf
    if ( nodenum < 0 )
    {
      leafnum = -1 - nodenum;
      leaf = &bspworld.dleafs[leafnum];
      if ( leaf->numleafbrushes && (leaf->contents & contentmask) )
        CM_TraceThroughLeaf(leafnum, origin, angles, start, boxmins, boxmaxs, end, contentmask, &trace);
      if ( bspworld.dword_10069584[leafnum] )
        sub_10003BF0(leafnum, start, boxmins, boxmaxs, end, passent, contentmask, &trace);
      continue;
    }
    node = &bspworld.dnodes[nodenum];
    VectorCopy(tstack_p->start, cur_start);
    VectorCopy(tstack_p->end, cur_end);
    planenum = tstack_p->planenum;
    planedist = tstack_p->planedist;
    //the piece goes back on the free list
    tstack_p->next = TR_ENC(freelist);
    freelist = tstack_p;
    plane = &bspworld.dplanes[node->planenum];
    if ( rotated )
    {
      VectorCopy(plane->normal, normal);
      RotatePoint(normal, axis);
      type = 4;
    }
    else
    {
      type = plane->type;
      VectorCopy(plane->normal, normal);
    }
    if ( translated )
    {
      if ( type < 3 ) dist = plane->dist + origin[type];
      else dist = plane->dist + DotProduct(normal, origin);
    }
    else
    {
      dist = plane->dist;
    }
    if ( boxmins && boxmaxs )
    {
      if ( type < 3 )
      {
        front = cur_start[type] - dist;
        back = cur_end[type] - dist;
        side = dir[type] < 0;
        offsets[0] = -boxmins[type];
        offsets[1] = -boxmaxs[type];
      }
      else
      {
        front = DotProduct(cur_start, normal) - dist;
        back = DotProduct(cur_end, normal) - dist;
        side = DotProduct(normal, dir) < 0;
        for ( i = 0; i < 3; i++ )
        {
          if ( normal[i] > 0 )
          {
            v1[i] = boxmins[i];
            v2[i] = boxmaxs[i];
          }
          else
          {
            v1[i] = boxmaxs[i];
            v2[i] = boxmins[i];
          }
        }
        invnormal[0] = -normal[0];
        invnormal[1] = -normal[1];
        invnormal[2] = -normal[2];
        offsets[0] = DotProduct(invnormal, v1);
        offsets[1] = DotProduct(invnormal, v2);
      }
      for ( i = 0; i < 2; i++ )
      {
        frontd[i] = front - offsets[i];
        backd[i] = back - offsets[i];
        sideflags[i][0] = frontd[i] > -0.005 && backd[i] > -0.005;
        sideflags[i][1] = frontd[i] < 0.005 && backd[i] < 0.005;
      }
      //the part on the side the trace line runs to
      if ( sideflags[0][side] || sideflags[1][side] )
      {
        if ( !freelist ) break;
        tstack_p = freelist;
        freelist = TR_DEC(tstack_p->next);
        VectorCopy(cur_start, tstack_p->start);
        VectorCopy(cur_end, tstack_p->end);
        tstack_p->planenum = planenum;
        tstack_p->planedist = planedist;
        tstack_p->nodenum = node->children[side];
        tstack_p->next = TR_ENC(tracelist);
        tracelist = tstack_p;
        if ( sideflags[0][side] && sideflags[1][side] ) continue;
      }
      //the part on the other side
      if ( sideflags[0][!side] || sideflags[1][!side] )
      {
        if ( !freelist ) break;
        tstack_p = freelist;
        freelist = TR_DEC(tstack_p->next);
        VectorCopy(cur_start, tstack_p->start);
        VectorCopy(cur_end, tstack_p->end);
        tstack_p->planenum = planenum;
        tstack_p->planedist = planedist;
        tstack_p->nodenum = node->children[!side];
        tstack_p->next = TR_ENC(tracelist);
        tracelist = tstack_p;
        if ( sideflags[0][!side] && sideflags[1][!side] ) continue;
      }
      if ( !sideflags[!side][0] && !sideflags[!side][1] )
      {
        frac1 = frontd[!side] / (frontd[!side] - backd[!side]);
        mid1[0] = cur_start[0] + (cur_end[0] - cur_start[0]) * frac1;
        mid1[1] = cur_start[1] + (cur_end[1] - cur_start[1]) * frac1;
        mid1[2] = cur_start[2] + (cur_end[2] - cur_start[2]) * frac1;
      }
      else
      {
        frac1 = -1;
      }
      if ( !sideflags[side][0] && !sideflags[side][1] )
      {
        frac2 = frontd[side] / (frontd[side] - backd[side]);
        mid2[0] = cur_start[0] + (cur_end[0] - cur_start[0]) * frac2;
        mid2[1] = cur_start[1] + (cur_end[1] - cur_start[1]) * frac2;
        mid2[2] = cur_start[2] + (cur_end[2] - cur_start[2]) * frac2;
      }
      else
      {
        frac2 = -1;
      }
      if ( frac1 < 0 && frac2 < 0 ) continue;
      //the piece beyond the split point, sorted into the trace list
      if ( !sideflags[0][side] && !sideflags[1][side] )
      {
        if ( frac1 >= 0 )
        {
          if ( !freelist ) break;
          tstack_p = freelist;
          freelist = TR_DEC(tstack_p->next);
          VectorCopy(mid1, tstack_p->start);
          VectorCopy(cur_end, tstack_p->end);
          tstack_p->planenum = node->planenum;
          tstack_p->planedist = offsets[!side];
          tstack_p->nodenum = node->children[side];
          prev = NULL;
          for ( ts = tracelist; ts; ts = TR_DEC(ts->next) )
          {
            if ( (tstack_p->start[sortaxis] < ts->start[sortaxis]) == positive )
            {
              tstack_p->next = TR_ENC(ts);
              if ( prev ) prev->next = TR_ENC(tstack_p);
              else tracelist = tstack_p;
              break;
            }
            prev = ts;
          }
          if ( !ts )
          {
            if ( prev ) prev->next = TR_ENC(tstack_p);
            else tracelist = tstack_p;
            tstack_p->next = 0;
          }
        }
        else if ( frac2 >= 0 )
        {
          if ( !freelist ) break;
          tstack_p = freelist;
          freelist = TR_DEC(tstack_p->next);
          VectorCopy(mid2, tstack_p->start);
          VectorCopy(cur_end, tstack_p->end);
          tstack_p->planenum = node->planenum;
          tstack_p->planedist = offsets[side];
          tstack_p->nodenum = node->children[side];
          prev = NULL;
          for ( ts = tracelist; ts; ts = TR_DEC(ts->next) )
          {
            if ( (tstack_p->start[sortaxis] < ts->start[sortaxis]) == positive )
            {
              tstack_p->next = TR_ENC(ts);
              if ( prev ) prev->next = TR_ENC(tstack_p);
              else tracelist = tstack_p;
              break;
            }
            prev = ts;
          }
          if ( !ts )
          {
            if ( prev ) prev->next = TR_ENC(tstack_p);
            else tracelist = tstack_p;
            tstack_p->next = 0;
          }
        }
      }
      //the piece in front of the split point
      if ( !sideflags[0][!side] && !sideflags[1][!side] )
      {
        if ( frac2 >= 0 )
        {
          if ( !freelist ) break;
          tstack_p = freelist;
          freelist = TR_DEC(tstack_p->next);
          VectorCopy(cur_start, tstack_p->start);
          VectorCopy(mid2, tstack_p->end);
          tstack_p->planenum = planenum;
          tstack_p->planedist = planedist;
          tstack_p->nodenum = node->children[!side];
          tstack_p->next = TR_ENC(tracelist);
          tracelist = tstack_p;
        }
        else if ( frac1 >= 0 )
        {
          if ( !freelist ) break;
          tstack_p = freelist;
          freelist = TR_DEC(tstack_p->next);
          VectorCopy(cur_start, tstack_p->start);
          VectorCopy(mid1, tstack_p->end);
          tstack_p->planenum = planenum;
          tstack_p->planedist = planedist;
          tstack_p->nodenum = node->children[!side];
          tstack_p->next = TR_ENC(tracelist);
          tracelist = tstack_p;
        }
      }
    }
    else
    {
      if ( type < 3 )
      {
        front = cur_start[type] - dist;
        back = cur_end[type] - dist;
      }
      else
      {
        front = DotProduct(cur_start, normal) - dist;
        back = DotProduct(cur_end, normal) - dist;
      }
      //the whole piece is in front of the plane
      if ( front > -0.005 && back > -0.005 )
      {
        if ( !freelist ) break;
        tstack_p = freelist;
        freelist = TR_DEC(tstack_p->next);
        VectorCopy(cur_start, tstack_p->start);
        VectorCopy(cur_end, tstack_p->end);
        tstack_p->planenum = planenum;
        tstack_p->planedist = 0;
        tstack_p->nodenum = node->children[0];
        tstack_p->next = TR_ENC(tracelist);
        tracelist = tstack_p;
      }
      //the whole piece is behind the plane
      else if ( front < 0.005 && back < 0.005 )
      {
        if ( !freelist ) break;
        tstack_p = freelist;
        freelist = TR_DEC(tstack_p->next);
        VectorCopy(cur_start, tstack_p->start);
        VectorCopy(cur_end, tstack_p->end);
        tstack_p->planenum = planenum;
        tstack_p->planedist = 0;
        tstack_p->nodenum = node->children[1];
        tstack_p->next = TR_ENC(tracelist);
        tracelist = tstack_p;
      }
      //the piece is split by the plane
      else
      {
        frac1 = front / (front - back);
        mid1[0] = cur_start[0] + (cur_end[0] - cur_start[0]) * frac1;
        mid1[1] = cur_start[1] + (cur_end[1] - cur_start[1]) * frac1;
        mid1[2] = cur_start[2] + (cur_end[2] - cur_start[2]) * frac1;
        side2 = front < 0;
        if ( !freelist ) break;
        tstack_p = freelist;
        freelist = TR_DEC(tstack_p->next);
        VectorCopy(mid1, tstack_p->start);
        VectorCopy(cur_end, tstack_p->end);
        tstack_p->planenum = node->planenum;
        tstack_p->planedist = 0;
        tstack_p->nodenum = node->children[!side2];
        tstack_p->next = TR_ENC(tracelist);
        tracelist = tstack_p;
        if ( !freelist ) break;
        tstack_p = freelist;
        freelist = TR_DEC(tstack_p->next);
        VectorCopy(cur_start, tstack_p->start);
        VectorCopy(mid1, tstack_p->end);
        tstack_p->planenum = planenum;
        tstack_p->planedist = 0;
        tstack_p->nodenum = node->children[side2];
        tstack_p->next = TR_ENC(tracelist);
        tracelist = tstack_p;
      }
    }
  }
  botimport.Print(PRT_ERROR, "AAS_TraceBSPModel: out of trace lines\n");
  return trace;
#undef TR_ENC
#undef TR_DEC
}

// gladiator.dll: 10005640..100056AC
// gladi386.so:   0000D0CC..0000D136
// Thin wrapper around AAS_TraceBSPModel supplying two zero vec3 locals as the
// entity-origin / angles slots, i.e. "AAS_Trace, but straight into world model
// 0 with no engine call".  DEAD in Gladiator: no caller in EITHER image.  The
// DLL keeps it through its /INCREMENTAL thunk; the .so exports it as F680,
// which is why it must NOT be `static` -- both compilers would strip a static
// function with no callers, and F680's 106 bytes are exactly this body.
//
// IDA rendered the hidden struct-return buffer as an explicit leading
// `void *out` parameter and the tail as `*out = AAS_TraceBSPModel(...)`.  That
// is the sret artifact, not the source: gcc will not forward a caller's buffer
// through a pointer PARAMETER (it cannot prove it does not alias the args), so
// the cast form costs an 84-byte temp plus a `rep movs` -- frame 0x60 against
// real's 0xc, OUR+4.  Returning bsp_trace_t BY VALUE, exactly like AAS_Trace,
// is what forwards it.  (2026-08-16; the F680/F663 mix-up is written up on
// AAS_Trace's own banner above.)
bsp_trace_t __cdecl sub_10005640(vec3_t start, vec3_t boxmins, vec3_t boxmaxs,
                                 vec3_t end, int passent, int contentmask)
{
  vec3_t zero_vec;

  zero_vec[0] = 0.0f;
  zero_vec[1] = 0.0f;
  zero_vec[2] = 0.0f;
  return AAS_TraceBSPModel(0, zero_vec, zero_vec,
                           start, boxmins, boxmaxs, end, passent, contentmask);
}

// gladiator.dll: 100056D0..10005767
// gladi386.so:   0000D138..0000D1F6
int __cdecl sub_100056D0(dbrush_t *a1, float *a2)
{
  int v3; // ebx
  /* v5: same BSP-plane-pointer-truncation bug class as sub_100044F0:v14/v38.
   * Original `int v5` truncated the selected dplane_t * on aarch64. */
  dplane_t *v5;
  int v6; // edx
  float v8; // st7

  for ( v3 = 0; v3 < a1->numsides; ++v3 )
  {
    v5 = &bspworld.dplanes[bspworld.dbrushsides[a1->firstside + v3].planenum];
    v6 = v5->type;
    if ( v6 < 3 )
    {
      /* axial plane shortcut: signbits of normal[v6] determine sign of a2[v6].
       * Original asm uses fcomps result (C0|C3 = "<= 0") to decide negation. */
      if ( v5->normal[v6] > 0.0f )
        v8 = a2[v6];
      else
        v8 = -a2[v6];
    }
    else
    {
      v8 = v5->normal[0] * *a2 + v5->normal[1] * a2[1] + v5->normal[2] * a2[2];
    }
    if ( v8 - v5->dist > 0.005 )
      return 0;
  }
  return 1;
}

// gladiator.dll: 100057A0..10005983
// gladi386.so:   0000D1F8..0000D5AC
int __cdecl sub_100057A0(float *a1, int a2, float *a3, float *a4)
{
  int v4; // edi
  int v6; // ebp
  /* v7: BSP leaf pointer for the containing leaf. */
  dleaf_t *v7;
  bsp_link_t *i; // ebp
  int v9; // [esp+10h] [ebp-7Ch]
  dbrush_t *v10; // [esp+14h] [ebp-78h]
  /* Declaration order of the four by-reference locals is recovered from the
   * reference .so: gcc 2.7 fills the address-taken frame group top-down in
   * declaration order, and gladi386.so's group is v13 / entdata / v11 / v12 --
   * exactly the reverse of IDA's emission order.  The DLL cannot show this
   * (MSVC6 /O2 assigns slots in first-reference order), so the ELF is the only
   * channel that records it; restoring it takes this row to a full MATCH on
   * gladi386.so at zero cost on the DLL. */
  float v13[3][3]; // [esp+30h] [ebp-5Ch] BYREF
  /* one bsp_entdata_t local */
  bsp_entdata_t entdata; // [esp+54h] [ebp-38h] BYREF
  float v11[3]; // [esp+18h] [ebp-74h] BYREF
  float v12[3]; // [esp+24h] [ebp-68h] BYREF

  if ( !bspworld.dword_100674C0 )
    return 0;
  v9 = 0;
  VectorSubtract(a1, a3, v11);
  v12[0] = -*a4;
  v12[1] = -a4[1];
  v12[2] = -a4[2];
  AnglesToAxis(v12, v13);
  RotatePoint(v11, v13);
  v6 = CM_PointLeafnum(v11, a2);
  v7 = &bspworld.dleafs[v6];
  for ( v4 = 0; v4 < v7->numleafbrushes; ++v4 )
  {
    int brushnum;

    brushnum = bspworld.dleafbrushes[v4 + v7->firstleafbrush];
    v10 = &bspworld.dbrushes[brushnum];
    if ( sub_100056D0(v10, v11) )
      return v10->contents;
  }
  for ( i = bspworld.dword_10069584[v6]; i; i = i->next_ent )
  {
    AAS_EntityBSPData(i->entnum, &entdata);
    if ( *a1 > entdata.absmins[0]
      && *a1 < entdata.absmaxs[0]
      && a1[1] > entdata.absmins[1]
      && a1[1] < entdata.absmaxs[1]
      && a1[2] > entdata.absmins[2]
      && a1[2] < entdata.absmaxs[2] )
    {
      if ( entdata.solid == 2 )
      {
        v9 |= 0x2000000u;
      }
      else if ( entdata.solid == 3 )
      {
        v9 |= sub_100057A0(a1, entdata.modelnum, entdata.origin, entdata.angles);
      }
    }
  }
  return v9;
}

// gladiator.dll: 10005A10..10005A45
// gladi386.so:   0000D5AC..0000D5EE
/* Point-only flavour of sub_100057A0: the caller's vec3 as origin with
 * zero mins/maxs, i.e. a degenerate point box.  DEAD in Gladiator. */
int __cdecl sub_10005A10(float *origin)
{
  vec3_t zero_vec;
  zero_vec[0] = 0.0f;
  zero_vec[1] = 0.0f;
  zero_vec[2] = 0.0f;
  return sub_100057A0(origin, 0, zero_vec, zero_vec);
}

// gladiator.dll: 10005A60..10005B00
// gladi386.so:   0000D5F0..0000D6E1
void __cdecl AAS_DecompressVis(int cluster, int visType)
{
  int c;
  int row;
  char *in;
  unsigned char *out;   /* byte, like Q1's decompressed[]: AAS_InPVS reads it zero-extended */

  if ( cluster == bspworld.dword_10069564 )
    return;
  in = bspworld.dvisdata + bspworld.dvis->bitofs[cluster][visType];
  row = (bspworld.dvis->numclusters + 7) >> 3;
  out = bspworld.byte_10067564;
  do
  {
    if ( *in )
    {
      *out++ = *in++;
      continue;
    }
    c = (unsigned __int8)in[1];
    if ( !c )
    {
      AAS_Error("AAS_DecompressVis: 0 repeat");
      return;
    }
    in += 2;
    while ( c )
    {
      *out++ = 0;
      --c;
    }
  }
  while ( out - bspworld.byte_10067564 < row );
  bspworld.dword_10069564 = cluster;
}

// gladiator.dll: 10005B30..10005C1C
// gladi386.so:   0000D6E4..0000D8EB
BOOL __cdecl AAS_InPVS(float *p1, float *p2, int type)
{
  int cluster;
  dleaf_t *leaf1, *leaf2;

  if ( !bspworld.dword_100674C0 )
    return 1;
  if ( !bspworld.visdatasize )
    return 1;
  if ( bspworld.flt_1006956C == p1[0] && bspworld.flt_10069570 == p1[1] && bspworld.flt_10069574 == p1[2] )
  {
    cluster = bspworld.dword_10069564;
  }
  else
  {
    leaf1 = sub_10003420(p1, 0);
    if ( leaf1->cluster == -1 )
      return 0;
    cluster = leaf1->cluster;
    bspworld.flt_1006956C = p1[0];
    bspworld.flt_10069570 = p1[1];
    bspworld.flt_10069574 = p1[2];
    bspworld.dword_10069568 = leaf1->area;
  }
  leaf2 = sub_10003420(p2, 0);
  if ( leaf2->cluster == -1 )
    return 0;
  AAS_DecompressVis(cluster, type);
  cluster = leaf2->cluster;
  /* Negative test, positive fall-through: gcc 2.7 then shares the top `return 1`
   * block as the .so does (cl.exe if-converts either spelling identically). */
  if ( !(bspworld.byte_10067564[cluster >> 3] & (1 << (cluster & 7))) )
    return 0;
  return 1;
}

// gladiator.dll: 10005C60..10005C75
// gladi386.so:   0000D8EC..0000D90F
qboolean __cdecl AAS_inPVS(vec3_t p1, vec3_t p2)
{
  return AAS_InPVS(p1, p2, 0);
}

// gladiator.dll: 10005C90..10005CA5
// gladi386.so:   0000D910..0000D933
BOOL __cdecl sub_10005C90(float *a1, float *a2)
{
  return AAS_InPVS(a1, a2, 1);
}

// gladiator.dll: 10005CC0..10005CD5
// gladi386.so:   0000D934..0000D95D
/* Double-indirect lookup into the cluster-routing pointer table.  DEAD in
 * Gladiator — the live accessors index aasworld.clusterareacache directly. */
int __cdecl sub_10005CC0(int a, int b)
{
  return ((int **)bspworld.dword_10067560)[a][b];
}

// gladiator.dll: 10005CF0..10005E10
// gladi386.so:   0000D960..0000DAF5
/* Reach-graph propagation over the cluster-routing matrix (dword_10067560, an
 * int[N] row-pointer table followed by N int[N] rows) and the 1-D flag row
 * dword_1006755C.  Three phases:
 *
 *   A: flag_row[row_index] = value;
 *   B: for each area i, clear matrix[i][*], set the diagonal, and for every
 *      areaportal of i whose flag is set, mark matrix[i][link] and
 *      matrix[link][i];
 *   C: the nested walk below.
 *
 * Phase C carries a faithful Mr. Elusive bug: its innermost loop is
 * `for (k = 0; k < numareas; j++)` -- k never advances, so it spins forever
 * walking j off the end of the matrix.  The byte-match needs the unbounded
 * shape.  Phase B's two inner loops share j (one register in the .so).
 *
 * Do NOT cache the globals in locals: the original re-reads numareas / dareas /
 * dareaportals / both matrix pointers at each use.
 *
 * DEAD in Gladiator. */
void __cdecl sub_10005CF0(int row_index, int value)
{
  int i, j, k, col;
  darea_t *area;
  dareaportal_t *portal;

  /* Phase A */
  ((int *)bspworld.dword_1006755C)[row_index] = value;

  /* Phase B */
  for (i = 0; i < bspworld.numareas; i++) {
    for (j = 0; j < bspworld.numareas; j++)
      ((int **)bspworld.dword_10067560)[i][j] = 0;
    ((int **)bspworld.dword_10067560)[i][i] = 1;

    area = &bspworld.dareas[i];
    for (j = 0; j < area->numareaportals; j++) {
      col = area->firstareaportal + j;
      if (((int *)bspworld.dword_1006755C)[col] != 0) {
        portal = &bspworld.dareaportals[col];
        ((int **)bspworld.dword_10067560)[i][portal->otherarea] = 1;
        ((int **)bspworld.dword_10067560)[portal->otherarea][i] = 1;
      }
    }
  }

  /* Phase C — the faithful Mr. Elusive bug (see banner); DEAD code,
   * never executed.  The innermost loop steps j, not k: gladi386.so keeps k
   * in its own register and indexes the matrix with it, which the frozen-k
   * `while (numareas > 0)` IDA shows cannot give. */
  for (i = 0; i < bspworld.numareas; i++) {
    for (j = 0; j < bspworld.numareas; j++) {
      for (k = 0; k < bspworld.numareas; j++) {
        if (((int **)bspworld.dword_10067560)[i][j] != 0 && ((int **)bspworld.dword_10067560)[j][k] != 0) {
          ((int **)bspworld.dword_10067560)[i][k] = 1;
          ((int **)bspworld.dword_10067560)[k][i] = 1;
        }
      }
    }
  }
}

// gladiator.dll: 10005E60..1000601A
// gladi386.so:   0000DAF8..0000DDA6
/* Rotated AABB of a Q2 BSP inline model.  Q3's cognate delegates the rotation to
 * a botimport callback; this does it locally.  Each Q2 dmodel_t entry is 48
 * bytes: mins[3], maxs[3], origin[3], headnode, firstface, numfaces. */
void __cdecl AAS_BSPModelMinsMaxsOrigin(int modelnum, vec3_t angles, vec3_t mins, vec3_t maxs, vec3_t origin)
{
  vec3_t axis[3];            /* rotation matrix from angles                 */
  vec3_t corner;
  vec3_t local_mins, local_maxs;
  vec3_t bb_mins, bb_maxs;   /* accumulator for the rotated bbox */
  int    i;

  if ( !bspworld.dword_100674C0 )
    return;
  if ( modelnum < 0 || modelnum >= bspworld.nummodels )
  {
    botimport.Print(PRT_FATAL, "AAS_BSPModelMinsMaxs: modelnum %d out of range [0-%d]", modelnum, bspworld.nummodels);
    if ( mins )   { VectorClear(mins); }
    if ( maxs )   { VectorClear(maxs); }
    if ( origin ) { VectorClear(origin); }
    return;
  }

  VectorCopy(bspworld.dmodels[modelnum].mins, local_mins);
  VectorCopy(bspworld.dmodels[modelnum].maxs, local_maxs);

  AnglesToAxis(angles, axis);   /* build 3x3 row-major rotation matrix */
  ClearBounds(bb_mins, bb_maxs);

  /* Rotate all 8 AABB corners through `axis`.  `corner` must stay an array:
   * RotatePoint reads it as a vec3_t. */
  for ( i = 0; i < 8; ++i )
  {
    /* Keep `(i < 4)` with mins in the THEN arm: the inverted
     * `(i >= 4) ? maxs : mins` flips the branch polarity. */
    corner[0] = (i < 4)          ? local_mins[0] : local_maxs[0];
    corner[1] = (i & 1)          ? local_mins[1] : local_maxs[1];
    corner[2] = (i < 2 || i > 6) ? local_mins[2] : local_maxs[2];
    RotatePoint(corner, (float *)axis);
    AddPointToBounds(corner, bb_mins, bb_maxs);
  }

  if ( mins ) { VectorCopy(bb_mins, mins); }
  if ( maxs ) { VectorCopy(bb_maxs, maxs); }
  if ( origin )
  {
    VectorCopy(bspworld.dmodels[modelnum].origin, origin);
  }
}

// gladiator.dll: 10006090..100060DA
// gladi386.so:   0000DDA8..0000DE3D
void __cdecl AAS_UnlinkFromBSPLeaves(bsp_link_t *leaves)
{
  bsp_link_t *result; // eax
  bsp_link_t *prev;   // ecx — prev_ent in leaf chain
  bsp_link_t *v3;     // esi — saved next_leaf
  bsp_link_t *next;   // ecx — next_ent in leaf chain

  result = leaves;
  if ( leaves )
  {
    do
    {
      v3 = result->next_leaf;
      prev = result->prev_ent;
      if ( prev )
        prev->next_ent = result->next_ent;
      else
        bspworld.dword_10069584[result->leafnum] = result->next_ent;
      next = result->next_ent;
      if ( next )
        next->prev_ent = result->prev_ent;
      sub_10003240(result);
      result = v3;
    }
    while ( v3 );
  }
}

// gladiator.dll: 10006100..100061C5
// gladi386.so:   0000DE40..0000DF48
/* Classify an AABB against a BSP splitting plane, returning 1/2/3 for
 * front/back/spanning.  The BSP-plane TWIN of Q3's AAS_BoxOnPlaneSide2: Q3 keeps
 * that name in be_aas_sample.c on the AAS-plane copy at 0x1001C2E0, whose only
 * caller is AAS_AASLinkEntity exactly as in Q3.  This one serves AAS_BSPLinkEntity,
 * which Q3 stubs, so its own name is not recoverable -- it stays sub_ like the
 * BSP-leaf link-heap twins (see naming.md). */
int __cdecl sub_10006100(vec3_t absmins, vec3_t absmaxs, float *p)
{
  int    i, sides;
  vec3_t corners[2];
  float  dist1, dist2;

  for ( i = 0; i < 3; ++i )
  {
    if ( p[i] < 0.0f )
    {
      corners[0][i] = absmins[i];
      corners[1][i] = absmaxs[i];
    }
    else
    {
      corners[1][i] = absmins[i];
      corners[0][i] = absmaxs[i];
    }
  }
  dist1 = DotProduct(p, corners[0]) - p[3];
  dist2 = DotProduct(p, corners[1]) - p[3];
  sides = 0;
  if ( dist1 >= 0.0f )
    sides = 1;
  if ( dist2 < 0.0f )
    sides |= 2;
  return sides;
}

// gladiator.dll: 10006210..1000636D
// gladi386.so:   0000DF48..0000E26F
bsp_link_t *__cdecl AAS_BSPLinkEntity(vec3_t absmins, vec3_t absmaxs, int entnum, int modelnum)
{
  bsp_link_t *link; // edi
  int *v6; // ebx
  int nodenum; // eax
  int leafnum; // esi
  bsp_link_t *newlink; // eax
  bsp_link_t *v10; // ecx
  dnode_t *v11; // esi
  dplane_t *plane; // ecx
  int v13; // edx
  int v14; // eax
  int v15[64]; // [esp+10h] [ebp-100h] BYREF — stack-based BSP traversal queue

  if ( !bspworld.dword_100674C0 )
    return 0;
  link = 0;
  v15[0] = bspworld.dmodels[modelnum].headnode;
  v6 = &v15[1];
  /* while(1)+break, NOT `while (--v6 >= &v15[0])`: v6 starts at &v15[1], so MSVC6
   * can prove that guard holds first time and rotates to a bottom test.  The
   * infinite-loop form keeps the test at the top — do not "simplify". */
  while ( 1 )
  {
    if ( --v6 < &v15[0] )
      break;
    nodenum = *v6;
    if ( nodenum < 0 )
    {
      leafnum = -1 - nodenum;
      newlink = sub_100031F0();
      if ( !newlink )
        return link;
      newlink->entnum    = entnum;
      newlink->leafnum   = leafnum;
      newlink->prev_leaf = 0;
      newlink->next_leaf = link;
      if ( link )
        link->prev_leaf = newlink;
      link = newlink;
      newlink->prev_ent  = 0;
      newlink->next_ent  = 0;
      newlink->next_ent  = bspworld.dword_10069584[leafnum];
      v10 = bspworld.dword_10069584[leafnum];
      if ( v10 )
        v10->prev_ent = newlink;
      bspworld.dword_10069584[leafnum] = newlink;
    }
    else
    {
      v11 = &bspworld.dnodes[nodenum];
      plane = &bspworld.dplanes[v11->planenum];
      v13 = plane->type;
      if ( v13 < 3 )
      {
        if ( plane->dist <= (float)absmins[v13] )
          v14 = 1;
        else if ( plane->dist >= (float)absmaxs[v13] )
          v14 = 2;
        else
          v14 = 3;
      }
      else
      {
        v14 = sub_10006100(absmins, absmaxs, (float *)plane);
      }
      if ( (v14 & 1) != 0 )
        *v6++ = v11->children[0];
      if ( (v14 & 2) != 0 )
        *v6++ = v11->children[1];
    }
  }
  return link;
}

// gladiator.dll: 100063D0..10006588
// gladi386.so:   0000E270..0000E563
/* AAS_EntitiesInBox(mins, maxs, list, maxcount): write up to `maxcount` entnums
 * of the world entities overlapping [mins,maxs] into list[] and return the count.
 *
 * AAS_BSPLinkEntity builds a transient bsp_link_t chain, one node per leaf the
 * box touches.  Each such leaf's entity-link chain is walked, duplicates skipped
 * by a linear scan, and each candidate's bbox overlap-tested.  SOLID_BBOX and
 * SOLID_TRIGGER are added directly; a SOLID_BSP movable brush is re-linked by
 * its own modelnum and added only if one of ITS leaves carries an AAS area
 * marker.  Every chain is released with AAS_UnlinkFromBSPLeaves, NULL included.
 *
 * DEAD in Gladiator. */
int __cdecl sub_100063D0(vec3_t mins, vec3_t maxs, int *list, int maxcount)
{
  bsp_link_t *linkhead;
  bsp_link_t *link;
  bsp_link_t *ent_link;
  bsp_link_t *brush_links;
  bsp_link_t *brush_iter;
  int         count;
  int         j;
  int         solid;
  bsp_entdata_t entdata;      /* AAS_EntityBSPData destination — 56 B */

  count = 0;
  linkhead = AAS_BSPLinkEntity(mins, maxs, 0, 0);
  /* NO early `if (!linkhead) return 0;` — the original's test is this loop's entry
   * guard and lands on the shared exit, so the NULL path still calls
   * AAS_UnlinkFromBSPLeaves(NULL), which tolerates it. */
  for (link = linkhead; link && count < maxcount; link = link->next_leaf) {
    ent_link = bspworld.dword_10069584[link->leafnum];
    if (!ent_link)
      continue;

    while (ent_link && count < maxcount) {
      /* Dedupe scan over the results so far.  Re-read `ent_link->entnum` at each of
       * the four use sites — the original never caches it in a local. */
      for (j = 0; j < count; j++) {
        if (list[j] == ent_link->entnum)
          break;
      }

      if (j == count) {
        AAS_EntityBSPData(ent_link->entnum, &entdata);

        /* All six bounds are the correct ones.  The two textually identical
         * `fld [esp+0x3c]` in the original read DIFFERENT fields: the first runs
         * while esp is still 8 lower, so it is absmins[0], the second absmins[2]. */
        if (entdata.absmins[0] <= maxs[0]
         && entdata.absmaxs[0] >= mins[0]
         && entdata.absmins[1] <= maxs[1]
         && entdata.absmaxs[1] >= mins[1]
         && entdata.absmins[2] <= maxs[2]
         && entdata.absmaxs[2] >= mins[2]) {
          solid = entdata.solid;
          if (solid == 1 || solid == 2) {
            list[count++] = ent_link->entnum;
          } else if (solid == 3) {
            /* 4th arg is entdata.modelnum, not entnum. */
            brush_links = AAS_BSPLinkEntity(mins, maxs, 0, entdata.modelnum);
            /* No `if (brush_links)` guard either — the NULL case unlinks NULL.  The
             * hit is reported by BREAKing to the shared post-loop re-check. */
            for (brush_iter = brush_links; brush_iter; brush_iter = brush_iter->next_leaf) {
              /* Through a `dleaf_t *`, not a subscript: the original folds the
               * scaled index and the lump base into ONE address (`add eax,[ecx+0x2c]`)
               * where the subscript form keeps two registers and a SIB. */
              dleaf_t *leaf = &bspworld.dleafs[brush_iter->leafnum];
              if (leaf->numleafbrushes)
                break;
            }
            if (brush_iter) {
              list[count++] = ent_link->entnum;
            }
            AAS_UnlinkFromBSPLeaves(brush_links);
          }
        }
      }
      ent_link = ent_link->next_ent;
    }
  }

  AAS_UnlinkFromBSPLeaves(linkhead);
  return count;
}

// gladiator.dll: 10006600..10006702
// gladi386.so:   0000E564..0000E653
// Set/update a BSP epair in an entity's epair list — the writing counterpart of
// AAS_ValueForBSPEpairKey.  On a key-hit frees the old value and replaces it; on
// a miss prepends a freshly-cleared 12-byte epair.  The original inlines the
// strcmp/strlen/memcpy as repe scas / rep movs; written here as the equivalent
// strdup-style idiom MSVC inlined from <string.h>.
// Allocator thunks: 0x10001479 -> GetClearedMemory, 0x10001AB4 -> GetMemory,
// 0x1000180C -> FreeMemory.
// DEAD in Gladiator; preserved by /INCREMENTAL.
void __cdecl sub_10006600(bsp_epair_t **head, char *key, char *value)
{
  bsp_epair_t *ep;
  int          klen;
  int          vlen1;
  int          vlen2;

  for ( ep = *head; ep; ep = ep->next )
  {
    if ( !strcmp(ep->key, key) )
    {
      FreeMemory(ep->value);
      vlen1 = strlen(value) + 1;
      ep->value = (char *)GetMemory(vlen1);
      strcpy(ep->value, value);
      return;
    }
  }
  ep = (bsp_epair_t *)GetClearedMemory(12);
  ep->next = *head;
  *head    = ep;
  klen = strlen(key) + 1;
  ep->key = (char *)GetMemory(klen);
  strcpy(ep->key, key);
  vlen2 = strlen(value) + 1;
  ep->value = (char *)GetMemory(vlen2);
  strcpy(ep->value, value);
}

// gladiator.dll: 10006760..100067BD
// gladi386.so:   0000E654..0000E699
char *__cdecl AAS_ValueForBSPEpairKey(bsp_entity_t *ent, const char *key)
{
  bsp_epair_t *ep;

  for ( ep = ent->epairs; ep; ep = ep->next )
  {
    if ( !strcmp(ep->key, key) )
      return ep->value;
  }
  return 0;
}

// gladiator.dll: 100067E0..1000686E
// gladi386.so:   0000E69C..0000E76A
int __cdecl AAS_VectorForBSPEpairKey(bsp_entity_t *ent, const char *key, vec3_t v)
{
  char *value; // eax
  double v1; // [esp+0h] [ebp-18h] BYREF
  double v2; // [esp+8h] [ebp-10h] BYREF
  double v3; // [esp+10h] [ebp-8h] BYREF

  value = AAS_ValueForBSPEpairKey(ent, key);
  if ( !value )
    return (int)(intptr_t)value;
  v3 = 0.0;
  v2 = 0.0;
  v1 = 0.0;
  sscanf(value, "%lf %lf %lf", &v1, &v2, &v3);
  *v = v1;
  v[1] = v2;
  v[2] = v3;
  return 1;
}

// gladiator.dll: 100068A0..100068C5
// gladi386.so:   0000E76C..0000E7D9
/* Third of Q3's AAS_{Value,Vector,Float,Int}ForBSPEpairKey family, in Q3's order.
 * Like its siblings it still returns the value directly; Q3 later moved every
 * member to an out-parameter plus a success flag. */
float __cdecl AAS_FloatForBSPEpairKey(bsp_entity_t *ent, const char *key)
{
  const char *value; // eax

  value = (const char *)AAS_ValueForBSPEpairKey(ent, key);
  if ( !value )
    return 0.0f;
  return atof(value);
}

// gladiator.dll: 100068E0..10006901
// gladi386.so:   0000E7DC..0000E83A
int __cdecl AAS_IntForBSPEpairKey(bsp_entity_t *ent, const char *key)
{
  const char *value; // eax

  value = (const char *)AAS_ValueForBSPEpairKey(ent, key);
  if ( !value )
    return (int)value;
  return atoi(value);
}

// gladiator.dll: 10006920..1000697A
// gladi386.so:   0000E83C..0000E8B2
void __cdecl AAS_FreeBSPEntities(bsp_entity_t *a1)
{
  bsp_entity_t *v1; // ebx
  bsp_epair_t  *epair; // esi
  bsp_entity_t *v3; // ebp
  bsp_epair_t  *nextepair; // edi

  for ( v1 = a1; v1; v1 = v3 )
  {
    v3 = v1->next;
    for ( epair = v1->epairs; epair; epair = nextepair )
    {
      nextepair = epair->next;
      if ( epair->key )
        FreeMemory(epair->key);
      if ( epair->value )
        FreeMemory(epair->value);
      FreeMemory(epair);
    }
    FreeMemory(v1);
  }
}

// gladiator.dll: 100069A0..10006C59
// gladi386.so:   0000E8B4..0000EC80
bsp_entity_t *AAS_ParseBSPEntities(void)
{
  script_t *script; // ebp
  token_t token;
  bsp_entity_t *ent; // [esp+10h]
  bsp_epair_t *epair; // ebx
  bsp_entity_t *entities; // edi

  script = LoadScriptMemory(bspworld.dentdata, bspworld.entdatasize, "entdata");
  SetScriptFlags(script, 12);
  entities = 0;
  while ( PS_ReadToken(script, &token) )
  {
    if ( strcmp(token.string, "{") )
    {
      ScriptError(script, "invalid %s\n", token.string);
      AAS_FreeBSPEntities(entities);
      FreeScript(script);
      return 0;
    }
    ent = (bsp_entity_t *)GetClearedMemory(sizeof(bsp_entity_t));
    ent->next = entities;
    entities = ent;
    while ( PS_ReadToken(script, &token) )
    {
      if ( !strcmp(token.string, "}") )
        break;
      epair = (bsp_epair_t *)GetClearedMemory(sizeof(bsp_epair_t));
      epair->next = ent->epairs;
      ent->epairs = epair;
      if ( token.type != 1 )
      {
        ScriptError(script, "invalid %s\n", token.string);
        AAS_FreeBSPEntities(entities);
        FreeScript(script);
        return 0;
      }
      StripDoubleQuotes(token.string);
      epair->key = (char *)GetMemory(strlen(token.string) + 1);
      strcpy(epair->key, token.string);
      if ( !PS_ExpectTokenType(script, 1, 0, &token) )
      {
        AAS_FreeBSPEntities(entities);
        FreeScript(script);
        return 0;
      }
      StripDoubleQuotes(token.string);
      epair->value = (char *)GetMemory(strlen(token.string) + 1);
      strcpy(epair->value, token.string);
    }
    if ( strcmp(token.string, "}") )
    {
      ScriptError(script, "missing }\n");
      AAS_FreeBSPEntities(entities);
      FreeScript(script);
      return 0;
    }
  }
  FreeScript(script);
  return entities;
}

// 10001C30: thunk -> 0x1003F5C0 = PS_ExpectTokenType (script-level expect)
// gladiator.dll: 10006D10..1000706B
// gladi386.so:   0000EC80..0000F0A1
/* Quake 1's `WinQuake/gl_rlight.c` RecursiveLightPoint (the GL variant, which
 * also exports the impact point), with Q1 `world.c`'s SV_RecursiveHullCheck head
 * grafted on, plus an axial fast path Q1 never had.  Algorithm is Q1, data
 * layout is Q2.
 *
 * Q1's text otherwise.  Deliberate deviations from Q1 -- do NOT "restore" them:
 *   - returns 0/1 rather than -1/0..255, and drops Q1's redundant second
 *     `if ((back<0)==side) return -1;`
 *   - drops Q1's `surf->flags & SURF_DRAWTILED` skip
 *   - `side == (back < 0)`, operands the other way round from Q1 (cl.exe keeps a
 *     comparison's textual order)
 *   - Q1's single-channel `r += *lightmap * scale` becomes a 3-channel RGB read,
 *     with Q1's `scale` variable kept but set to the fixed style value 264, and
 *     the sample offset is ds * width + dt, the transpose of Q1's dt * width + ds.
 * Reads the per-face {texturemins[2], extents[2]} table CalcSurfaceExtents
 * builds.
 *
 * Three source forms both originals prove, each one measured:
 *   - `scale = 264;` inside the maps loop, not the literal.  gcc 2.7 synthesises
 *     a multiply by a constant into shifts and adds only when the constant is
 *     visible at RTL expansion; gladi386.so has `imul eax,eax,0x108`, which is
 *     what a variable CSE later finds constant gives.
 *   - the lightmap offset as ONE expression with the RGB factor on each term and
 *     no `ds >>= 4; dt >>= 4;` statements.  The .so evaluates it in exactly this
 *     textual order, and it also fixes the ELF register allocation of the face
 *     loop; the stride likewise is `(w * h) * 3`, not `3 * w * h`.
 *   - `surf->lightofs` read at both uses, with no local.  cl.exe CSEs the two
 *     reads into one register-held value and orders the adds as the DLL does;
 *     through a local it folded the test into a memory compare and re-read the
 *     field.
 * The declaration order is the .so's spill-slot order (side, i, b, surf,
 * extents, node from the top of its frame down): gcc 2.7 assigns spilled
 * pseudos their slots in creation order, which is declaration order. */
int __cdecl RecursiveLightPoint(int nodenum, float *start, float *end, float *lightspot, int *pointcolor)
{
  float front, back, frac;
  int side;
  dplane_t *plane;
  vec3_t mid;
  int s, t, ds, dt;
  int i;
  texinfo_t *tex;
  byte *lightmap;
  int maps, r, g, b;
  unsigned scale;
  dface_t *surf;
  short *extents;
  dnode_t *node;

  if (nodenum < 0)
    return 0;    // didn't hit anything

  node = &bspworld.dnodes[nodenum];
  plane = &bspworld.dplanes[node->planenum];
  if (plane->type < 3)
  {
    front = start[plane->type] - plane->dist;
    back = end[plane->type] - plane->dist;
  }
  else
  {
    front = DotProduct(start, plane->normal) - plane->dist;
    back = DotProduct(end, plane->normal) - plane->dist;
  }
  side = front < 0;

  if (side == (back < 0))
    return RecursiveLightPoint(node->children[side], start, end, lightspot, pointcolor);

  frac = front / (front-back);
  mid[0] = start[0] + (end[0] - start[0])*frac;
  mid[1] = start[1] + (end[1] - start[1])*frac;
  mid[2] = start[2] + (end[2] - start[2])*frac;

  // go down front side
  if (RecursiveLightPoint(node->children[side], start, mid, lightspot, pointcolor))
    return 1;    // hit something

  // check for impact on this node
  surf = &bspworld.dfaces[node->firstface];
  extents = (short *)(bspworld.dword_10067558 + 8 * node->firstface);
  for (i = 0; i < node->numfaces; i++, surf++, extents += 4)
  {
    tex = &bspworld.texinfo[surf->texinfo];

    s = DotProduct(mid, tex->vecs[0]) + tex->vecs[0][3];
    t = DotProduct(mid, tex->vecs[1]) + tex->vecs[1][3];

    if (s < extents[0] || t < extents[1])
      continue;

    ds = s - extents[0];
    dt = t - extents[1];

    if (ds > extents[2] || dt > extents[3])
      continue;

    if (surf->lightofs < 0)
    {
      pointcolor[0] = 0;
      pointcolor[1] = 0;
      pointcolor[2] = 0;
      VectorCopy(mid, lightspot);
      return 1;
    }

    lightmap = (ds>>4) * ((extents[2]>>4)+1) * 3 + surf->lightofs + (dt>>4) * 3 + bspworld.dlightdata;
    r = 0;
    g = 0;
    b = 0;

    for (maps = 0; maps < 4 && surf->styles[maps] != 255; maps++)
    {
      scale = 264;
      r += lightmap[0] * scale;
      g += lightmap[1] * scale;
      b += lightmap[2] * scale;
      lightmap += ((extents[2]>>4)+1) * ((extents[3]>>4)+1) * 3;
    }
    pointcolor[0] = r >> 8;
    pointcolor[1] = g >> 8;
    pointcolor[2] = b >> 8;
    VectorCopy(mid, lightspot);
    return 1;
  }

  // go down back side
  return RecursiveLightPoint(node->children[!side], mid, end, lightspot, pointcolor);
}

// gladiator.dll: 10007150..100071BC
// gladi386.so:   0000F0A4..0000F125
/* Static-light helper for AAS_BSPTraceLight: traces model 0 via
 * RecursiveLightPoint, returning endpos plus the surface RGB.  Q3 stubs the whole
 * BSPTraceLight feature, so there is no cognate name. */
int __cdecl sub_10007150(intptr_t start, intptr_t end, intptr_t endpos, _DWORD *red, _DWORD *green, _DWORD *blue)
{
  /* int[3], not float[3]: RecursiveLightPoint writes the RGB samples as ints and
   * the original copies them out with plain movs.  Typed float, the copy becomes a
   * float->int conversion that truncates the denormal-looking samples to 0. */
  int v7[3]; // [esp+0h] [ebp-Ch] BYREF

  if ( !bspworld.dword_100674C0 )
    return 0;
  if ( !bspworld.dlightdata || !RecursiveLightPoint(bspworld.dmodels[0].headnode, (float *)start, (float *)end, (float *)endpos, v7) )
    return 0;
  *red = v7[0];
  *green = v7[1];
  *blue = v7[2];
  return 1;
}

// gladiator.dll: 100071E0..100073D3
// gladi386.so:   0000F128..0000F4CF
/* Quake 1 `WinQuake/model.c` CalcSurfaceExtents, walking the BSP file lumps
 * instead of a loaded model_t and writing an 8-byte {short texturemins[2]; short
 * extents[2]} record per face into the dword_10067558 side table.  Consumed by
 * RecursiveLightPoint.
 *
 * Deliberate deviations from Q1 — do NOT restore:
 *   - mins init is 99999.0f, not id's 999999 (maxs is -99999.0f as in Q1)
 *   - Q1's TEX_SPECIAL bad-extents Sys_Error check is absent
 *   - the `val` dot product is written vecs-first and out of index order.  This
 *     one is unobservable: MSVC6 /O2 canonicalises a 3-term FP sum to (2,1,0)
 *     whatever the source order. */
void CalcSurfaceExtents()
{
  float mins[2], maxs[2], val;
  int i, j, e, n;
  dvertex_t *v;
  texinfo_t *tex;
  int bmins[2], bmaxs[2];
  dface_t *face;

  if (bspworld.dword_10067558) FreeMemory(bspworld.dword_10067558);
  bspworld.dword_10067558 = GetClearedMemory(8 * bspworld.numfaces);
  for (n = 0; n < bspworld.numfaces; n++)
  {
    face = &bspworld.dfaces[n];
    mins[0] = mins[1] = 99999;
    maxs[0] = maxs[1] = -99999;

    tex = &bspworld.texinfo[face->texinfo];

    for (i = 0; i < face->numedges; i++)
    {
      e = bspworld.dsurfedges[face->firstedge + i];
      if (e >= 0)
        v = &bspworld.dvertexes[bspworld.dedges[e].v[0]];
      else
        v = &bspworld.dvertexes[bspworld.dedges[-e].v[1]];

      for (j = 0; j < 2; j++)
      {
        val = v->point[0] * tex->vecs[j][0] +
          v->point[1] * tex->vecs[j][1] +
          v->point[2] * tex->vecs[j][2] +
          tex->vecs[j][3];
        if (val < mins[j])
          mins[j] = val;
        if (val > maxs[j])
          maxs[j] = val;
      }
    }

    for (i = 0; i < 2; i++)
    {
      bmins[i] = floor(mins[i]/16);
      bmaxs[i] = ceil(maxs[i]/16);

      ((short *)(bspworld.dword_10067558 + 8 * n))[i] = bmins[i] * 16;
      ((short *)(bspworld.dword_10067558 + 8 * n))[2 + i] = (bmaxs[i] - bmins[i]) * 16;
    }
  }
}

// gladiator.dll: 10007460..1000786F
// gladi386.so:   0000F4D0..0000FC69
/* No parameters: the `a1` the decompiler shows is a phantom __fastcall arg — the
 * prologue's `push ecx` only reserves a local slot, and the sole caller sets up
 * no ecx at all.  Named from its cognate Q2_SwapBSPFile in bspc/l_bsp_q2.c,
 * minus bspc's `todisk` reverse direction.
 *
 * void, like bspc's: gladi386.so's epilogue is a bare `pop/pop/pop/pop; add esp,8;
 * ret` with nothing written to eax, and IDA's `result = bspworld.nummodels` inside
 * the models loop is what lets gcc hoist `nummodels` out of the loop guard -- the
 * original re-reads it from memory on every iteration, as aliasing requires. */
void Q2_SwapBSPFile(void)
{
  /* ONE counter for all four lump loops.  IDA lists i/j/k/m at the same
   * [esp+10h] because MSVC6 coalesced them; gcc 2.7 does NOT coalesce, so every
   * extra name is another frame slot -- the original gets by on TWO (frame 0x8:
   * this counter plus the vis cluster count).  Declared FIRST because gcc 2.7
   * lays the frame out in REVERSE declaration order and the original keeps the
   * counter in the HIGHER of its two slots. */
  int i;
  int v2; // ebx
  texinfo_t *v3; // esi
  float *v4; // edi
  int v11; // edi
  int v16; // ebx
  dplane_t *v17; // esi
  float *v18; // edi
  dnode_t *v23; // esi
  int v30; // ebx
  dleaf_t *v38; // esi
  int v45; // ebx
  dmodel_t *v70; // esi
  int v76; // edi

  for ( i = 0; i < bspworld.numtexinfo; ++i )
  {
    v2 = 8;
    v3 = &bspworld.texinfo[i];
    v4 = &v3->vecs[0][0];
    do
    {
      *v4 = LittleFloat(*v4);
      ++v4;
      --v2;
    }
    while ( v2 );
    v3->flags = LittleLong(v3->flags);
    v3->value = LittleLong(v3->value);
    v3->nexttexinfo = LittleLong(v3->nexttexinfo);
  }
  v11 = 0;
  if ( bspworld.dvis )
  {
    bspworld.dvis->numclusters = LittleLong(bspworld.dvis->numclusters);
    v11 = bspworld.dvis->numclusters;
  }
  /* bspc's plain `for (i = 0; i < j; i++)` (l_bsp_q2.c), NOT IDA's `while (1)` with
   * a mid-loop break and a trailing reload of `dvis`: the write-only `char *v9` was
   * IDA's name for that reload, and the break shape stops gcc's -funroll-loops from
   * unrolling this loop 4x with runtime preconditioning the way the original does. */
  for ( i = 0; i < v11; i++ )
  {
    bspworld.dvis->bitofs[i][0] = LittleLong(bspworld.dvis->bitofs[i][0]);
    bspworld.dvis->bitofs[i][1] = LittleLong(bspworld.dvis->bitofs[i][1]);
  }
  for ( i = 0; i < bspworld.numplanes; ++i )
  {
    v16 = 3;
    v17 = &bspworld.dplanes[i];
    v18 = v17->normal;
    do
    {
      *v18 = LittleFloat(*v18);
      ++v18;
      --v16;
    }
    while ( v16 );
    v17->dist = LittleFloat(v17->dist);
    v17->type = LittleLong(v17->type);
  }
  for ( i = 0; i < bspworld.numnodes; ++i )
  {
    v23 = &bspworld.dnodes[i];
    v23->planenum = LittleLong(v23->planenum);
    v23->children[0] = LittleLong(v23->children[0]);
    v23->children[1] = LittleLong(v23->children[1]);
    for ( v30 = 0; v30 < 3; ++v30 )
    {
      v23->mins[v30] = LittleShort(v23->mins[v30]);
      v23->maxs[v30] = LittleShort(v23->maxs[v30]);
    }
    v23->firstface = LittleShort(v23->firstface);
    v23->numfaces = LittleShort(v23->numfaces);
  }
  for ( i = 0; i < bspworld.numleafs; ++i )
  {
    v38 = &bspworld.dleafs[i];
    v38->contents = LittleLong(v38->contents);
    v38->cluster = LittleShort(v38->cluster);
    v38->area = LittleShort(v38->area);
    for ( v45 = 0; v45 < 3; ++v45 )
    {
      v38->mins[v45] = LittleShort(v38->mins[v45]);
      v38->maxs[v45] = LittleShort(v38->maxs[v45]);
    }
    v38->firstleafface = LittleShort(v38->firstleafface);
    v38->numleaffaces = LittleShort(v38->numleaffaces);
    v38->firstleafbrush = LittleShort(v38->firstleafbrush);
    v38->numleafbrushes = LittleShort(v38->numleafbrushes);
  }
  for ( i = 0; i < bspworld.numleafbrushes; ++i )
  {
    bspworld.dleafbrushes[i] = LittleShort(bspworld.dleafbrushes[i]);
  }
  for ( i = 0; i < bspworld.numbrushsides; ++i )
  {
    /* The LittleShort round-trip must stay (identity on little-endian): without it
     * an uninitialised temp lands on every brush's planenum at map-load time and
     * intermittently drives CM_TraceThroughBrush into an out-of-range plane. */
    bspworld.dbrushsides[i].planenum = LittleShort(bspworld.dbrushsides[i].planenum);
    bspworld.dbrushsides[i].texinfo  = LittleShort(bspworld.dbrushsides[i].texinfo);
  }
  for ( i = 0; i < bspworld.numbrushes; ++i )
  {
    bspworld.dbrushes[i].firstside = LittleLong(bspworld.dbrushes[i].firstside);
    bspworld.dbrushes[i].numsides = LittleLong(bspworld.dbrushes[i].numsides);
    bspworld.dbrushes[i].contents = LittleLong(bspworld.dbrushes[i].contents);
  }
  /* bspc's counted `for` over a typed `&dmodels[i]` (l_bsp_q2.c).  gladi386.so
   * says so directly: its entry test compares nummodels against the counter's
   * slot, which only gcc's own rotation of a counted `for` produces (IDA's
   * guard-plus-do/while compared against 0).  cl.exe emits the same code for
   * both. */
  for ( i = 0; i < bspworld.nummodels; i++ )
  {
    v70 = &bspworld.dmodels[i];
    v70->firstface = LittleLong(v70->firstface);
    v70->numfaces = LittleLong(v70->numfaces);
    v70->headnode = LittleLong(v70->headnode);
    for ( v76 = 0; v76 < 3; v76++ )
    {
      v70->mins[v76] = LittleFloat(v70->mins[v76]);
      v70->maxs[v76] = LittleFloat(v70->maxs[v76]);
      v70->origin[v76] = LittleFloat(v70->origin[v76]);
    }
  }
}

// 1000775A: Q2_SwapBSPFile brushsides loop — the LittleShort round-trip.
//            See note inside the function.
// gladiator.dll: 10007980..10007BAD
// gladi386.so:   0000FC6C..0001000F
void AAS_DumpBSPData()
{
  bspworld.nummodels = 0;
  if ( bspworld.dmodels )
    FreeMemory(bspworld.dmodels);
  bspworld.dmodels = 0;
  bspworld.visdatasize = 0;
  if ( bspworld.dvisdata )
    FreeMemory(bspworld.dvisdata);
  bspworld.dvisdata = 0;
  bspworld.dvis = 0;
  bspworld.lightdatasize = 0;
  if ( bspworld.dlightdata )
    FreeMemory(bspworld.dlightdata);
  bspworld.dlightdata = 0;
  bspworld.entdatasize = 0;
  if ( bspworld.dentdata )
    FreeMemory(bspworld.dentdata);
  bspworld.dentdata = 0;
  bspworld.numleafs = 0;
  if ( bspworld.dleafs )
    FreeMemory(bspworld.dleafs);
  bspworld.dleafs = 0;
  bspworld.numplanes = 0;
  if ( bspworld.dplanes )
    FreeMemory(bspworld.dplanes);
  bspworld.dplanes = 0;
  bspworld.numvertexes = 0;
  if ( bspworld.dvertexes )
    FreeMemory(bspworld.dvertexes);
  bspworld.dvertexes = 0;
  bspworld.numnodes = 0;
  if ( bspworld.dnodes )
    FreeMemory(bspworld.dnodes);
  bspworld.dnodes = 0;
  bspworld.numtexinfo = 0;
  if ( bspworld.texinfo )
    FreeMemory(bspworld.texinfo);
  bspworld.texinfo = 0;
  bspworld.numfaces = 0;
  if ( bspworld.dfaces )
    FreeMemory(bspworld.dfaces);
  bspworld.dfaces = 0;
  bspworld.numedges = 0;
  if ( bspworld.dedges )
    FreeMemory(bspworld.dedges);
  bspworld.dedges = 0;
  bspworld.numleaffaces = 0;
  if ( bspworld.dleaffaces )
    FreeMemory(bspworld.dleaffaces);
  bspworld.dleaffaces = 0;
  bspworld.numleafbrushes = 0;
  if ( bspworld.dleafbrushes )
    FreeMemory(bspworld.dleafbrushes);
  bspworld.dleafbrushes = 0;
  bspworld.numsurfedges = 0;
  if ( bspworld.dsurfedges )
    FreeMemory(bspworld.dsurfedges);
  bspworld.dsurfedges = 0;
  bspworld.numbrushes = 0;
  if ( bspworld.dbrushes )
    FreeMemory(bspworld.dbrushes);
  bspworld.dbrushes = 0;
  bspworld.numbrushsides = 0;
  if ( bspworld.dbrushsides )
    FreeMemory(bspworld.dbrushsides);
  bspworld.dbrushsides = 0;
  bspworld.numareas = 0;
  if ( bspworld.dareas )
    FreeMemory(bspworld.dareas);
  bspworld.dareas = 0;
  bspworld.numareaportals = 0;
  if ( bspworld.dareaportals )
    FreeMemory(bspworld.dareaportals);
  bspworld.dareaportals = 0;
  bspworld.dword_100674C0 = 0;
}

// gladiator.dll: 10007C40..10007CFD
// gladi386.so:   00010010..000100EE
void *__cdecl sub_10007C40(FILE *Stream, int Offset, size_t ElementSize, int a4, char *ArgList)
{
  void *v6; // esi

  if ( (int)ElementSize % a4 )
  {
    AAS_Error("odd %s bsp lump size\n", ArgList);
    AAS_DumpBSPData();
    fclose(Stream);
    return 0;
  }
  if ( fseek(Stream, Offset, 0) )
  {
    AAS_Error("can't seek to bsp lump %s\n", ArgList);
    AAS_DumpBSPData();
    fclose(Stream);
    return 0;
  }
  v6 = GetClearedMemory(ElementSize);
  if ( fread(v6, ElementSize, 1u, Stream) != 1 )
  {
    AAS_Error("can't read bsp lump %s\n", ArgList);
    FreeMemory(v6);
    AAS_DumpBSPData();
    fclose(Stream);
    return 0;
  }
  return v6;
}

// gladiator.dll: 10007D30..10008422
// gladi386.so:   000100F0..000111BF
int AAS_LoadBSPFile(char *FileName, int Offset, int Length)
{
  FILE *v3; // eax
  FILE *v4; // esi
  int v6; // eax
  int    ofs;
  size_t length;
  int v7; // eax
  dBspHeader_t bsp_h; /* Q2 BSP header: ident+version+19 lumps = 0xA0 bytes */

  AAS_DumpBSPData();
  v3 = DK_OpenBSP(FileName);  /* dkbot */
  v4 = v3;
  if ( !v3 )
  {
    AAS_Error("can't open bsp file %s\n", FileName);
    return BLERR_CANNOTOPENBSPFILE;
  }
  if ( fseek(v3, Offset, 0) )
  {
    AAS_Error("can't seek to bsp file %s\n");
    fclose(v4);
    return BLERR_CANNOTSEEKTOBSPFILE;
  }
  if ( fread(&bsp_h, 0xA0u, 1u, v4) != 1 )
  {
    AAS_Error("can't read header of bsp file %s\n", FileName);
    fclose(v4);
    return BLERR_CANNOTREADBSPHEADER;
  }
  v6 = bsp_h.ident = LittleLong(bsp_h.ident);
  if ( v6 != 1347633737 )
  {
    AAS_Error("%s is not an BSP file\n", FileName);
    fclose(v4);
    return BLERR_WRONGBSPFILEID;
  }
  v7 = bsp_h.version = LittleLong(bsp_h.version);
  if ( v7 != 38 )
  {
    AAS_Error("bsp file %s is version %i, not %i\n", FileName, v7, 38);
    fclose(v4);
    return BLERR_WRONGBSPFILEVERSION;
  }
  ofs = Offset + LittleLong(bsp_h.lumps[0].fileofs);
  length = LittleLong(bsp_h.lumps[0].filelen);
  bspworld.dentdata = sub_10007C40(v4, ofs, length, 1, "entity");
  if ( !bspworld.dentdata )
    return BLERR_CANNOTREADBSPLUMP;
  bspworld.entdatasize = length;
  ofs = Offset + LittleLong(bsp_h.lumps[1].fileofs);
  length = LittleLong(bsp_h.lumps[1].filelen);
  bspworld.dplanes = sub_10007C40(v4, ofs, length, 20, "planes");
  if ( !bspworld.dplanes )
    return BLERR_CANNOTREADBSPLUMP;
  bspworld.numplanes = length / 0x14;
  ofs = Offset + LittleLong(bsp_h.lumps[2].fileofs);
  length = LittleLong(bsp_h.lumps[2].filelen);
  bspworld.dvertexes = sub_10007C40(v4, ofs, length, 12, "vertexes");
  if ( !bspworld.dvertexes )
    return BLERR_CANNOTREADBSPLUMP;
  bspworld.numvertexes = length / 0xC;
  ofs = Offset + LittleLong(bsp_h.lumps[3].fileofs);
  length = LittleLong(bsp_h.lumps[3].filelen);
  if ( length )
  {
    bspworld.dvisdata = sub_10007C40(v4, ofs, length, 1, "visibility");
    if ( !bspworld.dvisdata )
      return BLERR_CANNOTREADBSPLUMP;
  }
  else
  {
    bspworld.dvisdata = 0;
    botimport.Print(PRT_MESSAGE, "WARNGING: bsp has no visibility data\n");
  }
  bspworld.visdatasize = length;
  bspworld.dvis = (dvis_t *)bspworld.dvisdata;
  ofs = Offset + LittleLong(bsp_h.lumps[4].fileofs);
  length = LittleLong(bsp_h.lumps[4].filelen);
  bspworld.dnodes = sub_10007C40(v4, ofs, length, 28, "nodes");
  if ( !bspworld.dnodes )
    return BLERR_CANNOTREADBSPLUMP;
  bspworld.numnodes = length / 0x1C;
  ofs = Offset + LittleLong(bsp_h.lumps[5].fileofs);
  length = LittleLong(bsp_h.lumps[5].filelen);
  bspworld.texinfo = sub_10007C40(v4, ofs, length, 76, "texinfo");
  if ( !bspworld.texinfo )
    return BLERR_CANNOTREADBSPLUMP;
  bspworld.numtexinfo = length / 0x4C;
  ofs = Offset + LittleLong(bsp_h.lumps[6].fileofs);
  length = LittleLong(bsp_h.lumps[6].filelen);
  bspworld.dfaces = sub_10007C40(v4, ofs, length, 20, "faces");
  if ( !bspworld.dfaces )
    return BLERR_CANNOTREADBSPLUMP;
  bspworld.numfaces = length / 0x14;
  ofs = Offset + LittleLong(bsp_h.lumps[7].fileofs);
  length = LittleLong(bsp_h.lumps[7].filelen);
  if ( length )
  {
    bspworld.dlightdata = sub_10007C40(v4, ofs, length, 1, "lightning");
    if ( !bspworld.dlightdata )
      return BLERR_CANNOTREADBSPLUMP;
  }
  else
  {
    bspworld.dlightdata = 0;
    botimport.Print(PRT_MESSAGE, "WARNING: bsp has no light data\n");
  }
  bspworld.lightdatasize = length;
  ofs = Offset + LittleLong(bsp_h.lumps[8].fileofs);
  length = LittleLong(bsp_h.lumps[8].filelen);
  bspworld.dleafs = sub_10007C40(v4, ofs, length, 28, "leafs");
  if ( !bspworld.dleafs )
    return BLERR_CANNOTREADBSPLUMP;
  bspworld.numleafs = length / 0x1C;
  ofs = Offset + LittleLong(bsp_h.lumps[9].fileofs);
  length = LittleLong(bsp_h.lumps[9].filelen);
  bspworld.dleaffaces = sub_10007C40(v4, ofs, length, 2, "leaf faces");
  if ( !bspworld.dleaffaces )
    return BLERR_CANNOTREADBSPLUMP;
  bspworld.numleaffaces = length >> 1;
  ofs = Offset + LittleLong(bsp_h.lumps[10].fileofs);
  length = LittleLong(bsp_h.lumps[10].filelen);
  bspworld.dleafbrushes = sub_10007C40(v4, ofs, length, 2, "leaf brushes");
  if ( !bspworld.dleafbrushes )
    return BLERR_CANNOTREADBSPLUMP;
  bspworld.numleafbrushes = length >> 1;
  ofs = Offset + LittleLong(bsp_h.lumps[11].fileofs);
  length = LittleLong(bsp_h.lumps[11].filelen);
  bspworld.dedges = sub_10007C40(v4, ofs, length, 4, "edges");
  if ( !bspworld.dedges )
    return BLERR_CANNOTREADBSPLUMP;
  bspworld.numedges = length >> 2;
  ofs = Offset + LittleLong(bsp_h.lumps[12].fileofs);
  length = LittleLong(bsp_h.lumps[12].filelen);
  bspworld.dsurfedges = sub_10007C40(v4, ofs, length, 4, "surfedges");
  if ( !bspworld.dsurfedges )
    return BLERR_CANNOTREADBSPLUMP;
  bspworld.numsurfedges = length >> 2;
  ofs = Offset + LittleLong(bsp_h.lumps[13].fileofs);
  length = LittleLong(bsp_h.lumps[13].filelen);
  bspworld.dmodels = sub_10007C40(v4, ofs, length, 48, "models");
  if ( !bspworld.dmodels )
    return BLERR_CANNOTREADBSPLUMP;
  bspworld.nummodels = length / 0x30;
  ofs = Offset + LittleLong(bsp_h.lumps[14].fileofs);
  length = LittleLong(bsp_h.lumps[14].filelen);
  bspworld.dbrushes = sub_10007C40(v4, ofs, length, 12, "brushes");
  if ( !bspworld.dbrushes )
    return BLERR_CANNOTREADBSPLUMP;
  bspworld.numbrushes = length / 0xC;
  ofs = Offset + LittleLong(bsp_h.lumps[15].fileofs);
  length = LittleLong(bsp_h.lumps[15].filelen);
  bspworld.dbrushsides = sub_10007C40(v4, ofs, length, 4, "brush sides");
  if ( !bspworld.dbrushsides )
    return BLERR_CANNOTREADBSPLUMP;
  bspworld.numbrushsides = length >> 2;
  Q2_SwapBSPFile();
  bspworld.dword_100674C0 = 1;
  fclose(v4);
  CalcSurfaceExtents();
  sub_100030A0();
  sub_10003280();
  sub_100032D0();
  return BLERR_NOERROR;
}

// gladiator.dll: 100085F0..1000860B
// gladi386.so:   000111C0..000111F1
int sub_100085F0()
{
  return sub_10037850(aasworld.mapname, bspworld.dentdata, bspworld.entdatasize);
}
