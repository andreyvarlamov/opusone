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

struct imported_material
{
    simple_string TexturePaths[TEXTURE_TYPE_COUNT];
};

struct imported_mesh
{
    u32 VertexCount;
    u32 IndexCount;
    u32 MaterialIndex;
    
    vec3 *VertexPositions;
    vec3 *VertexTangents;
    vec3 *VertexBitangents;
    vec3 *VertexNormals;
    vec4 *VertexColors;
    vec2 *VertexUVs;

    i32 *Indices;
};

struct imported_model
{
    u32 MeshCount;
    u32 MaterialCount;
    
    imported_mesh *Meshes;
    imported_material *Materials;
};

internal inline vec2
Assimp_ConvertVec2F(aiVector3D AssimpVec)
{
    vec2 Vec { AssimpVec.x, AssimpVec.y };
    return Vec;
}

internal inline vec3
Assimp_ConvertVec3F(aiVector3D AssimpVec)
{
    vec3 Vec { AssimpVec.x, AssimpVec.y, AssimpVec.z };
    return Vec;
}

internal inline vec4
Assimp_ConvertColor4F(aiColor4D AssimpColor) 
{
    vec4 Color { AssimpColor.r, AssimpColor.g, AssimpColor.b, AssimpColor.a };
    return Color;
}

imported_model *
Assimp_LoadModel(memory_arena *AssetArena, const char *Path)
{
    const aiScene *AssimpScene = aiImportFile(Path,
                                              aiProcess_CalcTangentSpace |
                                              aiProcess_Triangulate |
                                              aiProcess_JoinIdenticalVertices |
                                              aiProcess_FlipUVs |
                                              aiProcess_RemoveRedundantMaterials);

    // TODO: Handle these errors
    Assert(AssimpScene);
    Assert(!(AssimpScene->mFlags & AI_SCENE_FLAGS_INCOMPLETE));
    Assert(AssimpScene->mRootNode);

    imported_model *Model = PushStruct(AssetArena, imported_model);

    Model->MeshCount = AssimpScene->mNumMeshes;
    Model->Meshes = PushArray(AssetArena, Model->MeshCount, imported_mesh);

    if (AssimpScene->mNumMaterials > 0)
    {
        simple_string TestSimpleString = SimpleString("Hello World!");
        simple_string ModelPath = GetDirectoryFromPath(Path);
        
        Model->MaterialCount = AssimpScene->mNumMaterials + 1; // Material Index 0 -> No Material
        Model->Materials = PushArray(AssetArena, Model->MaterialCount, imported_material);
        for (u32 TextureType = 0;
             TextureType < TEXTURE_TYPE_COUNT;
             ++TextureType)
        {
            Model->Materials[0].TexturePaths[TextureType].Length = 0;
            Model->Materials[0].TexturePaths[TextureType].D[0] = '\0';
        }

        for (u32 MaterialIndex = 1;
             MaterialIndex < Model->MaterialCount;
             ++MaterialIndex)
        {
            imported_material *Material = Model->Materials + MaterialIndex;
            aiMaterial *AssimpMaterial = AssimpScene->mMaterials[MaterialIndex-1];

            aiTextureType TextureTypes[] = { 
                aiTextureType_DIFFUSE,
                aiTextureType_SPECULAR,
                aiTextureType_EMISSIVE,
                aiTextureType_HEIGHT
            };

            for (u32 TextureType = 0;
                 TextureType < TEXTURE_TYPE_COUNT;
                 ++TextureType)
            {
                if (aiGetMaterialTextureCount(AssimpMaterial, TextureTypes[TextureType]) > 0)
                {
                    aiString AssimpTextureFilename;
                    aiGetMaterialString(AssimpMaterial, AI_MATKEY_TEXTURE(TextureTypes[TextureType], 0), &AssimpTextureFilename);
                    if (AssimpTextureFilename.length > 0)
                    {
                        simple_string TexturePath = CatStrings(ModelPath.D, AssimpTextureFilename.C_Str());
                        // TODO: Even if paths longer 128 chars are uncommon, this is definitely something
                        // to make more robust in shipping code
                        Assert(TexturePath.Length != (TexturePath.BufferSize - 1));
                        Material->TexturePaths[TextureType] = TexturePath;
                    }
                }
            }
        }
    }
    else
    {
        Model->MaterialCount = 0;
    }
    
    for (u32 MeshIndex = 0;
         MeshIndex < Model->MeshCount;
         ++MeshIndex)
    {
        aiMesh *AssimpMesh = AssimpScene->mMeshes[MeshIndex];

        imported_mesh *Mesh = &Model->Meshes[MeshIndex];

        Mesh->VertexCount = AssimpMesh->mNumVertices;
        Mesh->IndexCount = AssimpMesh->mNumFaces * 3;

        Mesh->VertexPositions = PushArray(AssetArena, Mesh->VertexCount, vec3);
        Mesh->VertexTangents = PushArray(AssetArena, Mesh->VertexCount, vec3);
        Mesh->VertexBitangents = PushArray(AssetArena, Mesh->VertexCount, vec3);
        Mesh->VertexNormals = PushArray(AssetArena, Mesh->VertexCount, vec3);
        Mesh->VertexColors = PushArray(AssetArena, Mesh->VertexCount, vec4);
        Mesh->VertexUVs = PushArray(AssetArena, Mesh->VertexCount, vec2);
        Mesh->Indices = PushArray(AssetArena, Mesh->IndexCount, i32);

        for (u32 VertexIndex = 0;
             VertexIndex < Mesh->VertexCount;
             ++VertexIndex)
        {
            Mesh->VertexPositions[VertexIndex] = Assimp_ConvertVec3F(AssimpMesh->mVertices[VertexIndex]);
            Mesh->VertexTangents[VertexIndex] = Assimp_ConvertVec3F(AssimpMesh->mTangents[VertexIndex]);
            Mesh->VertexBitangents[VertexIndex] = Assimp_ConvertVec3F(AssimpMesh->mBitangents[VertexIndex]);
            Mesh->VertexNormals[VertexIndex] = Assimp_ConvertVec3F(AssimpMesh->mNormals[VertexIndex]);
            if (AssimpMesh->mTextureCoords[0])
            {
                Mesh->VertexUVs[VertexIndex] = Assimp_ConvertVec2F(AssimpMesh->mTextureCoords[0][VertexIndex]);
            }
            if (AssimpMesh->mColors[0])
            {
                Mesh->VertexColors[VertexIndex] = Assimp_ConvertColor4F(AssimpMesh->mColors[0][VertexIndex]);
            }
            else
            {
                Mesh->VertexColors[VertexIndex] = vec4 { 1.0f, 1.0f, 1.0f, 1.0f };
            }
        }

        i32 *IndexCursor = Mesh->Indices;
        for (u32 FaceIndex = 0;
             FaceIndex < AssimpMesh->mNumFaces;
             ++FaceIndex)
        {
            aiFace *AssimpFace = &AssimpMesh->mFaces[FaceIndex];
            for (u32 IndexIndex = 0;
                 IndexIndex < 3;
                 ++IndexIndex)
            {
                *IndexCursor++ = AssimpFace->mIndices[IndexIndex];
            }
        }

        if (Model->MaterialCount > 0)
        {
            Mesh->MaterialIndex = AssimpMesh->mMaterialIndex + 1;
        }
        else
        {
            Mesh->MaterialIndex = 0;
        }
    }

    return Model;
}

#endif
