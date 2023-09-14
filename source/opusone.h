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

#define VERTICES_PER_RENDER_UNIT 32768
#define INDICES_PER_RENDER_UNIT 131072
struct render_unit
{
    u32 VAO;
    u32 VBO;
    u32 EBO;

    u32 VertexCount;
    u32 MaxVertexCount;
    
    u32 IndexCount;
    u32 MaxIndexCount;

    u32 VertexSpecID;

    render_unit *Next;
};

struct render_data_material
{
    u32 TextureIDs[TEXTURE_TYPE_COUNT];
};

struct render_data_mesh
{
    u32 RenderUnitID;
    u32 MaterialID;

    world_object *WorldObjectPtrs[16];
    
    u32 StartingIndex;
    u32 IndexCount;
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

    render_unit *FirstRenderUnit;

    u32 MaterialCount;
    render_data_material *Materials;
    
    u32 MeshCount;
    render_data_mesh *Meshes;
    
    u32 ImportedModelCount;
    imported_model **ImportedModels;

    u32 ShaderID;
};

#endif
