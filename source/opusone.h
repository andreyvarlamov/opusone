#ifndef OPUSONE_H
#define OPUSONE_H

#include "opusone_common.h"
#include "opusone_math.h"
#include "opusone_linmath.h"
#include "opusone_camera.h"
#include "opusone_assimp.h"
#include "opusone_render.h"
#include "opusone_animation.h"
#include "opusone_immtext.h"

enum collision_type
{
    COLLISION_TYPE_NONE,
    COLLISION_TYPE_NONE_MOUSE,
    COLLISION_TYPE_MESH,
    COLLISION_TYPE_BV_AABB,
    COLLISION_TYPE_BV_SPHERE,
    COLLISION_TYPE_BV_HULL,
};

struct aabb
{
    vec3 Center;
    vec3 Extents;
};

#if 0
struct sphere
{
    vec3 Center;
    f32 Radius;
};

struct hull
{
    u32 VertexCount;
    u32 IndexCount;

    vec3 *Vertices;
    i32 *Indices;
};
#endif

union collision_geometry
{
    aabb AABB;
    /* sphere Sphere; */
    /* hull Hull; */
};

struct world_object_blueprint
{
    imported_model *ImportedModel;

    render_unit *RenderUnit;
    u32 BaseMeshID;
    u32 MeshCount;

    collision_geometry *BoundingVolume;
    collision_type CollisionType;
};

struct world_object_instance
{
    u32 BlueprintID;
    
    vec3 Position;
    quat Rotation;
    vec3 Scale;

    animation_state *AnimationState;
};

struct game_state
{
    memory_arena RootArena;
    
    memory_arena WorldArena;
    memory_arena RenderArena;
    memory_arena AssetArena;
    memory_arena TransientArena;
    
    camera Camera;
    b32 ForceFirstPersonTemp;

    render_unit StaticRenderUnit;
    render_unit SkinnedRenderUnit;
    render_unit DebugDrawRenderUnit;
    render_unit ImmTextRenderUnit;
    
    u32 WorldObjectBlueprintCount;
    world_object_blueprint *WorldObjectBlueprints;
    
    u32 WorldObjectInstanceCount;
    world_object_instance *WorldObjectInstances;

    font_info *ContrailOne;
    font_info *MajorMono;

    f32 PlayerCameraYOffset;
    u32 PlayerWorldInstanceID;

    b32 DebugCollisions;
};

#endif
