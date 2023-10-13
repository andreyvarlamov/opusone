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
#include "opusone_entity.h"

struct game_requested_controls
{
    b32 PlayerForward;
    b32 PlayerBackward;
    b32 PlayerLeft;
    b32 PlayerRight;
    b32 PlayerUp;
    b32 PlayerDown;
    b32 PlayerJump;

    b32 CameraForward;
    b32 CameraBackward;
    b32 CameraLeft;
    b32 CameraRight;
    b32 CameraUp;
    b32 CameraDown;
    b32 CameraIsIndependent;
};

struct game_state
{
    memory_arena RootArena;
    
    memory_arena WorldArena;
    memory_arena RenderArena;
    memory_arena AssetArena;
    memory_arena TransientArena;

    game_requested_controls RequestedControls;

    camera Camera;
    b32 CameraWillReset;
    f32 CameraYawToResetTo;
    f32 CameraPitchToResetTo;

    render_unit StaticRenderUnit;
    render_unit SkinnedRenderUnit;
    render_unit DebugDrawRenderUnit;
    render_unit ImmTextRenderUnit;

    entity_type_spec *EntityTypeSpecs;

    u32 EntityCount;
    entity Entities[100]; // TODO: This should be a bucket array / spatial hash table eventually

    font_info *ContrailOne;
    font_info *MajorMono;

    entity *PlayerEntity;
    
    f32 PlayerEyeHeight;
    vec3 PlayerVelocity;
    b32 PlayerAirborne;
    f32 PlayerSpecJumpVelocity;
    f32 PlayerSpecAccelerationValue;
    f32 PlayerSpecGravityValue;
    f32 PlayerSpecDragValue;

    b32 MouseControlledTemp;
    b32 GravityDisabledTemp;
};

#endif
