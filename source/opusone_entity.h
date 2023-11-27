#ifndef OPUSONE_ENTITY_H
#define OPUSONE_ENTITY_H

#include "opusone_assimp.h"
#include "opusone_common.h"
#include "opusone_linmath.h"
#include "opusone_render.h"
#include "opusone_collision.h"
#include "opusone_animation.h"

enum entity_type
{
    EntityType_None = 0,
    EntityType_BoxRoom,
    EntityType_Player,
    EntityType_Enemy,
    EntityType_Thing,
    EntityType_Container,
    EntityType_Snowman,
    EntityType_ObstacleCourse,
    EntityType_Count
};

struct entity_type_spec
{
    imported_model *ImportedModel;
    
    render_unit *RenderUnit;
    u32 BaseMeshID;
    u32 MeshCount;

    collision_geometry *CollisionGeometry;
    collision_type CollisionType;

    u32 MaxHealth;
};

struct world_position
{
    vec3 P;
    quat R;
    vec3 S;
};

inline world_position
WorldPosition(vec3 P, quat R, vec3 S)
{
    world_position Result = {};

    Result.P = P;
    Result.R = R;
    Result.S = S;

    return Result;
}

inline mat4
WorldPositionTransform(world_position *WorldPosition)
{
    mat4 Result = Mat4GetFullTransform(WorldPosition->P, WorldPosition->R, WorldPosition->S);
    return Result;
}

inline void
WorldPositionPointTransform(vec3 *Point, world_position *WorldPosition)
{
    FullTransformPoint(Point, WorldPosition->P, WorldPosition->R, WorldPosition->S);
}

struct entity
{
    entity_type Type;

    world_position WorldPosition;
    b32 IsInvisible;

    animation_state *AnimationState;
};

struct game_state;
entity *AddEntity(game_state *GameState,
                  entity_type EntityType, vec3 Position, quat Rotation, vec3 Scale, b32 IsInvisible = false);

#endif
