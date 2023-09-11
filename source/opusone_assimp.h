#ifndef OPUSONE_ASSIMP_H
#define OPUSONE_ASSIMP_H

#include "opusone_common.h"
#include "opusone_math.h"
#include "opusone_linmath.h"

#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

enum texture_type
{
    TEXTURE_TYPE_DIFFUSE,
    TEXTURE_TYPE_SPECULAR,
    TEXTURE_TYPE_EMISSION,
    TEXTURE_TYPE_NORMAL,
    TEXTURE_TYPE_COUNT
};

struct imported_mesh
{
    u32 VertexCount;
    u32 IndexCount;
    u32 Textures[TEXTURE_TYPE_COUNT];
    
    vec3 *VertexPosition;
    vec3 *VertexTangent;
    vec3 *VertexBitangent;
    vec3 *VertexNormal;
    vec4 *VertexColor;
    vec2 *VertexUV;

    i32 *Indices;
};

struct imported_model
{
    u32 MeshCount;
    u32 TextureCount;
    imported_mesh *Meshes;
    simple_string *TexturePaths;
};

imported_model *
Assimp_LoadModel(memory_arena *AssetArena, const char *Path)
{
    const aiScene *AssimpScene = aiImportFile(Path,
                                              aiProcess_CalcTangentSpace |
                                              aiProcess_Triangulate |
                                              aiProcess_JoinIdenticalVertices |
                                              aiProcess_FlipUVs);

    // TODO: Handle these errors
    Assert(AssimpScene);
    Assert(!(AssimpScene->mFlags & AI_SCENE_FLAGS_INCOMPLETE));
    Assert(AssimpScene->mRootNode);

    imported_model *Model = PushStruct(AssetArena, imported_model);

    Model->MeshCount = AssimpScene->mNumMeshes;
    Model->Meshes = PushArray(AssetArena, Model->MeshCount, imported_mesh);

    for (u32 MeshIndex = 0;
         MeshIndex < Model->MeshCount;
         ++MeshIndex)
    {
        aiMesh *AssimpMesh = AssimpScene->mMeshes[MeshIndex];

        imported_mesh *Mesh = Model->Meshes[MeshIndex];

        Mesh->VertexCount = AssimpMesh->mNumVertices;
        Mesh->IndexCount = AssimpMesh->mNumFaces * 3;
        Mesh->TextureCount = AssimpMesh->;
    }
    
    return Model;
}

#endif
