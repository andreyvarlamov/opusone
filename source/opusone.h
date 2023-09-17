#ifndef OPUSONE_H
#define OPUSONE_H

#include "opusone_common.h"
#include "opusone_math.h"
#include "opusone_linmath.h"
#include "opusone_camera.h"
#include "opusone_assimp.h"

struct render_data_material
{
    u32 TextureIDs[TEXTURE_TYPE_COUNT];
};

// TODO: Should world object instance just point to a mesh?
// The problem is that when I'm rendering I'm going other way,
// I'm going by render meshes according to the index order.
// I would need to search through all world object instances by the render mesh ID,
// to get the transform info which is needed for rendering.
// But the thing is that the transform info is not in an optimal format for rendering anyways.
// Maybe later, when I will probably have something like an entity, an entity will just point to its
// render info. Then when processing entities as part of game logic, I will pre-calculate transform
// matrices, and put them in the right order for the renderer.
// Also this will decouple render data from world_object/entity/position, which is more of a game logic thing.
#define MAX_INSTANCES_PER_MESH 16
struct render_data_mesh
{
    u32 BaseVertexIndex; // NOTE: E.g. Index #0 for mesh will point to Index #BaseVertexIndex for render unit
    u32 StartingIndex;
    u32 IndexCount;

    u32 MaterialID;

    u32 InstanceIDs[MAX_INSTANCES_PER_MESH];
};

struct render_unit
{
    u32 VAO;
    u32 VBO;
    u32 EBO;

    u32 VertexCount;
    u32 MaxVertexCount;

    u32 IndexCount;
    u32 MaxIndexCount;

    u32 MaterialCount;
    render_data_material *Materials;

    u32 MeshCount;
    render_data_mesh *Meshes;

    render_unit *Next;
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
};

struct game_state
{
    memory_arena RootArena;
    
    memory_arena WorldArena;
    memory_arena RenderArena;
    memory_arena AssetArena;
    
    camera Camera;
    camera_control_scheme CameraControlScheme;

    render_unit RenderUnit;
    
    u32 WorldObjectBlueprintCount;
    world_object_blueprint *WorldObjectBlueprints;
    
    u32 WorldObjectInstanceCount;
    world_object_instance *WorldObjectInstances;

    u32 ShaderID;
};

#endif
