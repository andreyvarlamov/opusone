#ifndef OPUSONE_RENDER_H
#define OPUSONE_RENDER_H

#include "opusone_common.h"
// TODO: Maybe wrap GLenum for data types and usage, so can delay including glad until the source file
#include <glad/glad.h>

enum gl_vert_attrib_type
{
    OO_GL_VERT_ATTRIB_INT,
    OO_GL_VERT_ATTRIB_FLOAT,
    OO_GL_VERT_ATTRIB_COUNT
};

enum vert_spec_type
{
    VERT_SPEC_STATIC_MESH,
    VERT_SPEC_SKINNED_MESH,
    VERT_SPEC_COUNT
};

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

    vert_spec_type VertSpecType;
    
    render_unit *Next;
};

u32
OpenGL_BuildShaderProgram(const char *VertexPath, const char *FragmentPath);

inline void
OpenGL_UseShader(u32 ShaderID);

inline b32
OpenGL_SetUniformInt(u32 ShaderID, const char *UniformName, i32 Value, b32 UseProgram);

inline b32
OpenGL_SetUniformVec3F(u32 ShaderID, const char *UniformName, f32 *Value, b32 UseProgram);

inline b32
OpenGL_SetUniformVec4F(u32 ShaderID, const char *UniformName, f32 *Value, b32 UseProgram);

inline b32
OpenGL_SetUniformMat3F(u32 ShaderID, const char *UniformName, f32 *Value, b32 UseProgram);

inline b32
OpenGL_SetUniformMat4F(u32 ShaderID, const char *UniformName, f32 *Value, b32 UseProgram);

void
OpenGL_PrepareVertexDataHelper(u32 VertexCount, u32 IndexCount,
                               size_t *AttribStrides, u8 *AttribComponentCounts,
                               GLenum *GLDataTypes, gl_vert_attrib_type *GLAttribTypes, u32 AttribCount,
                               size_t IndexSize, GLenum GLUsage,
                               u32 *Out_VAO, u32 *Out_VBO, u32 *Out_EBO);

void
OpenGL_SubVertexDataHelper(u32 StartingVertex, u32 VertexCount, u32 MaxVertexCount,
                           u32 StartingIndex, u32 IndexCount,
                           size_t *AttribStrides, u32 AttribCount, size_t IndexSize,
                           u32 VBO, u32 EBO, void **AttribData, void *IndicesData);

u32
OpenGL_LoadTexture(u8 *ImageData, u32 Width, u32 Height, u32 Pitch, u32 BytesPerPixel);

void
OpenGL_BindAndActivateTexture(u32 TextureUnitIndex, u32 TextureID);

void
PrepareVertexDataForRenderUnit(render_unit *RenderUnit);

void
SubVertexDataForRenderUnit(render_unit *RenderUnit,
                           void **AttribData, u32 AttribCount, void *IndicesData,
                           u32 VertexToSubCount, u32 IndexToSubCount);

void
BindTexturesForMaterial(render_data_material *Material);

#endif
