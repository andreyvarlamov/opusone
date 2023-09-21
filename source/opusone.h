#ifndef OPUSONE_H
#define OPUSONE_H

#include "opusone_common.h"
#include "opusone_math.h"
#include "opusone_linmath.h"
#include "opusone_camera.h"
#include "opusone_assimp.h"
#include "opusone_render.h"

struct animation_state
{
    imported_armature *Armature;
    imported_animation *Animation;

    double CurrentTicks;
};

struct world_object_blueprint
{
    imported_model *ImportedModel;

    render_unit *RenderUnit;
    u32 BaseMeshID;
    u32 MeshCount;
};

struct world_object_instance
{
    u32 BlueprintID;
    
    vec3 Position;
    quat Rotation;
    vec3 Scale;

    animation_state *AnimationState;
};

struct glyph_info
{
    vec2 GlyphUVs[4];

    i32 MinX;
    i32 MaxX;
    i32 MinY;
    i32 MaxY;
    i32 Advance;
};

struct font_info
{
    u32 GlyphCount;
    glyph_info *GlyphInfos;

    u32 TextureID;
    u32 PointSize;
    u32 Height;
};

struct game_state
{
    memory_arena RootArena;
    
    memory_arena WorldArena;
    memory_arena RenderArena;
    memory_arena AssetArena;
    
    camera Camera;

    render_unit StaticRenderUnit;
    render_unit SkinnedRenderUnit;
    render_unit DebugDrawRenderUnit;
    render_unit ImmTextRenderUnit;
    
    u32 WorldObjectBlueprintCount;
    world_object_blueprint *WorldObjectBlueprints;
    
    u32 WorldObjectInstanceCount;
    world_object_instance *WorldObjectInstances;

    font_info *ContrailOne24;
};

#endif
