/*
 * be_aas_sample.c — Gladiator Bot v0.96 botlib (Mr. Elusive, 1999), reconstructed
 * from the Windows gladiator.dll.  DLL extent 0x1001AC00..0x1001C6C0.
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
#include "be_aas_sample.h"
#include "be_aas_bspq2.h"
#include "be_aas_main.h"
#include "be_interface.h"
#include "l_libvar.h"
#include "l_memory.h"

// gladiator.dll: 1001AC00..1001ACC2
// gladi386.so:   00028A34..00028CEE
void AAS_InitAASLinkHeap()
{
  aas_link_t *heap;
  int i;
  int count;

  count = aasworld.linkheapsize;
  heap = aasworld.linkheap;
  if ( !heap )
  {
    count = (int)(intptr_t)LibVarValue("max_aaslinks", (char *)"4096");
    if ( count < 0 )
      count = 0;
    aasworld.linkheapsize = count;
    heap = (aas_link_t *)GetMemory(sizeof(aas_link_t) * count);
    aasworld.linkheap = heap;
  }
  aasworld.linkheap[0].prev_ent = NULL;
  aasworld.linkheap[0].next_ent = &aasworld.linkheap[1];
  for ( i = 1; i < count - 1; ++i )
  {
    aasworld.linkheap[i].prev_ent = &aasworld.linkheap[i - 1];
    aasworld.linkheap[i].next_ent = &aasworld.linkheap[i + 1];
  }
  aasworld.linkheap[count - 1].prev_ent = &aasworld.linkheap[count - 2];
  aasworld.linkheap[count - 1].next_ent = NULL;
  aasworld.freelinks = aasworld.linkheap;
}

// gladiator.dll: 1001AD10..1001AD33
// gladi386.so:   00028CF0..00028D38
void AAS_FreeAASLinkHeap()
{
  if ( aasworld.linkheap )
    FreeMemory(aasworld.linkheap);
  aasworld.linkheap = NULL;
  aasworld.linkheapsize = 0;
}

// gladiator.dll: 1001AD50..1001AD81
// gladi386.so:   00028D38..00028D86
aas_link_t *AAS_AllocAASLink()
{
  aas_link_t *result;
  aas_link_t *next;

  result = aasworld.freelinks;
  if ( !result )
  {
    botimport.Print(PRT_FATAL, "empty aas link heap\n");
    return NULL;
  }
  next = result->next_ent;
  aasworld.freelinks = next;
  if ( next )
    next->prev_ent = NULL;
  return result;
}

// gladiator.dll: 1001ADA0..1001ADCB
// gladi386.so:   00028D88..00028DD8
void __cdecl AAS_DeAllocAASLink(aas_link_t *link)
{
  if ( aasworld.freelinks )
    aasworld.freelinks->prev_ent = link;
  link->prev_ent = NULL;
  link->next_ent = aasworld.freelinks;
  link->prev_area = NULL;
  link->next_area = NULL;
  aasworld.freelinks = link;
}

// gladiator.dll: 1001ADE0..1001AE16
// gladi386.so:   00028DD8..00028E2B
void AAS_InitAASLinkedEntities(void)
{
  if ( aasworld.loaded )
  {
    if ( aasworld.arealinkedentities )
      FreeMemory(aasworld.arealinkedentities);
    aasworld.arealinkedentities = (aas_link_t **)GetClearedMemory(sizeof(aas_link_t *) * aasworld.numareas);
  }
}

// gladiator.dll: 1001AE30..1001AE4D
// gladi386.so:   00028E2C..00028E64
void AAS_FreeAASLinkedEntities()
{
  if ( aasworld.arealinkedentities )
    FreeMemory(aasworld.arealinkedentities);
  aasworld.arealinkedentities = NULL;
}

// gladiator.dll: 1001AE60..1001AEDE
// gladi386.so:   00028E64..00028F11
int __cdecl AAS_PointAreaNum(vec3_t point)
{
  int nodenum; // eax
  aas_node_t *node; // ecx
  aas_plane_t *plane;
  vec_t dist;

  if ( !aasworld.loaded )
  {
    botimport.Print(PRT_ERROR, "AAS_PointAreaNum: aas not loaded\n");
    return 0;
  }
  nodenum = 1;
  do
  {
    node = &aasworld.nodes[nodenum];
    plane = &aasworld.planes[node->planenum];
    dist = (point[0]*plane->normal[0]) + (point[1]*plane->normal[1]) + (point[2]*plane->normal[2]) - plane->dist;
    if ( dist > 0.0f )
      nodenum = node->children[0];
    else
      nodenum = node->children[1];
  }
  while ( nodenum > 0 );
  if ( !nodenum )
    return 0;
  return -nodenum;
}

// gladiator.dll: 1001AF00..1001AF37
// gladi386.so:   00028F14..00028F68
// Returns aasworld.areasettings[areanum].cluster (struct offset +0xC into the
// 28-byte aas_areasettings_t).  No aasworld.loaded guard, unlike
// AAS_AreaPresenceType.  Uses `areanum > 0` (not `>= 0`) so area 0 is treated as
// OOR; on OOR prints "AAS_AreaCluster: invalid area number\n" at level 3 and
// returns 0.
int __cdecl AAS_AreaCluster(int areanum)
{
  if ( areanum <= 0 || areanum >= aasworld.numareas )
  {
    botimport.Print(PRT_ERROR, "AAS_AreaCluster: invalid area number\n");
    return 0;
  }
  return aasworld.areasettings[areanum].cluster;
}

// gladiator.dll: 1001AF50..1001AF90
// gladi386.so:   00028F68..00028FC4
int __cdecl AAS_AreaPresenceType(int areanum)
{
  if ( !aasworld.loaded )
    return 0;
  if ( areanum <= 0 || areanum >= aasworld.numareas )
  {
    botimport.Print(PRT_ERROR, "AAS_AreaPresenceType: invalid area number\n");
    return 0;
  }
  return aasworld.areasettings[areanum].presencetype;
}

// gladiator.dll: 1001AFA0..1001AFD7
// gladi386.so:   00028FC4..0002907B
/* Q3 be_aas_sample.c's AAS_PointPresenceType, verbatim (1 = PRESENCE_NONE), in its
 * Q3 slot after AAS_AreaPresenceType.  Its one caller is the crouch test in
 * AAS_ClientMovementPrediction.  It used to carry the name AAS_PointContents, which
 * belongs to the botimport wrapper at 0x10003080 -- and one call site had followed
 * the name to the wrong function (BotFinishTravel_WaterJump). */
int __cdecl AAS_PointPresenceType(vec3_t point)
{
  int areanum; // eax

  if ( !aasworld.loaded )
    return 0;
  areanum = AAS_PointAreaNum(point);
  if ( !areanum )
    return PRESENCE_NONE;
  return aasworld.areasettings[areanum].presencetype;
}

// gladiator.dll: 1001AFF0..1001B0F0
// gladi386.so:   0002907C..00029278
// Q3 be_aas_sample.c's AAS_BoxOriginDistanceFromPlane: pick the box corner that
// touches a plane first (side selects maxs-vs-mins per normal component, with the
// +/-0.001 BBOX_NORMAL_EPSILON deadband), then return its dot product with the
// negated normal on a local copy.  Q3's text verbatim, including the call to
// VectorInverse -- the function the DLL really calls here (thunk 0x1000147E ->
// 0x10043540), not the tree's surplus 1-arg VectorNegate.  Same slot as in Q3,
// right before AAS_AreaEntityCollision.  DEAD in Gladiator -- kept only by
// /INCREMENTAL.
float __cdecl AAS_BoxOriginDistanceFromPlane(vec3_t normal, vec3_t mins, vec3_t maxs, int side)
{
  vec3_t v1, v2;
  int i;

  //swap maxs and mins when on the other side of the plane
  if ( side )
  {
    //get a point of the box that would be one of the first
    //to collide with the plane
    for ( i = 0; i < 3; i++ )
    {
      if ( normal[i] > 0.001 ) v1[i] = maxs[i];
      else if ( normal[i] < -0.001 ) v1[i] = mins[i];
      else v1[i] = 0;
    }
  }
  else
  {
    //get a point of the box that would be one of the first
    //to collide with the plane
    for ( i = 0; i < 3; i++ )
    {
      if ( normal[i] > 0.001 ) v1[i] = mins[i];
      else if ( normal[i] < -0.001 ) v1[i] = maxs[i];
      else v1[i] = 0;
    }
  }
  //
  VectorCopy(normal, v2);
  VectorInverse(v2);
  return DotProduct(v1, v2);
}

// gladiator.dll: 1001B130..1001B214
// gladi386.so:   00029278..0002936A
qboolean __cdecl AAS_AreaEntityCollision(int areanum, char *start, vec3_t end, int presencetype, int passent, aas_trace_t *trace)
{
  aas_link_t *link; // esi
  vec3_t boxmins; // [esp+10h] [ebp-60h] BYREF
  vec3_t boxmaxs; // [esp+4h] [ebp-6Ch] BYREF
  bsp_trace_t bsptrace; // [esp+1Ch] [ebp-54h] BYREF
  int collision; // [esp+80h] [ebp+10h]

  AAS_PresenceTypeBoundingBox(presencetype, boxmins, boxmaxs);
  bsptrace.fraction = 1.0;
  collision = 0;
  /* Q3's plain `for` walk.  gcc rotates it, so the empty-list guard lands on the
   * post-loop `collision` test — an explicit `if (!link) goto fail;` instead lets
   * it thread straight to `return 0`, which is what the reconstruction used to
   * have.  Both originals disagree with that: the DLL merges the two tails anyway,
   * and gcc keeps the test.  Keep the positive `if (collision)` — the negative
   * guard is canonicalised back to this by gcc and never reaches the DLL. */
  for ( link = aasworld.arealinkedentities[areanum]; link; link = link->next_ent )
  {
    if ( link->entnum != passent )
    {
      if ( AAS_EntityCollision(link->entnum, start, boxmins, boxmaxs, end, MASK_PLAYERSOLID, &bsptrace) )
        collision = 1;
    }
  }
  if ( collision )
  {
    trace->startsolid = bsptrace.startsolid;
    trace->ent = bsptrace.ent;
    VectorCopy(bsptrace.endpos, trace->endpos);
    trace->area = 0;
    trace->planenum = 0;
    return 1;
  }
  return 0;
}

// gladiator.dll: 1001B260..1001B86F
// gladi386.so:   0002936C..00029C32
/* Sweep a presence-typed bbox along a line and return the first AAS-leaf hit
 * (fraction=1.0 if the trace clears the BSP).  The BSP traversal stack is
 * aas_tracestack_t frames walked by one tstack_p, as in Q3.
 *
 * Differences from Q3, all faithful to the original:
 *   - ON_EPSILON is 0.0005 (Q3's pre-bk010221 value).
 *   - No PLANE_X/Y/Z axial shortcut (Q3 added, then disabled, one).
 *   - frac is clamped to [0,1], not Q3's later [0.001,0.999].
 *   - No `if (front == back)` FPE guard.
 *
 * Returns by value, as in Q3; passent = -1 disables the entity collision test.
 */
aas_trace_t __cdecl AAS_TraceClientBBox(vec3_t start, vec3_t end,
                                        int presencetype, int passent)
{
  int side, nodenum, tmpplanenum;
  float front, back, frac;
  /* Function-scoped as in Q3: v1 = trace direction, v2 = traversed segment.  The
   * startsolid arms have NO VectorClear(v1), so the later plane-facing test reads a
   * stale v1 — an authentic original bug; Q3 added the clear. */
  vec3_t cur_start, cur_end, cur_mid, v1, v2;
  aas_tracestack_t tracestack[64];
  aas_tracestack_t *tstack_p;
  aas_node_t *aasnode;
  aas_plane_t *plane;
  aas_trace_t trace;

  memset(&trace, 0, sizeof(trace));
  if ( !aasworld.loaded )
    return trace;

  tstack_p = tracestack;
  /* we start with the whole line on the stack */
  VectorCopy(start, tstack_p->start);
  VectorCopy(end, tstack_p->end);
  tstack_p->planenum = 0;
  /* start with node 1 because node zero is a dummy for a solid leaf */
  tstack_p->nodenum = 1;     /* starting at the root of the tree */
  tstack_p++;

  while ( 1 )
  {
    /* pop up the stack */
    tstack_p--;
    /* if the trace stack is empty (ended up with a piece of the
     * line to be traced in an area) */
    if ( tstack_p < tracestack )
    {
      /* nothing was hit */
      trace.startsolid = 0;
      trace.fraction = 1.0;
      /* endpos is the end of the line */
      VectorCopy(end, trace.endpos);
      /* nothing hit */
      trace.ent = 0;
      trace.area = 0;
      trace.planenum = 0;
      return trace;
    }
    /* number of the current node to test the line against */
    nodenum = tstack_p->nodenum;
    /* if it is an area */
    if ( nodenum < 0 )
    {
      /* if can't enter the area because it hasn't got the right presence type */
      if ( !(aasworld.areasettings[-nodenum].presencetype & presencetype) )
      {
        /* if the start point is still the initial start point
         * NOTE: no need for epsilons because the points will be
         * exactly the same when they're both the start point */
        if ( tstack_p->start[0] == start[0]
          && tstack_p->start[1] == start[1]
          && tstack_p->start[2] == start[2] )
        {
          trace.startsolid = 1;
          trace.fraction = 0.0;
        }
        else
        {
          trace.startsolid = 0;
          VectorSubtract(end, start, v1);
          VectorSubtract(tstack_p->start, start, v2);
          trace.fraction = VectorLength(v2) / VectorNormalize(v1);
          VectorMA(tstack_p->start, -0.125, v1, tstack_p->start);
        }
        VectorCopy(tstack_p->start, trace.endpos);
        trace.ent = 0;
        trace.area = -nodenum;
        trace.planenum = tstack_p->planenum;
        /* always take the plane with normal facing towards the trace start */
        plane = &aasworld.planes[trace.planenum];
        if ( DotProduct(v1, plane->normal) > 0.0f )
          trace.planenum ^= 1;
        return trace;
      }
      else
      {
        if ( passent >= 0 )
        {
          if ( AAS_AreaEntityCollision(-nodenum, tstack_p->start,
                                       tstack_p->end, presencetype, passent,
                                       &trace) )
          {
            if ( !trace.startsolid )
            {
              VectorSubtract(end, start, v1);
              VectorSubtract(trace.endpos, start, v2);
              trace.fraction = VectorLength(v2) / VectorLength(v1);
            }
            return trace;
          }
        }
      }
      trace.lastarea = -nodenum;
      continue;
    }
    /* if it is a solid leaf */
    if ( !nodenum )
    {
      /* if the start point is still the initial start point
       * NOTE: no need for epsilons because the points will be
       * exactly the same when they're both the start point */
      if ( tstack_p->start[0] == start[0]
        && tstack_p->start[1] == start[1]
        && tstack_p->start[2] == start[2] )
      {
        trace.startsolid = 1;
        trace.fraction = 0.0;
      }
      else
      {
        trace.startsolid = 0;
        VectorSubtract(end, start, v1);
        VectorSubtract(tstack_p->start, start, v2);
        trace.fraction = VectorLength(v2) / VectorNormalize(v1);
        VectorMA(tstack_p->start, -0.125, v1, tstack_p->start);
      }
      VectorCopy(tstack_p->start, trace.endpos);
      trace.ent = 0;
      trace.area = 0;     /* hit solid leaf */
      trace.planenum = tstack_p->planenum;
      /* always take the plane with normal facing towards the trace start */
      plane = &aasworld.planes[trace.planenum];
      if ( DotProduct(v1, plane->normal) > 0.0f )
        trace.planenum ^= 1;
      return trace;
    }
    /* the node to test against */
    aasnode = &aasworld.nodes[nodenum];
    /* start point of current line to test against node */
    VectorCopy(tstack_p->start, cur_start);
    /* end point of the current line to test against node */
    VectorCopy(tstack_p->end, cur_end);
    /* the current node plane */
    plane = &aasworld.planes[aasnode->planenum];
    front = DotProduct(cur_start, plane->normal) - plane->dist;
    back = DotProduct(cur_end, plane->normal) - plane->dist;
    /* if the whole to be traced line is totally at the front of this node
     * only go down the tree with the front child */
    if ( front > -0.0005 && back > -0.0005 )
    {
      /* keep the current start and end point on the stack
       * and go down the tree with the front child */
      tstack_p->nodenum = aasnode->children[0];
      tstack_p++;
    }
    /* if the whole to be traced line is totally at the back of this node
     * only go down the tree with the back child */
    else if ( front < 0.0005 && back < 0.0005 )
    {
      /* keep the current start and end point on the stack
       * and go down the tree with the back child */
      tstack_p->nodenum = aasnode->children[1];
      tstack_p++;
    }
    /* go down the tree both at the front and back of the node */
    else
    {
      tmpplanenum = tstack_p->planenum;
      /* calculate the hitpoint with the node (split point of the line)
       * put the crosspoint TRACEPLANE_EPSILON pixels on the near side */
      if ( front < 0 )
        frac = (front + 0.125) / (front - back);
      else
        frac = (front - 0.125) / (front - back);
      /* dkbot: players rest TRACEPLANE_EPSILON above floors: step past a split on the start */
      if ( frac <= 0 )
        frac = 0.001f;
      else if ( frac > 1 )
        frac = 1;
      cur_mid[0] = cur_start[0] + (cur_end[0] - cur_start[0]) * frac;
      cur_mid[1] = cur_start[1] + (cur_end[1] - cur_start[1]) * frac;
      cur_mid[2] = cur_start[2] + (cur_end[2] - cur_start[2]) * frac;

      /* side the front part of the line is on */
      side = front < 0;
      /* first put the end part of the line on the stack (back side) */
      VectorCopy(cur_mid, tstack_p->start);
      /* not necessary to store because still on stack:
       * VectorCopy(cur_end, tstack_p->end); */
      tstack_p->planenum = aasnode->planenum;
      tstack_p->nodenum = aasnode->children[!side];
      tstack_p++;
      /* now put the part near the start of the line on the stack so we will
       * continue with that part first */
      VectorCopy(cur_start, tstack_p->start);
      VectorCopy(cur_mid, tstack_p->end);
      tstack_p->planenum = tmpplanenum;
      tstack_p->nodenum = aasnode->children[side];
      tstack_p++;
    }
  }
}

// gladiator.dll: 1001BA00..1001BC88
// gladi386.so:   00029C34..00029F40
/* Recursive subdivision of the line by the BSP tree, collecting all areas the line
 * passes through (up to maxareas).  Matches Q3's AAS_TraceAreas, but simpler: 4 args
 * (no separate `points` output), a 64-frame stack, and no TRACEPLANE_EPSILON
 * adjustment in the split.
 *
 * a1 = start (vec3*), a2 = end (vec3*), a3 = areas[] output, a4 = maxareas. */
int __cdecl AAS_TraceAreas(float *start, float *end, int *areas, int maxareas)
{
  int side, nodenum, tmpplanenum;
  int numareas;
  float front, back, frac;
  vec3_t cur_start, cur_end, cur_mid;
  aas_tracestack_t tracestack[64];
  aas_tracestack_t *tstack_p;
  aas_node_t *aasnode;
  aas_plane_t *plane;

  numareas = 0;
  areas[0] = 0;
  if ( !aasworld.loaded )
    return numareas;

  tstack_p = tracestack;
  VectorCopy(start, tstack_p->start);
  VectorCopy(end, tstack_p->end);
  tstack_p->planenum = 0;
  tstack_p->nodenum = 1;     /* root of BSP */
  tstack_p++;

  while ( 1 )
  {
    tstack_p--;
    if ( tstack_p < tracestack )
      return numareas;

    nodenum = tstack_p->nodenum;
    if ( nodenum < 0 )
    {
      areas[numareas] = -nodenum;
      numareas++;
      if ( numareas >= maxareas )
        return numareas;
      continue;
    }
    if ( !nodenum )
      continue;

    aasnode = &aasworld.nodes[nodenum];
    VectorCopy(tstack_p->start, cur_start);
    VectorCopy(tstack_p->end, cur_end);
    plane = &aasworld.planes[aasnode->planenum];
    front = DotProduct(cur_start, plane->normal) - plane->dist;
    back = DotProduct(cur_end, plane->normal) - plane->dist;

    if ( front > 0.0f && back > 0.0f )
    {
      tstack_p->nodenum = aasnode->children[0];
      tstack_p++;
    }
    else if ( front <= 0.0f && back <= 0.0f )
    {
      tstack_p->nodenum = aasnode->children[1];
      tstack_p++;
    }
    else
    {
      tmpplanenum = tstack_p->planenum;
      /* Keep the stripped-epsilon branch skeleton: collapsing it changes MSVC6's
       * x87 compare/divide schedule. */
      if ( front < 0.0f )
        frac = front / (front - back);
      else
        frac = front / (front - back);
      if ( frac < 0.0f )
        frac = 0.0f;
      else if ( frac > 1.0f )
        frac = 1.0f;
      cur_mid[0] = cur_start[0] + (cur_end[0] - cur_start[0]) * frac;
      cur_mid[1] = cur_start[1] + (cur_end[1] - cur_start[1]) * frac;
      cur_mid[2] = cur_start[2] + (cur_end[2] - cur_start[2]) * frac;

      side = front < 0.0f;
      VectorCopy(cur_mid, tstack_p->start);
      tstack_p->planenum = aasnode->planenum;
      tstack_p->nodenum = aasnode->children[!side];
      tstack_p++;

      VectorCopy(cur_start, tstack_p->start);
      VectorCopy(cur_mid, tstack_p->end);
      tstack_p->planenum = tmpplanenum;
      tstack_p->nodenum = aasnode->children[side];
      tstack_p++;
    }
  }
}

// gladiator.dll: 1001BD40..1001BE98
// gladi386.so:   00029F40..0002A0FD
// Four args (face, pnormal, point, epsilon) with an inlined CrossProduct +
// DotProduct loop, matching Q3's AAS_InsideFace; the callers' `add esp,0x10`
// cleanup confirms the count.  DEAD in Gladiator — only reachable via
// /INCREMENTAL thunks from sub_1001C0B0 and AAS_TraceEndFace.
qboolean __cdecl AAS_InsideFace(aas_face_t *face, vec3_t pnormal, vec3_t point, float epsilon)
{
  int i, firstvertex, edgenum;
  vec3_t v0;
  vec3_t edgevec, pointvec, sepnormal;
  aas_edge_t *edge;

  if ( !aasworld.loaded )
    return 0;
  for ( i = 0; i < face->numedges; i++ )
  {
    edgenum = aasworld.edgeindex[face->firstedge + i];
    edge = &aasworld.edges[abs(edgenum)];
    firstvertex = edgenum < 0;
    VectorCopy(aasworld.vertexes[edge->v[firstvertex]], v0);
    VectorSubtract(aasworld.vertexes[edge->v[!firstvertex]], v0, edgevec);
    VectorSubtract(point, v0, pointvec);
    sepnormal[0] = edgevec[1] * pnormal[2] - edgevec[2] * pnormal[1];
    sepnormal[1] = edgevec[2] * pnormal[0] - edgevec[0] * pnormal[2];
    sepnormal[2] = edgevec[0] * pnormal[1] - edgevec[1] * pnormal[0];
    if ( DotProduct(pointvec, sepnormal) < -epsilon )
      return 0;
  }
  return 1;
}

// gladiator.dll: 1001BF00..1001C045
// gladi386.so:   0002A100..0002A249
qboolean __cdecl AAS_PointInsideFace(int facenum, vec3_t point, float epsilon)
{
  int i;
  int edgenum;
  int firstvertex;
  vec_t *v1;
  vec_t *v2;
  aas_edge_t *edge;
  aas_plane_t *plane;
  vec3_t edgevec;
  vec3_t pointvec;
  vec3_t sepnormal;
  aas_face_t *face;

  if ( !aasworld.loaded )
    return 0;
  face = &aasworld.faces[facenum];
  plane = &aasworld.planes[face->planenum];
  for ( i = 0; i < face->numedges; i++ )
  {
    edgenum = aasworld.edgeindex[face->firstedge + i];
    edge = &aasworld.edges[abs(edgenum)];
    firstvertex = edgenum < 0;
    v1 = aasworld.vertexes[edge->v[firstvertex]];
    v2 = aasworld.vertexes[edge->v[!firstvertex]];
    VectorSubtract(v2, v1, edgevec);
    VectorSubtract(point, v1, pointvec);
    CrossProduct(edgevec, plane->normal, sepnormal);
    if ( DotProduct(pointvec, sepnormal) < -epsilon )
      return 0;
  }
  return 1;
}

// gladiator.dll: 1001C0B0..1001C17E
// gladi386.so:   0002A24C..0002A37E
// Scan an area's face list for the first face whose faceflags (offset +4) has
// FACE_GROUND set and that survives a predicate call into AAS_InsideFace with a +Z or
// -Z unit vector (chosen by the sign of the face plane's z-component) and a 0.01f
// epsilon.  Returns the matching face pointer or NULL.  aasworld globals: areas
// (stride 48 — numfaces at +4, firstface at +8), faceindex, faces pool (stride 24 —
// planenum at +0, faceflags at +4), planes pool (stride 20 — normal at +0..+8, dist
// at +12, signbits at +16).  DEAD in Gladiator — /INCREMENTAL.
void *__cdecl AAS_AreaGroundFace(int areanum, void *point)
{
  int    i;
  int    facenum;
  aas_face_t *face;
  aas_area_t *area;
  float  plane_z;
  vec3_t up = { 0.0f, 0.0f, 1.0f };
  vec3_t dir;

  if ( !aasworld.loaded )
    return 0;
  area = &aasworld.areas[areanum];
  for ( i = 0; i < area->numfaces; i++ )
  {
    facenum = aasworld.faceindex[area->firstface + i];
    face = &aasworld.faces[abs(facenum)];
    if ( !(face->faceflags & FACE_GROUND) )
      continue;
    plane_z = aasworld.planes[face->planenum].normal[2];
    if ( plane_z < 0.0f )
    {
      dir[0] = -up[0];
      dir[1] = -up[1];
      dir[2] = -up[2];
    }
    else
    {
      VectorCopy(up, dir);
    }
    if ( AAS_InsideFace(face, dir, (float *)point, 0.01f) )
      return face;
  }
  return 0;
}

// gladiator.dll: 1001C1C0..1001C1F9
// gladi386.so:   0002A380..0002A3D2
/* Copies a face's BSP plane (normal + dist) into the caller's buffers.
 * DEAD in Gladiator — live code walks aasworld.planes/faces directly. */
void __cdecl AAS_FacePlane(int facenum, vec3_t normal, float *dist)
{
  int    plane_idx;
  float *plane;

  plane_idx = aasworld.faces[facenum].planenum;
  plane     = (float *)&aasworld.planes[plane_idx];
  VectorCopy(plane, normal);
  *dist     = plane[3];
}

// gladiator.dll: 1001C210..1001C2A8
// gladi386.so:   0002A3D4..0002A4CE
// Q3 be_aas_sample.c's AAS_TraceEndFace, line for line, plus a leading
// `aasworld.loaded` guard: skip a startsolid trace (+0), scan the faces of
// trace->lastarea (+0x18) for one in the end plane ((planenum & ~1) against +0x20)
// and return it if AAS_InsideFace(face, plane, trace->endpos (+8), 0.01f).  Q3's
// text verbatim, down to the never-assigned `firstface` it returns: that variable
// and the wrapping `if` are what reproduce the .so's register allocation.
// DEAD -- kept only by /INCREMENTAL.
aas_face_t *__cdecl AAS_TraceEndFace(aas_trace_t *trace)
{
  int i, facenum;
  aas_area_t *area;
  aas_face_t *face, *firstface = NULL;

  if ( !aasworld.loaded )
    return NULL;
  //if started in solid no face was hit
  if ( trace->startsolid )
    return NULL;
  //trace->lastarea is the last area the trace was in
  area = &aasworld.areas[trace->lastarea];
  //check which face the trace.endpos was in
  for ( i = 0; i < area->numfaces; i++ )
  {
    facenum = aasworld.faceindex[area->firstface + i];
    face = &aasworld.faces[abs(facenum)];
    //if the face is in the same plane as the trace end point
    if ( (face->planenum & ~1) == (trace->planenum & ~1) )
    {
      if ( AAS_InsideFace(face, (float *)&aasworld.planes[face->planenum], trace->endpos, 0.01f) )
        return face;
    }
  }
  return firstface;
}

// gladiator.dll: 1001C2E0..1001C3A5
// gladi386.so:   0002A4D0..0002A5D8
/* Q3 be_aas_sample.c's AAS_BoxOnPlaneSide2, verbatim; its only caller is
 * AAS_AASLinkEntity, as in Q3.  The BSP-plane twin is sub_10006100. */
int __cdecl AAS_BoxOnPlaneSide2(float *a1, float *a2, float *a3)
{
  int   i, sides;
  vec3_t corners[2]; /* [0]=closer to plane normal, [1]=farther */
  float dist1, dist2;

  for ( i = 0; i < 3; ++i )
  {
    if ( a3[i] < 0.0f )
    {
      corners[0][i] = a1[i];
      corners[1][i] = a2[i];
    }
    else
    {
      corners[1][i] = a1[i];
      corners[0][i] = a2[i];
    }
  }
  dist1 = DotProduct(a3, corners[0]) - a3[3];
  dist2 = DotProduct(a3, corners[1]) - a3[3];
  sides = 0;
  if ( dist1 >= 0.0f )
    sides = 1;
  if ( dist2 < 0.0f )
    sides |= 2;
  return sides;
}

// gladiator.dll: 1001C3F0..1001C43A
// gladi386.so:   0002A5D8..0002A66D
void __cdecl AAS_UnlinkFromAreas(aas_link_t *areas)
{
  aas_link_t *result; // eax
  aas_link_t *prev;   // ecx — prev_ent in area chain
  aas_link_t *v3;     // esi — saved next_area for iteration
  aas_link_t *next;   // ecx — next_ent in area chain

  result = areas;
  if ( areas )
  {
    do
    {
      v3 = result->next_area;
      prev = result->prev_ent;
      if ( prev )
        prev->next_ent = result->next_ent;
      else
        aasworld.arealinkedentities[result->areanum] = result->next_ent;
      next = result->next_ent;
      if ( next )
        next->prev_ent = result->prev_ent;
      AAS_DeAllocAASLink(result);
      result = v3;
    }
    while ( v3 );
  }
}

// gladiator.dll: 1001C460..1001C5BA
// gladi386.so:   0002A670..0002A99F
aas_link_t *__cdecl AAS_AASLinkEntity(vec3_t absmins, vec3_t absmaxs, int entnum)
{
  aas_link_t *areas;
  aas_link_t *link;
  aas_link_t *next;
  aas_node_t *aasnode;
  aas_plane_t *plane;
  int *lstack_p;
  int nodenum;
  int type;
  int side;
  /* The BSP traversal queue is 256 bytes = 64 int slots in the original frame.  Sized
   * any smaller, pushing both children of a node runs off the array into adjacent
   * locals and corrupts the traversal, leaving nearly every level item with
   * goal_areanum 0.
   *
   * 64 is also the original's OVERFLOW bug, and it is reproduced deliberately: there
   * is no upper-bound check on lstack_p anywhere in this loop, in either 1999 binary.
   * The DLL frame is `sub esp,0x100` with linkstack at [esp+10h] after four pushes,
   * and gladi386.so's is `sub esp,0x128` with it at [esp+38h] after four pushes — in
   * both, linkstack[0] sits exactly 0x100 below the return address, so linkstack[64]
   * IS the return-address slot.  The only compare against the pointer is the
   * underflow test (DLL 1001C4AC, ELF 2A6DF); both push sites just store and add 4.
   * Q3 fixed this twice over in be_aas_sample.c: linkstack[128], plus an explicit
   * `if (lstack_p >= &linkstack[127]) { PRT_ERROR "AAS_LinkEntity: stack overflow";
   * break; }` after each push.  Either is a deviation, so both live behind
   * GLAD_SERVERFIX below — see .claude/memory/known_outstanding_bugs.md,
   * "AAS_AASLinkEntity linkstack overflow".
   *
   * Dense custom maps do exceed 64 (chaves needs up to 79; all 44 stock/RA2 maps
   * peak at 40).  Q3 also added a duplicate-leaf check this function lacks, so a
   * single entity can burn far more of the max_aaslinks heap than Q3 would.
   *
   * GLAD_SERVERFIX(aas-linkstack-overflow) builds Q3's fix instead: its array size
   * AND its post-push guards, both, because either alone is insufficient — 128 only
   * moves the cliff, and the guard alone silently truncates the area link on the 7%
   * of chaves positions that need more than 64. */
#if GLAD_SERVERFIX /* GLAD_SERVERFIX(aas-linkstack-overflow) */
  int linkstack[128]; // Q3's size
#else
  int linkstack[64]; // [esp+10h] [ebp-100h] BYREF — stack-based BSP traversal queue
#endif

  if ( !aasworld.loaded )
  {
    botimport.Print(PRT_ERROR, "AAS_LinkEntity: aas not loaded\n");
    return 0;
  }
  areas = 0;
  linkstack[0] = 1;
  lstack_p = &linkstack[1];
  while ( 1 )
  {
    --lstack_p;
    if ( lstack_p < &linkstack[0] )
      break;
    nodenum = *lstack_p;
    if ( nodenum < 0 )
    {
      link = AAS_AllocAASLink();
      if ( !link )
        return areas;
      link->entnum = entnum;
      link->areanum = -nodenum;
      link->prev_area = NULL;
      link->next_area = areas;
      if ( areas )
        areas->prev_area = link;
      areas = link;
      link->prev_ent = NULL;
      link->next_ent = ((aas_link_t **)aasworld.arealinkedentities)[-nodenum];
      next = ((aas_link_t **)aasworld.arealinkedentities)[-nodenum];
      if ( next )
        next->prev_ent = link;
      ((aas_link_t **)aasworld.arealinkedentities)[-nodenum] = link;
      continue;
    }
    if ( !nodenum )
      continue;
    aasnode = &aasworld.nodes[nodenum];
    plane = &aasworld.planes[aasnode->planenum];
    type = plane->type;
#if GLAD_SERVERFIX /* GLAD_SERVERFIX(aas-link-axial-sign) */
    /* Q3's AAS_AASLinkEntity: the full side test for every plane.  The axial shortcut
     * below compares dist against the box as if the normal pointed along +axis.  But
     * bspc stores every plane next to its negation, the +axis one first, and a node
     * that splits facing -axis gets the odd one (Q3 bspc aas_store.c, AAS_GetPlane);
     * 1-5% of the nodes in the stock and RA2 .aas files do.  There the shortcut takes
     * the wrong side, so the walk skips areas the box is in.  `type` stays set but
     * unused, so the #else arm's text is unchanged. */
    side = AAS_BoxOnPlaneSide2(absmins, absmaxs, plane->normal);
#else
    if ( type < 3 )
    {
      /* Q3's AAS_BoxOnPlaneSide macro, inlined: side&1 descends front child[0], side&2
       * descends back child[1].  The comparison polarity below is the original's and
       * matters behaviourally — inverted, a box straddling the plane classifies as
       * front-only instead of both children.  It is only right for a normal that points
       * along +axis; see the GLAD_SERVERFIX arm. */
      if ( plane->dist <= (float)absmins[type] )
        side = 1;
      else if ( plane->dist >= (float)absmaxs[type] )
        side = 2;
      else
        side = 3;
    }
    else
    {
      side = AAS_BoxOnPlaneSide2(absmins, absmaxs, plane->normal);
    }
#endif
    if ( (side & 1) != 0 )
      *lstack_p++ = aasnode->children[0];
#if GLAD_SERVERFIX /* GLAD_SERVERFIX(aas-linkstack-overflow) */
    if ( lstack_p >= &linkstack[127] )
    {
      botimport.Print(PRT_ERROR, "AAS_LinkEntity: stack overflow\n");
      break;
    }
#endif
    if ( (side & 2) != 0 )
      *lstack_p++ = aasnode->children[1];
#if GLAD_SERVERFIX /* GLAD_SERVERFIX(aas-linkstack-overflow) */
    if ( lstack_p >= &linkstack[127] )
    {
      botimport.Print(PRT_ERROR, "AAS_LinkEntity: stack overflow\n");
      break;
    }
#endif
  }
  return areas;
}

// gladiator.dll: 1001C620..1001C697
// gladi386.so:   0002A9A0..0002AA2E
/* AAS_LinkEntityClientBBox — adjust the entity bbox by the presence-type
 * bounding box, then link it into the AAS area tree. */
aas_link_t *__cdecl AAS_LinkEntityClientBBox(vec3_t absmins, vec3_t absmaxs, int entnum, int presencetype)
{
  vec3_t mins, maxs; // [esp+Ch]/[esp+0h] [ebp-24h]/[ebp-30h] BYREF
  vec3_t newabsmins, newabsmaxs; // [esp+24h]/[esp+18h] [ebp-Ch]/[ebp-18h] BYREF

  AAS_PresenceTypeBoundingBox(presencetype, mins, maxs);
  VectorSubtract(absmins, maxs, newabsmins);
  VectorSubtract(absmaxs, mins, newabsmaxs);
  return AAS_AASLinkEntity(newabsmins, newabsmaxs, entnum);
}

// gladiator.dll: 1001C6C0..1001C6DD
// gladi386.so:   0002AA30..0002AA60
char *__cdecl AAS_PlaneFromNum(int planenum)
{
  if ( !aasworld.loaded )
    return 0;
  return &aasworld.planes[planenum];
}
