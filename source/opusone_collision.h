#ifndef OPUSONE_COLLISION_H
#define OPUSONE_COLLISION_H

#include "opusone_common.h"
#include "opusone_linmath.h"

enum collision_type
{
    COLLISION_TYPE_NONE,
    COLLISION_TYPE_AABB,
    COLLISION_TYPE_SPHERE,
    COLLISION_TYPE_POLYHEDRON_SET
};

struct aabb
{
    vec3 Center;
    vec3 Extents;
};

struct box
{
    vec3 Axes[3];
    vec3 Center;
    vec3 Extents;
};

struct sphere
{
    vec3 Center;
    f32 Radius;
};

struct edge
{
    u32 AIndex;
    u32 BIndex;
    vec3 AB;
};

struct plane
{
    vec3 Normal;
    f32 Distance;
};

struct polygon
{
    plane Plane;

    u32 EdgeCount;
    u32 *EdgeIndices;

    u32 VertexCount;
    u32 *VertexIndices;
};

struct polyhedron
{
    u32 VertexCount;
    vec3 *Vertices;

    u32 EdgeCount;
    edge *Edges;

    u32 FaceCount;
    polygon *Faces;
};

struct polyhedron_set
{
    u32 PolyhedronCount;
    polyhedron *Polyhedra;
};

union collision_geometry
{
    aabb AABB;
    sphere Sphere;
    polyhedron_set PolyhedronSet;
};

void
ComputePolyhedronFromVertices(memory_arena *Arena, memory_arena *TransientArena,
                              vec3 *ImportedVertices, u32 VertexCount, i32 *ImportedIndices, u32 IndexCount,
                              polyhedron *Out_Polyhedron);

polyhedron_set *
CopyPolyhedronSet(memory_arena *Arena, polyhedron_set *PolyhedronSet);

polyhedron_set *
CopyAndTransformPolyhedronSet(memory_arena *Arena, polyhedron_set *Original, vec3 Position, quat Rotation, vec3 Scale);

b32
IsSeparatedBoxPolyhedron(polyhedron *Polyhedron, box *Box, vec3 *Out_SmallestOverlapAxis, f32 *Out_SmallestOverlap);

b32
IsThereASeparatingAxisTriBox(vec3 A, vec3 B, vec3 C, vec3 BoxCenter, vec3 BoxExtents, vec3 *BoxAxes, vec3 *Out_SmallestOverlapAxis, f32 *Out_SmallestOverlap, b32 *Out_OverlapAxisIsTriNormal);

b32
IsThereASeparatingAxisTriBoxFast(vec3 A, vec3 B, vec3 C, vec3 BoxCenter, vec3 BoxExtents, vec3 *BoxAxes, f32 *Out_TriNormalOverlap);

b32
IntersectRayAABB(vec3 P, vec3 D, vec3 BoxCenter, vec3 BoxExtents, f32 *Out_TMin, vec3 *Out_Q);

b32
IntersectRayTri(vec3 P, vec3 D, vec3 A, vec3 B, vec3 C, f32 *Out_U, f32 *Out_V, f32 *Out_W, f32 *Out_T);

#endif
