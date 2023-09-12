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

#define MAX_BONE_CHILDREN 16
struct imported_bone
{
    u32 ID;
    u32 ParentID;
    // TODO: Do I actually need to store children?
    u32 ChildrenIDs[MAX_BONE_CHILDREN];
    u32 ChildrenCount;

    // TODO: Hash table to find bones instead of this?
    simple_string BoneName;
    mat4 TransformToParent;
    // TODO: This is in mesh space, but I apply transforms to all objects in Blender,
    // and don't take into account object transforms for now
    mat4 InverseBindTransform;
};

#define MAX_BONES_PER_VERTEX 4
struct vertex_bone_data
{
    u32 BoneIDs[MAX_BONES_PER_VERTEX];
    f32 BoneWeights[MAX_BONES_PER_VERTEX];
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

    vertex_bone_data *VertexBoneData;
    
    i32 *Indices;
};

struct imported_armature
{
    u32 BoneCount;
    imported_bone *Bones;
};

struct imported_animation_key
{
    vec3 Position;
    quat Rotation;
    // TODO: Process scaling in animations? As I understand it's not commonly used
    // because it makes it less efficient to recalculate normals and the whole tangent space.
    // I have to do transpose(inverse(Model * boneTransform)) for every vertex, instead of
    // calculating normalMatrix = transpose(inverse(Model)) on CPU and only once per set vertices, then doing
    // normalMatrix * mat3(boneTransform) -- mat3 removing the translation component, and only keeping rotation (already no scaling).
    // But maybe it's not a big deal, and maybe it's not true that scaling is not commonly used. Not sure where I heard that from.
    // vec3 Scale;
};

struct imported_animation
{
    u32 KeyCount;
    u32 ChannelCount;
    f32 *KeyTimes;
    imported_animation_key *Keys;

    simple_string AnimationName;
};

struct imported_model
{
    u32 MeshCount;
    u32 MaterialCount;
    u32 AnimationCount;
    
    imported_mesh *Meshes;
    imported_material *Materials;
    imported_armature *Armature;
    imported_animation *Animations;
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

internal inline mat4
Assimp_ConvertMat4F(aiMatrix4x4 AssimpMat)
{
    mat4 Result {};

    Result.E[0][0] = AssimpMat.a1; Result.E[0][1] = AssimpMat.b1; Result.E[0][2] = AssimpMat.c1; Result.E[0][3] = AssimpMat.d1;
    Result.E[1][0] = AssimpMat.a2; Result.E[1][1] = AssimpMat.b2; Result.E[1][2] = AssimpMat.c2; Result.E[1][3] = AssimpMat.d2;
    Result.E[2][0] = AssimpMat.a3; Result.E[2][1] = AssimpMat.b3; Result.E[2][2] = AssimpMat.c3; Result.E[2][3] = AssimpMat.d3;
    Result.E[3][0] = AssimpMat.a4; Result.E[3][1] = AssimpMat.b4; Result.E[3][2] = AssimpMat.c4; Result.E[3][3] = AssimpMat.d4;

    return Result;
}

internal void
Assimp_GetArmatureInfoRecursive(aiNode *Node, aiNode **Out_ArmatureNode, u32 *Out_BoneCount)
{
    if (*Out_ArmatureNode)
    {
        (*Out_BoneCount)++;
    }
    else if (CompareStrings(Node->mName.C_Str(), "Armature"))
    {
        *Out_ArmatureNode = Node;
    }

    for (u32 ChildIndex = 0;
         ChildIndex < Node->mNumChildren;
         ++ChildIndex)
    {
        Assimp_GetArmatureInfoRecursive(Node->mChildren[ChildIndex], Out_ArmatureNode, Out_BoneCount);
    }
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

    imported_model *Model = MemoryArena_PushStruct(AssetArena, imported_model);

    //
    // NOTE: Process material data
    //
    if (AssimpScene->mNumMaterials > 0)
    {
        simple_string TestSimpleString = SimpleString("Hello World!");
        simple_string ModelPath = GetDirectoryFromPath(Path);
        
        Model->MaterialCount = AssimpScene->mNumMaterials + 1; // Material Index 0 -> No Material
        Model->Materials = MemoryArena_PushArray(AssetArena, Model->MaterialCount, imported_material);
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

    //
    // NOTE: Process armature data and bone data stored on AssimpScene level
    //
    aiNode *ArmatureNode = 0;
    u32 BoneCount = 0;
    Assimp_GetArmatureInfoRecursive(AssimpScene->mRootNode, &ArmatureNode, &BoneCount);
    if (ArmatureNode && BoneCount > 0)
    {
        // TODO: See if this needs to handle more bones
        u32 MaxBoneCount = 256;
        Assert(BoneCount < MaxBoneCount);

        Model->Armature = MemoryArena_PushStruct(AssetArena, imported_armature);
        Model->Armature->BoneCount = BoneCount + 1;
        // TODO: Should I keep bones in a graph instead of converting to a flat list at this point?
        Model->Armature->Bones = MemoryArena_PushArray(AssetArena, Model->Armature->BoneCount, imported_bone);

        MemoryArena_Freeze(AssetArena);
        
        aiNode **NodeQueue = MemoryArena_PushArray(AssetArena, MaxBoneCount, aiNode *);
        u32 *ParentIDHelperQueue = MemoryArena_PushArray(AssetArena, MaxBoneCount, u32);
        
        u32 QueueStart = 0;
        u32 QueueEnd = 0;
        NodeQueue[QueueEnd] = ArmatureNode;
        ParentIDHelperQueue[QueueEnd] = 0;
        QueueEnd++;

        u32 CurrentBoneIndex = 0;
        
        while (QueueStart != QueueEnd)
        {
            aiNode *CurrentNode = NodeQueue[QueueStart];
            u32 CurrentParent = ParentIDHelperQueue[QueueStart];
            QueueStart++;

            imported_bone *CurrentBone = Model->Armature->Bones + CurrentBoneIndex;

            if (CurrentBoneIndex == 0)
            {
                CurrentBone->BoneName = SimpleString("DummyBone");
            }
            else
            {
                CurrentBone->BoneName = SimpleString(CurrentNode->mName.C_Str());
            }

            CurrentBone->ID = CurrentBoneIndex;
            CurrentBone->ParentID = CurrentParent;
            CurrentBone->TransformToParent = Assimp_ConvertMat4F(CurrentNode->mTransformation);

            for (u32 ChildIndex = 0;
                 ChildIndex < CurrentNode->mNumChildren;
                 ++ChildIndex)
            {
                NodeQueue[QueueEnd] = CurrentNode->mChildren[ChildIndex];
                ParentIDHelperQueue[QueueEnd] = CurrentBoneIndex;
                CurrentBone->ChildrenIDs[CurrentBone->ChildrenCount++] = QueueEnd;
                QueueEnd++;
            }

            CurrentBoneIndex++;
        }

        MemoryArena_Unfreeze(AssetArena);
    }
    else
    {
        Model->Armature = 0;
    }
    
    //
    // NOTE: Process data stored per mesh
    //
    Model->MeshCount = AssimpScene->mNumMeshes;
    Model->Meshes = MemoryArena_PushArray(AssetArena, Model->MeshCount, imported_mesh);
    for (u32 MeshIndex = 0;
         MeshIndex < Model->MeshCount;
         ++MeshIndex)
    {
        aiMesh *AssimpMesh = AssimpScene->mMeshes[MeshIndex];

        imported_mesh *Mesh = &Model->Meshes[MeshIndex];

        //
        // NOTE: Vertex data
        //
        Mesh->VertexCount = AssimpMesh->mNumVertices;
        Mesh->VertexPositions = MemoryArena_PushArray(AssetArena, Mesh->VertexCount, vec3);
        Mesh->VertexTangents = MemoryArena_PushArray(AssetArena, Mesh->VertexCount, vec3);
        Mesh->VertexBitangents = MemoryArena_PushArray(AssetArena, Mesh->VertexCount, vec3);
        Mesh->VertexNormals = MemoryArena_PushArray(AssetArena, Mesh->VertexCount, vec3);
        for (u32 VertexIndex = 0;
             VertexIndex < Mesh->VertexCount;
             ++VertexIndex)
        {
            Mesh->VertexPositions[VertexIndex] = Assimp_ConvertVec3F(AssimpMesh->mVertices[VertexIndex]);
            Mesh->VertexTangents[VertexIndex] = Assimp_ConvertVec3F(AssimpMesh->mTangents[VertexIndex]);
            Mesh->VertexBitangents[VertexIndex] = Assimp_ConvertVec3F(AssimpMesh->mBitangents[VertexIndex]);
            Mesh->VertexNormals[VertexIndex] = Assimp_ConvertVec3F(AssimpMesh->mNormals[VertexIndex]);
        }
        if (AssimpMesh->mTextureCoords[0])
        {
            Mesh->VertexUVs = MemoryArena_PushArray(AssetArena, Mesh->VertexCount, vec2);
            for (u32 VertexIndex = 0;
                 VertexIndex < Mesh->VertexCount;
                 ++VertexIndex)
            {
                Mesh->VertexUVs[VertexIndex] = Assimp_ConvertVec2F(AssimpMesh->mTextureCoords[0][VertexIndex]);
            }
        }
        else
        {
            Mesh->VertexUVs = 0;
        }
        if (AssimpMesh->mColors[0])
        {
            Mesh->VertexColors = MemoryArena_PushArray(AssetArena, Mesh->VertexCount, vec4);
            for (u32 VertexIndex = 0;
                 VertexIndex < Mesh->VertexCount;
                 ++VertexIndex)
            {
                Mesh->VertexColors[VertexIndex] = Assimp_ConvertColor4F(AssimpMesh->mColors[0][VertexIndex]);
            }
        }
        else
        {
            Mesh->VertexColors = 0;
        }

        //
        // NOTE: Index data
        //
        Mesh->IndexCount = AssimpMesh->mNumFaces * 3;
        Mesh->Indices = MemoryArena_PushArray(AssetArena, Mesh->IndexCount, i32);
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

        //
        // NOTE: Material data
        //
        if (Model->MaterialCount > 0)
        {
            Mesh->MaterialIndex = AssimpMesh->mMaterialIndex + 1;
        }
        else
        {
            Mesh->MaterialIndex = 0;
        }

        //
        // NOTE: Bone data
        //
        if (Model->Armature)
        {
            Mesh->VertexBoneData = MemoryArena_PushArray(AssetArena, Mesh->VertexCount, vertex_bone_data);
            
            for (u32 AssimpBoneIndex = 0;
                 AssimpBoneIndex < AssimpMesh->mNumBones;
                 ++AssimpBoneIndex)
            {
                aiBone *AssimpBone = AssimpMesh->mBones[AssimpBoneIndex];

                // NOTE: Find the bone with the corresponding name among the bones that are already saved in Model->Armature
                u32 BoneID = 0;
                for (u32 SearchBoneIndex = 0;
                     SearchBoneIndex < Model->Armature->BoneCount;
                     ++SearchBoneIndex)
                {
                    if (CompareStrings(Model->Armature->Bones[SearchBoneIndex].BoneName.D, AssimpBone->mName.C_Str()))
                    {
                        BoneID = SearchBoneIndex;
                        break;
                    }
                }

                Model->Armature->Bones[BoneID].InverseBindTransform = Assimp_ConvertMat4F(AssimpBone->mOffsetMatrix);

                for (u32 AssimpWeightIndex = 0;
                     AssimpWeightIndex < AssimpBone->mNumWeights;
                     ++AssimpWeightIndex)
                {
                    aiVertexWeight *AssimpWeight = AssimpBone->mWeights + AssimpWeightIndex;

                    if (AssimpWeight->mWeight > 0.0f)
                    {
                        vertex_bone_data *VertexBoneData = Mesh->VertexBoneData + AssimpWeight->mVertexId;

                        b32 FoundSlotForBone = false;
                        for (u32 SearchEmptySlotIndex = 0;
                             SearchEmptySlotIndex < MAX_BONES_PER_VERTEX;
                             ++SearchEmptySlotIndex)
                        {
                            if (VertexBoneData->BoneIDs[SearchEmptySlotIndex] == 0)
                            {
                                VertexBoneData->BoneIDs[SearchEmptySlotIndex] = BoneID;
                                VertexBoneData->BoneWeights[SearchEmptySlotIndex] = AssimpWeight->mWeight;
                                FoundSlotForBone = true;
                                break;
                            }
                        }
                        
                        Assert(FoundSlotForBone);
                    }
                }
            }
        }

        //
        // NOTE: Parse animations
        //
        Noop;
    }

    return Model;
}

#endif
