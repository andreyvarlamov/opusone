#ifndef OPUSONE_H
#define OPUSONE_H

#include "opusone_common.h"
#include "opusone_math.h"
#include "opusone_linmath.h"
#include "opusone_camera.h"
#include "opusone_assimp.h"

struct world_object
{
    vec3 Position;
    quat Rotation;
    vec3 Scale;
};

struct render_data_material
{
    u32 TextureIDs[TEXTURE_TYPE_COUNT];
};

struct render_data_mesh
{
    u32 StartingIndex;
    u32 IndexCount;

    u32 MaterialID;

    // TODO: How to make this dynamic, so I can render more instances of this mesh?
    u32 WorldObjectCount;
    world_object **WorldObjectPtrs;
};

struct render_unit
{
    u32 VAO;
    u32 VBO;
    u32 EBO;

    u32 VertexCount;
    u32 IndexCount;

    // TODO: How to make this dynamic, so I can add more materials and meshes
    // if there's space for vertices/indices on GPU?
    u32 MaterialCount;
    render_data_material *Materials;

    u32 MeshCount;
    render_data_mesh *Meshes;

    render_unit *Next;
};

struct game_state
{
    memory_arena RootArena;
    
    memory_arena WorldArena;
    memory_arena RenderArena;
    memory_arena AssetArena;
    
    camera Camera;
    camera_control_scheme CameraControlScheme;
    
    u32 WorldObjectCount;
    world_object *WorldObjects;

    render_unit RenderUnit;
    
    u32 ImportedModelCount;
    imported_model **ImportedModels;

    u32 ShaderID;
};

#endif
