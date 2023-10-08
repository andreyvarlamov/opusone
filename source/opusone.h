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
#include "opusone_collision.h"

struct world_object_blueprint
{
    imported_model *ImportedModel;

    render_unit *RenderUnit;
    u32 BaseMeshID;
    u32 MeshCount;

    collision_geometry *CollisionGeometry;
    collision_type CollisionType;
};

struct world_object_instance
{
    u32 BlueprintID;
    
    vec3 Position;
    quat Rotation;
    vec3 Scale;

    b32 IsInvisible;
    
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
    b32 MouseControlledTemp;

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
