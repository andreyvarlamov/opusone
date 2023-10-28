#ifndef OPUSONE_COLLISION_H
#define OPUSONE_COLLISION_H

#include "opusone_common.h"
#include "opusone_linmath.h"

#define COLLISION_ITERATIONS 3
#define COLLISION_CONTACTS_PER_ITERATION 4

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

enum feature_type
{
    FeatureType_FaceA,
    FeatureType_FaceB,
    FeatureType_EdgeCross
};

struct collision_feature
{
    feature_type FeatureType;
    u32 EdgeAIndex;
    vec3 EdgeA;
    u32 EdgeBIndex;
    vec3 EdgeB;
    u32 FaceIndex;
    vec3 FaceNormal;
};

struct collision_contact
{
    vec3 Normal;
    f32 Depth;
    entity *Entity;
    u32 PolyhedronIndex;
    collision_feature Feature;
};

inline collision_contact
CollisionContact()
{
    collision_contact Result = {};

    Result.Depth = FLT_MAX;

    return Result;
}

inline void
InitializeContactArray(collision_contact *Contacts, u32 ContactCount)
{
    for (u32 ContactIndex = 0;
         ContactIndex < ContactCount;
         ++ContactIndex, ++Contacts)
    {
        *Contacts = CollisionContact();
    }
}

inline b32
PopulateContact(collision_contact *Contact, vec3 Normal, f32 Depth, entity *Entity, u32 PolyhedronIndex)
{
    Assert(Contact);
    
    if (Depth < Contact->Depth)
    {
        Contact->Normal = Normal;
        Contact->Depth = Depth;
        Contact->Entity = Entity;
        Contact->PolyhedronIndex = PolyhedronIndex;
        return true;
    }

    return false;
}

inline b32
PopulateContactArray(collision_contact *Contacts, u32 ContactCount, vec3 Normal, f32 Depth, entity *Entity, u32 PolyhedronIndex)
{
    b32 ContactAdded = PopulateContact(Contacts + ContactCount-1, Normal, Depth, Entity, PolyhedronIndex);
    b32 NeedAnotherSortingIteration = ContactAdded;

    while (NeedAnotherSortingIteration)
    {
        NeedAnotherSortingIteration = false;
        for (i32 ContactIndex = (i32) ContactCount - 1;
             ContactIndex > 0;
             --ContactIndex)
        {
            if (Contacts[ContactIndex].Depth < Contacts[ContactIndex-1].Depth)
            {
                collision_contact TempContact = Contacts[ContactIndex];
                Contacts[ContactIndex] = Contacts[ContactIndex-1];
                Contacts[ContactIndex-1] = TempContact;
                NeedAnotherSortingIteration = true;
            }
        }
    }

    return ContactAdded;
}

inline b32
IsGroundContact(collision_contact *Contact, f32 VerticalAngleThresholdDegrees = 30.0f)
{
    f32 VerticalSine = VecDot(Contact->Normal, Vec3(0,1,0));
    f32 VerticalCollisionAngleThreshold = ToRadiansF(VerticalAngleThresholdDegrees);
    b32 Result = (VerticalSine > SinF(VerticalCollisionAngleThreshold));
    return Result;
}

inline b32
AreContactsSame(collision_contact *ContactA, collision_contact *ContactB)
{
    b32 Result = (ContactA->Entity == ContactB->Entity &&
                  ContactA->PolyhedronIndex == ContactB->PolyhedronIndex &&
                  AreVecEqual(ContactA->Normal, ContactB->Normal));
    return Result;
}

void
ComputePolyhedronFromVertices(memory_arena *Arena, memory_arena *TransientArena,
                              vec3 *ImportedVertices, u32 VertexCount, i32 *ImportedIndices, u32 IndexCount,
                              polyhedron *Out_Polyhedron);

polyhedron_set *
CopyPolyhedronSet(memory_arena *Arena, polyhedron_set *PolyhedronSet);

polyhedron_set *
CopyAndTransformPolyhedronSet(memory_arena *Arena, polyhedron_set *Original, vec3 Position, quat Rotation, vec3 Scale);

b32
AreSeparatedBoxPolyhedron(polyhedron *Polyhedron, box *Box, vec3 *Out_SmallestOverlapAxis, f32 *Out_SmallestOverlap);

b32
IsThereASeparatingAxisTriBox(vec3 A, vec3 B, vec3 C, vec3 BoxCenter, vec3 BoxExtents, vec3 *BoxAxes, vec3 *Out_SmallestOverlapAxis, f32 *Out_SmallestOverlap, b32 *Out_OverlapAxisIsTriNormal);

b32
IsThereASeparatingAxisTriBoxFast(vec3 A, vec3 B, vec3 C, vec3 BoxCenter, vec3 BoxExtents, vec3 *BoxAxes, f32 *Out_TriNormalOverlap);

b32
IntersectRayAABB(vec3 P, vec3 D, vec3 BoxCenter, vec3 BoxExtents, f32 *Out_TMin, vec3 *Out_Q);

b32
IntersectRayTri(vec3 P, vec3 D, vec3 A, vec3 B, vec3 C, f32 *Out_U, f32 *Out_V, f32 *Out_W, f32 *Out_T);

struct game_state;
void
CheckCollisionsForEntity(game_state *GameState, entity *Entity, vec3 EntityTranslation,
                         u32 MaxClosestContactCount, collision_contact *Out_ClosestContacts);

vec3
GetCollidingVectorComponent(vec3 V, collision_contact *Contact);

#endif
