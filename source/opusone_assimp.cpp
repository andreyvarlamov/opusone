#include "opusone_assimp.h"

#include "opusone_common.h"
#include "opusone_linmath.h"

#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

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

internal inline quat
Assimp_ConvertQuatF(aiQuaternion AssimpQuat)
{
    quat Quat {};
    
    Quat.W = AssimpQuat.w;
    Quat.X = AssimpQuat.x;
    Quat.Y = AssimpQuat.y;
    Quat.Z = AssimpQuat.z;
    
    return Quat;
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
Assimp_GetArmatureInfoRecursive(aiNode *Node, aiNode **Out_ArmatureNode, u32 *Out_BoneCount, b32 ArmatureFound)
{
    if (ArmatureFound)
    {
        (*Out_BoneCount)++;
    }
    else if (CompareStrings(Node->mName.C_Str(), "Armature"))
    {
        ArmatureFound = true;
        *Out_ArmatureNode = Node;
    }

    for (u32 ChildIndex = 0;
         ChildIndex < Node->mNumChildren;
         ++ChildIndex)
    {
        Assimp_GetArmatureInfoRecursive(Node->mChildren[ChildIndex], Out_ArmatureNode, Out_BoneCount, ArmatureFound);
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

    imported_model ZeroModel {};
    *Model = ZeroModel;

    //
    // NOTE: Process material data
    //
    if (AssimpScene->mNumMaterials > 0)
    {
        simple_string ModelPath = GetDirectoryFromPath(Path);
        
        Model->MaterialCount = AssimpScene->mNumMaterials + 1; // Material Index 0 -> No Material
        Model->Materials = MemoryArena_PushArray(AssetArena, Model->MaterialCount, imported_material);

        imported_material ZeroMaterial {};
        Model->Materials[0] = ZeroMaterial;
        
        for (u32 MaterialIndex = 1;
             MaterialIndex < Model->MaterialCount;
             ++MaterialIndex)
        {
            aiMaterial *AssimpMaterial = AssimpScene->mMaterials[MaterialIndex-1];
            imported_material *Material = Model->Materials + MaterialIndex;

            *Material = ZeroMaterial;

            aiTextureType TextureTypes[] = { 
                aiTextureType_DIFFUSE,
                aiTextureType_DIFFUSE_ROUGHNESS, // used for specular
                aiTextureType_EMISSIVE,
                aiTextureType_NORMALS
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

    //
    // NOTE: Process armature data and bone data stored on AssimpScene level
    //
    aiNode *ArmatureNode = 0;
    u32 BoneCount = 0;
    Assimp_GetArmatureInfoRecursive(AssimpScene->mRootNode, &ArmatureNode, &BoneCount, false);
    if (ArmatureNode && BoneCount > 0)
    {
        // TODO: See if this needs to handle more bones
        u32 MaxBoneCount = MAX_BONES_PER_MODEL;
        Assert(BoneCount < MaxBoneCount);

        Model->Armature = MemoryArena_PushStruct(AssetArena, imported_armature);
        Model->Armature->BoneCount = BoneCount + 1;
        // TODO: Should I keep bones in a graph instead of converting to a flat list at this point?
        Model->Armature->Bones = MemoryArena_PushArray(AssetArena, Model->Armature->BoneCount, imported_bone);
        imported_bone ZeroBone {};
        for (u32 BoneIndex = 0;
             BoneIndex < Model->Armature->BoneCount;
             ++BoneIndex)
        {
            Model->Armature->Bones[BoneIndex] = ZeroBone;
        }

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

            Assert(CurrentBoneIndex < Model->Armature->BoneCount);
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
        Assert(CurrentBoneIndex == Model->Armature->BoneCount);

        MemoryArena_Unfreeze(AssetArena);
    }
    
    //
    // NOTE: Process data stored per mesh
    //
    Model->MeshCount = AssimpScene->mNumMeshes;
    Model->Meshes = MemoryArena_PushArray(AssetArena, Model->MeshCount, imported_mesh);
    Assert(Model->MeshCount > 0); // TODO: Handle mesh count 0 for shipping code -> just abort loading the model
    for (u32 MeshIndex = 0;
         MeshIndex < Model->MeshCount;
         ++MeshIndex)
    {
        aiMesh *AssimpMesh = AssimpScene->mMeshes[MeshIndex];
        imported_mesh *Mesh = &Model->Meshes[MeshIndex];

        imported_mesh ZeroMesh {};
        *Mesh = ZeroMesh;

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
            Mesh->MaterialID = AssimpMesh->mMaterialIndex + 1;
        }

        //
        // NOTE: Bone data
        //
        if (Model->Armature)
        {
            Mesh->VertexBoneIDs = MemoryArena_PushArray(AssetArena, Mesh->VertexCount, vert_bone_ids);
            Mesh->VertexBoneWeights = MemoryArena_PushArray(AssetArena, Mesh->VertexCount, vert_bone_weights);
            vert_bone_ids ZeroBoneIDs {};
            vert_bone_weights ZeroBoneWeights {};
            for (u32 VertexIndex = 0;
                 VertexIndex < Mesh->VertexCount;
                 ++VertexIndex)
            {
                Mesh->VertexBoneIDs[VertexIndex] = ZeroBoneIDs;
                Mesh->VertexBoneWeights[VertexIndex] = ZeroBoneWeights;
            }
            
            for (u32 AssimpBoneIndex = 0;
                 AssimpBoneIndex < AssimpMesh->mNumBones;
                 ++AssimpBoneIndex)
            {
                aiBone *AssimpBone = AssimpMesh->mBones[AssimpBoneIndex];

                // NOTE: Find the bone with the corresponding name among the bones that are already saved in Model->Armature
                u32 BoneID = 0;
                b32 BoneFound = false;
                for (u32 SearchBoneIndex = 0;
                     SearchBoneIndex < Model->Armature->BoneCount;
                     ++SearchBoneIndex)
                {
                    if (CompareStrings(Model->Armature->Bones[SearchBoneIndex].BoneName.D, AssimpBone->mName.C_Str()))
                    {
                        BoneID = SearchBoneIndex;
                        BoneFound = true;
                        break;
                    }
                }
                Assert(BoneFound);

                Model->Armature->Bones[BoneID].InverseBindTransform = Assimp_ConvertMat4F(AssimpBone->mOffsetMatrix);

                for (u32 AssimpWeightIndex = 0;
                     AssimpWeightIndex < AssimpBone->mNumWeights;
                     ++AssimpWeightIndex)
                {
                    aiVertexWeight *AssimpWeight = AssimpBone->mWeights + AssimpWeightIndex;

                    if (AssimpWeight->mWeight > 0.0f)
                    {
                        vert_bone_ids *VertexBoneIDs = Mesh->VertexBoneIDs + AssimpWeight->mVertexId;
                        vert_bone_weights *VertexBoneWeights = Mesh->VertexBoneWeights + AssimpWeight->mVertexId;

                        b32 FoundSlotForBone = false;
                        for (u32 SearchEmptySlotIndex = 0;
                             SearchEmptySlotIndex < MAX_BONES_PER_VERTEX;
                             ++SearchEmptySlotIndex)
                        {
                            if (VertexBoneIDs->D[SearchEmptySlotIndex] == 0)
                            {
                                VertexBoneIDs->D[SearchEmptySlotIndex] = BoneID;
                                VertexBoneWeights->D[SearchEmptySlotIndex] = AssimpWeight->mWeight;
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
        if (Model->Armature && (AssimpScene->mNumAnimations > 0))
        {
            Model->AnimationCount = AssimpScene->mNumAnimations;
            Model->Animations = MemoryArena_PushArray(AssetArena, Model->AnimationCount, imported_animation);
            for (u32 AnimationIndex = 0;
                 AnimationIndex < Model->AnimationCount;
                 ++AnimationIndex)
            {
                aiAnimation *AssimpAnimation = AssimpScene->mAnimations[AnimationIndex];
                imported_animation *Animation = Model->Animations + AnimationIndex;
                imported_animation ZeroAnimation {};
                *Animation = ZeroAnimation;

                Animation->ChannelCount = AssimpAnimation->mNumChannels + 1;
                Animation->Channels = MemoryArena_PushArray(AssetArena, Animation->ChannelCount, imported_animation_channel);
                Animation->TicksDuration = AssimpAnimation->mDuration;
                Animation->TicksPerSecond = AssimpAnimation->mTicksPerSecond;
                Animation->AnimationName = SimpleString(AssimpAnimation->mName.C_Str());

                imported_animation_channel ZeroChannel {};
                Animation->Channels[0] = ZeroChannel;
                
                for (u32 ChannelIndex = 1;
                     ChannelIndex < Animation->ChannelCount;
                     ++ChannelIndex)
                {
                    aiNodeAnim *AssimpChannel = AssimpAnimation->mChannels[ChannelIndex-1];

                    u32 BoneID = 0;
                    for (u32 BoneSearchIndex = 1;
                         BoneSearchIndex < Model->Armature->BoneCount;
                         ++BoneSearchIndex)
                    {
                        if (CompareStrings(Model->Armature->Bones[BoneSearchIndex].BoneName.D, AssimpChannel->mNodeName.C_Str()))
                        {
                            BoneID = BoneSearchIndex;
                            break;
                        }
                    }
                    Assert(BoneID > 0);

                    imported_animation_channel *Channel = Animation->Channels + BoneID;

                    Channel->BoneID = BoneID;
                    
                    Channel->PositionKeyCount = AssimpChannel->mNumPositionKeys;
                    Channel->PositionKeys = MemoryArena_PushArray(AssetArena, Channel->PositionKeyCount, vec3);
                    Channel->PositionKeyTimes = MemoryArena_PushArray(AssetArena, Channel->PositionKeyCount, f64);
                    for (u32 PositionKeyIndex = 0;
                         PositionKeyIndex < Channel->PositionKeyCount;
                         ++PositionKeyIndex)
                    {
                        aiVectorKey *AssimpPositionKey = AssimpChannel->mPositionKeys + PositionKeyIndex;
                        f64 *PositionKeyTime = Channel->PositionKeyTimes + PositionKeyIndex;
                        vec3 *PositionKey = Channel->PositionKeys + PositionKeyIndex;

                        *PositionKeyTime = AssimpPositionKey->mTime;
                        *PositionKey = Assimp_ConvertVec3F(AssimpPositionKey->mValue);
                    }

                    Channel->RotationKeyCount = AssimpChannel->mNumRotationKeys;
                    Channel->RotationKeys = MemoryArena_PushArray(AssetArena, Channel->RotationKeyCount, quat);
                    Channel->RotationKeyTimes = MemoryArena_PushArray(AssetArena, Channel->RotationKeyCount, f64);
                    for (u32 RotationKeyIndex = 0;
                         RotationKeyIndex < Channel->RotationKeyCount;
                         ++RotationKeyIndex)
                    {
                        aiQuatKey *AssimpRotationKey = AssimpChannel->mRotationKeys + RotationKeyIndex;
                        f64 *RotationKeyTime = Channel->RotationKeyTimes + RotationKeyIndex;
                        quat *RotationKey = Channel->RotationKeys + RotationKeyIndex;

                        *RotationKeyTime = AssimpRotationKey->mTime;
                        *RotationKey = Assimp_ConvertQuatF(AssimpRotationKey->mValue);
                    }

                    Channel->ScaleKeyCount = AssimpChannel->mNumScalingKeys;
                    Channel->ScaleKeys = MemoryArena_PushArray(AssetArena, Channel->ScaleKeyCount, vec3);
                    Channel->ScaleKeyTimes = MemoryArena_PushArray(AssetArena, Channel->ScaleKeyCount, f64);
                    for (u32 ScaleKeyIndex = 0;
                         ScaleKeyIndex < Channel->ScaleKeyCount;
                         ++ScaleKeyIndex)
                    {
                        aiVectorKey *AssimpScaleKey = AssimpChannel->mScalingKeys + ScaleKeyIndex;
                        f64 *ScaleKeyTime = Channel->ScaleKeyTimes + ScaleKeyIndex;
                        vec3 *ScaleKey = Channel->ScaleKeys + ScaleKeyIndex;

                        *ScaleKeyTime = AssimpScaleKey->mTime;
                        *ScaleKey = Assimp_ConvertVec3F(AssimpScaleKey->mValue);
                    }
                }
            }
        }

        Noop;
    }

    return Model;
}
