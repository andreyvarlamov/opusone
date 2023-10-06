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

struct sphere
{
    vec3 Center;
    f32 Radius;
};

struct edge
{
    vec3 *A;
    vec3 *B;
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
    edge **Edges;

    u32 VertexCount;
    vec3 **Vertices;
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

polyhedron *
ComputePolyhedronFromVertices(memory_arena *Arena, memory_arena *TransientArena,
                              vec3 *ImportedVertices, u32 VertexCount, i32 *ImportedIndices, u32 IndexCount);

#endif
