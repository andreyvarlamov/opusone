#include "opusone_common.h"
#include "opusone_platform.h"
#include "opusone.h"
#include "opusone_math.h"
#include "opusone_linmath.h"
#include "opusone_camera.h"
#include "opusone_assimp.h"
#include "opusone_render.h"

#include <cstdio>
#include <glad/glad.h>

void
ComputeTransformsForAnimation(animation_state *AnimationState, mat4 *BoneTransforms, u32 BoneTransformCount)
{
    imported_armature *Armature = AnimationState->Armature;
    imported_animation *Animation = AnimationState->Animation;
    Assert(Armature);
    Assert(Animation);
    Assert(Armature->BoneCount == BoneTransformCount);
    
    for (u32 BoneIndex = 1;
         BoneIndex < Armature->BoneCount;
         ++BoneIndex)
    {
        imported_bone *Bone = Armature->Bones + BoneIndex;

        mat4 *Transform = BoneTransforms + BoneIndex;
                            
        imported_animation_channel *Channel = Animation->Channels + BoneIndex;
        Assert(Channel->BoneID == BoneIndex);

        // NOTE: Can't just use a "no change" key, because animation is supposed
        // to include bone's transform to parent. If there are such cases, will
        // need to handle them specially
        Assert(Channel->PositionKeyCount != 0);
        Assert(Channel->RotationKeyCount != 0);
        Assert(Channel->ScaleKeyCount != 0);

        vec3 Position {};

        if (Channel->PositionKeyCount == 1)
        {
            Position = Channel->PositionKeys[0];
        }
        else if (Channel->PositionKeyCount > 1)
        {
            u32 PrevTimeIndex = 0;
            u32 TimeIndex = 0;
            for (;
                 TimeIndex < Channel->PositionKeyCount;
                 ++TimeIndex)
            {
                if (AnimationState->CurrentTicks <= Channel->PositionKeyTimes[TimeIndex])
                {
                    break;
                }
                PrevTimeIndex = TimeIndex;
            }
            Assert(TimeIndex < Channel->PositionKeyCount);
            // NOTE: If the current animation time is at exactly 0.0, both keys will be the same,
            // lerp will have no effect. No need to branch, because that should happen only rarely anyways.
                                
            vec3 PositionA = Channel->PositionKeys[PrevTimeIndex];
            vec3 PositionB = Channel->PositionKeys[TimeIndex];

            f32 T = (f32) ((AnimationState->CurrentTicks - Channel->PositionKeyTimes[PrevTimeIndex]) /
                           (Channel->PositionKeyTimes[TimeIndex] - Channel->PositionKeyTimes[PrevTimeIndex]));

            Position = Vec3Lerp(PositionA, PositionB, T);

#if 0
            if (ShouldLog && PrevTimeIndex == 0)
            {
                printf("[%s]-P-(%d)<%0.3f,%0.3f,%0.3f>:(%d)<%0.3f,%0.3f,%0.3f>[%0.3f]<%0.3f,%0.3f,%0.3f>\n",
                       Bone->BoneName.D,
                       PrevTimeIndex, PositionA.X, PositionA.Y, PositionA.Z,
                       TimeIndex, PositionB.X, PositionB.Y, PositionB.Z,
                       T, Position.X, Position.Y, Position.Z);
            }
#endif
        }
                            
        quat Rotation = QuatGetNeutral();

        if (Channel->RotationKeyCount == 1)
        {
            Rotation = Channel->RotationKeys[0];
        }
        else if (Channel->PositionKeyCount > 1)
        {
            u32 PrevTimeIndex = 0;
            u32 TimeIndex = 0;
            for (;
                 TimeIndex < Channel->PositionKeyCount;
                 ++TimeIndex)
            {
                if (AnimationState->CurrentTicks <= Channel->RotationKeyTimes[TimeIndex])
                {
                    break;
                }
                PrevTimeIndex = TimeIndex;
            }
            Assert(TimeIndex < Channel->RotationKeyCount);

            quat RotationA = Channel->RotationKeys[PrevTimeIndex];
            quat RotationB = Channel->RotationKeys[TimeIndex];

            f32 T = (f32) ((AnimationState->CurrentTicks - Channel->RotationKeyTimes[PrevTimeIndex]) /
                           (Channel->RotationKeyTimes[TimeIndex] - Channel->RotationKeyTimes[PrevTimeIndex]));

            Rotation = QuatSphericalLerp(RotationA, RotationB, T);

#if 0
            if (ShouldLog && PrevTimeIndex == 0)
            {
                printf("[%s]-R-(%d)<%0.3f,%0.3f,%0.3f,%0.3f>:(%d)<%0.3f,%0.3f,%0.3f,%0.3f>[%0.3f]<%0.3f,%0.3f,%0.3f,%0.3f>\n",
                       Bone->BoneName.D,
                       PrevTimeIndex, RotationA.W, RotationA.X, RotationA.Y, RotationA.Z,
                       TimeIndex, RotationB.W, RotationB.X, RotationB.Y, RotationB.Z,
                       T, Rotation.W, Rotation.X, Rotation.Y, Rotation.Z);
            }
#endif
        }
                            
        vec3 Scale { 1.0f, 1.0f, 1.0f };

        if (Channel->ScaleKeyCount == 1)
        {
            Scale = Channel->ScaleKeys[0];
        }
        else if (Channel->ScaleKeyCount > 1)
        {
            u32 PrevTimeIndex = 0;
            u32 TimeIndex = 0;
            for (;
                 TimeIndex < Channel->ScaleKeyCount;
                 ++TimeIndex)
            {
                if (AnimationState->CurrentTicks <= Channel->ScaleKeyTimes[TimeIndex])
                {
                    break;
                }
                PrevTimeIndex = TimeIndex;
            }
            Assert(TimeIndex < Channel->RotationKeyCount);

            vec3 ScaleA = Channel->ScaleKeys[PrevTimeIndex];
            vec3 ScaleB = Channel->ScaleKeys[TimeIndex];

            f32 T = (f32) ((AnimationState->CurrentTicks - Channel->ScaleKeyTimes[PrevTimeIndex]) /
                           (Channel->ScaleKeyTimes[TimeIndex] - Channel->ScaleKeyTimes[PrevTimeIndex]));

            Scale = Vec3Lerp(ScaleA, ScaleB, T);

#if 0
            if (ShouldLog && PrevTimeIndex == 0)
            {
                printf("[%s]-S-(%d)<%0.3f,%0.3f,%0.3f>:(%d)<%0.3f,%0.3f,%0.3f>[%0.3f]<%0.3f,%0.3f,%0.3f>\n",
                       Bone->BoneName.D,
                       PrevTimeIndex, ScaleA.X, ScaleA.Y, ScaleA.Z,
                       TimeIndex, ScaleB.X, ScaleB.Y, ScaleB.Z,
                       T, Scale.X, Scale.Y, Scale.Z);
            }
#endif
        }

        mat4 AnimationTransform = Mat4GetFullTransform(Position, Rotation, Scale);

        if (Bone->ParentID == 0)
        {
            *Transform = AnimationTransform;
        }
        else
        {
            Assert (Bone->ParentID < BoneIndex);
            *Transform = BoneTransforms[Bone->ParentID] * AnimationTransform;
        }
    }
}

void
GameUpdateAndRender(game_input *GameInput, game_memory *GameMemory, b32 *GameShouldQuit)
{
    game_state *GameState = (game_state *) GameMemory->Storage;

    //
    // NOTE: First frame/initialization
    //
    if (!GameMemory->IsInitialized)
    {
        GameMemory->IsInitialized = true;

        // TODO: Allocate more than game_state size? Better if I do hot-reloading in the future
        // NOTE: Not sure if nested arenas are a good idea, but I will go with it for now
        u8 *RootArenaBase = (u8 *) GameMemory->Storage + sizeof(game_state);
        size_t RootArenaSize = GameMemory->StorageSize - sizeof(game_state);
        GameState->RootArena = MemoryArena(RootArenaBase, RootArenaSize);

        GameState->WorldArena = MemoryArenaNested(&GameState->RootArena, Megabytes(4));
        GameState->RenderArena = MemoryArenaNested(&GameState->RootArena, Megabytes(4));
        GameState->AssetArena = MemoryArenaNested(&GameState->RootArena, Megabytes(16));

        u32 StaticShader = OpenGL_BuildShaderProgram("resources/shaders/StaticMesh.vs", "resources/shaders/Basic.fs");
        OpenGL_SetUniformInt(StaticShader, "DiffuseMap", 0, true);
        OpenGL_SetUniformInt(StaticShader, "SpecularMap", 1, false);
        OpenGL_SetUniformInt(StaticShader, "EmissionMap", 2, false);
        OpenGL_SetUniformInt(StaticShader, "NormalMap", 3, false);
        u32 SkinnedShader = OpenGL_BuildShaderProgram("resources/shaders/SkinnedMesh.vs", "resources/shaders/Basic.fs");
        OpenGL_SetUniformInt(SkinnedShader, "DiffuseMap", 0, true);
        OpenGL_SetUniformInt(SkinnedShader, "SpecularMap", 1, false);
        OpenGL_SetUniformInt(SkinnedShader, "EmissionMap", 2, false);
        OpenGL_SetUniformInt(SkinnedShader, "NormalMap", 3, false);
        u32 DebugDrawShader = OpenGL_BuildShaderProgram("resources/shaders/DebugDraw.vs", "resources/shaders/DebugDraw.fs");

        InitializeCamera(&GameState->Camera, vec3 { 0.0f, 1.7f, 5.0f }, 0.0f, 90.0f, 5.0f, CAMERA_CONTROL_MOUSE);

        //
        // NOTE: Load models using assimp
        //
        simple_string ModelPaths[] = {
            SimpleString("resources/models/box_room/BoxRoom.gltf"),
            SimpleString("resources/models/adam/adam_new.gltf"),
            SimpleString("resources/models/complex_animation_keys/AnimationStudy2c.gltf")
        };

        GameState->WorldObjectBlueprintCount = ArrayCount(ModelPaths) + 1;
        GameState->WorldObjectBlueprints = MemoryArena_PushArray(&GameState->WorldArena,
                                                                 GameState->WorldObjectBlueprintCount,
                                                                 world_object_blueprint);
        
        for (u32 BlueprintIndex = 1;
             BlueprintIndex < GameState->WorldObjectBlueprintCount;
             ++BlueprintIndex)
        {
            world_object_blueprint *Blueprint = GameState->WorldObjectBlueprints + BlueprintIndex;

            world_object_blueprint ZeroBlueprint {};
            *Blueprint = ZeroBlueprint;
            
            Blueprint->ImportedModel = Assimp_LoadModel(&GameState->AssetArena, ModelPaths[BlueprintIndex-1].D);
        }

        //
        // NOTE: Initialize render units
        //
        u32 MaterialsForStaticCount = 0;
        u32 MeshesForStaticCount = 0;
        u32 VerticesForStaticCount = 0;
        u32 IndicesForStaticCount = 0;
        u32 MaterialsForSkinnedCount = 0;
        u32 MeshesForSkinnedCount = 0;
        u32 VerticesForSkinnedCount = 0;
        u32 IndicesForSkinnedCount = 0;
        
        for (u32 BlueprintIndex = 1;
             BlueprintIndex < GameState->WorldObjectBlueprintCount;
             ++BlueprintIndex)
        {
            world_object_blueprint *Blueprint = GameState->WorldObjectBlueprints + BlueprintIndex;
            imported_model *ImportedModel = Blueprint->ImportedModel;

            u32 *MaterialCount;
            u32 *MeshCount;
            u32 *VertexCount;
            u32 *IndexCount;
            
            if (ImportedModel->Armature)
            {
                MaterialCount = &MaterialsForSkinnedCount;
                MeshCount = &MeshesForSkinnedCount;
                VertexCount = &VerticesForSkinnedCount;
                IndexCount = &IndicesForSkinnedCount;
            }
            else
            {
                MaterialCount = &MaterialsForStaticCount;
                MeshCount = &MeshesForStaticCount;
                VertexCount = &VerticesForStaticCount;
                IndexCount = &IndicesForStaticCount;
            }
            
            *MaterialCount += ImportedModel->MaterialCount - ((ImportedModel->MaterialCount == 0) ? 0 : 1);
            *MeshCount += ImportedModel->MeshCount;

            for (u32 MeshIndex = 0;
                 MeshIndex < ImportedModel->MeshCount;
                 ++MeshIndex)
            {
                imported_mesh *ImportedMesh = ImportedModel->Meshes + MeshIndex;

                *VertexCount += ImportedMesh->VertexCount;
                *IndexCount += ImportedMesh->IndexCount;
            }
        }

        if (VerticesForStaticCount > 0)
        {
            InitializeRenderUnit(&GameState->StaticRenderUnit, VERT_SPEC_STATIC_MESH,
                                 MaterialsForStaticCount, MeshesForStaticCount,
                                 VerticesForStaticCount, IndicesForStaticCount,
                                 false, StaticShader, &GameState->RenderArena);
        }

        if (VerticesForSkinnedCount > 0)
        {
            InitializeRenderUnit(&GameState->SkinnedRenderUnit, VERT_SPEC_SKINNED_MESH,
                                 MaterialsForSkinnedCount, MeshesForSkinnedCount,
                                 VerticesForSkinnedCount, IndicesForSkinnedCount,
                                 false, SkinnedShader, &GameState->RenderArena);
        }

        // Debug draw render unit
        {
            u32 MaxMarkers = 1024;
            u32 MaxVertices = 16384;
            u32 MaxIndices = 65536;
            
            InitializeRenderUnit(&GameState->DebugDrawRenderUnit, VERT_SPEC_DEBUG_DRAW,
                                 0, MaxMarkers, MaxVertices, MaxIndices, true, DebugDrawShader,
                                 &GameState->RenderArena);
        }

        //
        // NOTE: Imported data -> Rendering data
        //
        for (u32 BlueprintIndex = 1;
             BlueprintIndex < GameState->WorldObjectBlueprintCount;
             ++BlueprintIndex)
        {
            world_object_blueprint *Blueprint = GameState->WorldObjectBlueprints + BlueprintIndex;
            imported_model *ImportedModel = Blueprint->ImportedModel;

            render_unit *RenderUnit;

            if (ImportedModel->Armature)
            {
                RenderUnit = &GameState->SkinnedRenderUnit;
            }
            else
            {
                RenderUnit = &GameState->StaticRenderUnit;
            }

            for (u32 MaterialPerModelIndex = 1;
                 MaterialPerModelIndex < ImportedModel->MaterialCount;
                 ++MaterialPerModelIndex)
            {
                imported_material *ImportedMaterial = ImportedModel->Materials + MaterialPerModelIndex;
                render_data_material *Material = RenderUnit->Materials + RenderUnit->MaterialCount + (MaterialPerModelIndex - 1);
                *Material = {};
                
                for (u32 TexturePathIndex = 0;
                     TexturePathIndex < TEXTURE_TYPE_COUNT;
                     ++TexturePathIndex)
                {
                    simple_string Path = ImportedMaterial->TexturePaths[TexturePathIndex];
                    if (Path.Length > 0)
                    {
                        platform_load_image_result LoadImageResult = PlatformLoadImage(Path.D);

                        Material->TextureIDs[TexturePathIndex] =
                            OpenGL_LoadTexture(LoadImageResult.ImageData,
                                               LoadImageResult.Width, LoadImageResult.Height,
                                               LoadImageResult.Pitch, LoadImageResult.BytesPerPixel);

                        PlatformFreeImage(&LoadImageResult);
                    }
                }
            }

            for (u32 MeshIndex = 0;
                 MeshIndex < ImportedModel->MeshCount;
                 ++MeshIndex)
            {
                imported_mesh *ImportedMesh = ImportedModel->Meshes + MeshIndex;
                render_marker *Marker = RenderUnit->Markers + RenderUnit->MarkerCount + MeshIndex;
                *Marker = {};

                Marker->StateT = RENDER_STATE_MESH;
                Marker->BaseVertexIndex = RenderUnit->VertexCount;
                Marker->StartingIndex = RenderUnit->IndexCount;
                Marker->IndexCount = ImportedMesh->IndexCount;
                // MaterialID per model -> MaterialID per render unit
                Marker->StateD.Mesh.MaterialID = ((ImportedMesh->MaterialID == 0) ? 0 :
                                    (RenderUnit->MaterialCount + (ImportedMesh->MaterialID - 1)));

                void *AttribData[16] = {};
                u32 AttribCount = 0;
                
                switch (RenderUnit->VertSpecType)
                {
                    case VERT_SPEC_STATIC_MESH:
                    {
                        AttribData[AttribCount++] = ImportedMesh->VertexPositions;
                        AttribData[AttribCount++] = ImportedMesh->VertexTangents;
                        AttribData[AttribCount++] = ImportedMesh->VertexBitangents;
                        AttribData[AttribCount++] = ImportedMesh->VertexNormals;
                        AttribData[AttribCount++] = ImportedMesh->VertexColors;
                        AttribData[AttribCount++] = ImportedMesh->VertexUVs;
                    } break;

                    case VERT_SPEC_SKINNED_MESH:
                    {
                        AttribData[AttribCount++] = ImportedMesh->VertexPositions;
                        AttribData[AttribCount++] = ImportedMesh->VertexTangents;
                        AttribData[AttribCount++] = ImportedMesh->VertexBitangents;
                        AttribData[AttribCount++] = ImportedMesh->VertexNormals;
                        AttribData[AttribCount++] = ImportedMesh->VertexColors;
                        AttribData[AttribCount++] = ImportedMesh->VertexUVs;
                        AttribData[AttribCount++] = ImportedMesh->VertexBoneIDs;
                        AttribData[AttribCount++] = ImportedMesh->VertexBoneWeights;
                    } break;

                    default:
                    {
                        InvalidCodePath;
                    } break;
                }

                Assert(AttribCount <= ArrayCount(AttribData));
                SubVertexDataForRenderUnit(RenderUnit, AttribData, AttribCount, ImportedMesh->Indices,
                                           ImportedMesh->VertexCount, ImportedMesh->IndexCount);
            }

            Blueprint->RenderUnit = RenderUnit;
            Blueprint->BaseMeshID = RenderUnit->MarkerCount;
            Blueprint->MeshCount = ImportedModel->MeshCount;

            RenderUnit->MaterialCount += ImportedModel->MaterialCount - ((ImportedModel->MaterialCount == 0) ? 0 : 1);
            RenderUnit->MarkerCount += ImportedModel->MeshCount;
        }

        //
        // NOTE: OpenGL init state
        //
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        //
        // NOTE: Add instances of world object blueprints
        //
        world_object_instance Instances[] = {
            world_object_instance { 1, vec3 { 0.0f, 0.0f, 0.0f }, QuatGetNeutral(), vec3 { 1.0f, 1.0f, 1.0f }, 0 },
            world_object_instance { 1, vec3 { 20.0f, 0.0f, 0.0f }, QuatGetRotationAroundAxis(vec3 { 0.0f, 1.0f, 1.0f }, ToRadiansF(45.0f)), vec3 { 0.5f, 1.0f, 2.0f }, 0 },
            world_object_instance { 1, vec3 { 0.0f, 0.0f, -30.0f }, QuatGetRotationAroundAxis(vec3 { 1.0f, 1.0f, 1.0f }, ToRadiansF(160.0f)), vec3 { 1.0f, 1.0f, 5.0f }, 0 },

            world_object_instance { 2, vec3 { 0.0f, 0.0f, 0.0f }, QuatGetNeutral(), vec3 { 1.0f, 1.0f, 1.0f }, 0 },
            world_object_instance { 2, vec3 { -1.0f, 0.0f, 0.0f }, QuatGetNeutral(), vec3 { 1.0f, 1.0f, 1.0f }, 0 },
            world_object_instance { 2, vec3 { 1.0f, 0.0f, 0.0f }, QuatGetNeutral(), vec3 { 1.0f, 1.0f, 1.0f }, 0 },
            world_object_instance { 2, vec3 { 2.0f, 0.0f, 0.0f }, QuatGetNeutral(), vec3 { 1.0f, 1.0f, 1.0f }, 0 },

            world_object_instance { 3, vec3 { 0.0f, 0.0f, -3.0f }, QuatGetNeutral(), vec3 { 0.3f, 0.3f, 0.3f }, 0 },
        };

        GameState->WorldObjectInstanceCount = ArrayCount(Instances) + 1;
        GameState->WorldObjectInstances = MemoryArena_PushArray(&GameState->WorldArena,
                                                                GameState->WorldObjectInstanceCount,
                                                                world_object_instance);
        
        for (u32 InstanceIndex = 1;
             InstanceIndex < GameState->WorldObjectInstanceCount;
             ++InstanceIndex)
        {
            world_object_instance *Instance = GameState->WorldObjectInstances + InstanceIndex;
            *Instance = Instances[InstanceIndex-1];

            Assert(Instance->BlueprintID > 0);
            
            world_object_blueprint *Blueprint = GameState->WorldObjectBlueprints + Instance->BlueprintID;

            if (Blueprint->ImportedModel->Armature)
            {
                Assert(Blueprint->ImportedModel->Animations);
                Assert(Blueprint->ImportedModel->AnimationCount > 0);

                Instance->AnimationState = MemoryArena_PushStruct(&GameState->WorldArena, animation_state);

                Instance->AnimationState->Armature = Blueprint->ImportedModel->Armature;
                
                if (Instance->BlueprintID == 2)
                {
                    // NOTE: Adam: default animation - running
                    Instance->AnimationState->Animation = Blueprint->ImportedModel->Animations + 2;
                }
                else if (Instance->BlueprintID == 3)
                {
                    Instance->AnimationState->Animation = Blueprint->ImportedModel->Animations;
                }
                else
                {
                    // NOTE: All models with armature should have animation in animation state set
                    InvalidCodePath;
                }
                Instance->AnimationState->CurrentTicks = 0.0f;
            }

            render_unit *RenderUnit = Blueprint->RenderUnit;

            for (u32 MeshIndex = 0;
                 MeshIndex < Blueprint->MeshCount;
                 ++MeshIndex)
            {
                render_marker *Marker = RenderUnit->Markers + Blueprint->BaseMeshID + MeshIndex;
                Assert(Marker->StateT == RENDER_STATE_MESH);
                render_state_mesh *Mesh = &Marker->StateD.Mesh;

                b32 SlotFound = false;
                for (u32 SlotIndex = 0;
                     SlotIndex < MAX_INSTANCES_PER_MESH;
                     ++SlotIndex)
                {
                    if (Mesh->InstanceIDs[SlotIndex] == 0)
                    {
                        Mesh->InstanceIDs[SlotIndex] = InstanceIndex;
                        SlotFound = true;
                        break;
                    }
                }
                Assert(SlotFound);
            }
        }
    }

    //
    // NOTE: Misc controls
    //
    if (GameInput->CurrentKeyStates[SDL_SCANCODE_ESCAPE])
    {
        *GameShouldQuit = true;
    }

    //
    // NOTE: Camera
    //
    UpdateCameraForFrame(GameState, GameInput);

    //
    // NOTE: Game logic update
    //
    for (u32 InstanceIndex = 0;
         InstanceIndex < GameState->WorldObjectInstanceCount;
         ++InstanceIndex)
    {
        world_object_instance *Instance = GameState->WorldObjectInstances + InstanceIndex;

        if (Instance->AnimationState)
        {
            f32 DeltaTicks = (f32) (GameInput->DeltaTime * (Instance->AnimationState->Animation->TicksPerSecond));
            Instance->AnimationState->CurrentTicks += DeltaTicks;
            if (Instance->AnimationState->CurrentTicks >= Instance->AnimationState->Animation->TicksDuration)
            {
                Instance->AnimationState->CurrentTicks -= Instance->AnimationState->Animation->TicksDuration;
            }
        }
    }

    {
        vec3 BoxPosition = { -5.0f, 1.0f, 0.0f };
        vec3 BoxExtents = { 1.0f, 1.0f, 1.0f };

        for (u32 BoxIndex = 1;
             BoxIndex <= 5;
             ++BoxIndex)
        {
            vec3 BoxVertices[] = {
                BoxPosition + vec3 { -BoxExtents.X,  BoxExtents.Y,  BoxExtents.Z },
                BoxPosition + vec3 { -BoxExtents.X, -BoxExtents.Y,  BoxExtents.Z },
                BoxPosition + vec3 {  BoxExtents.X, -BoxExtents.Y,  BoxExtents.Z },
                BoxPosition + vec3 {  BoxExtents.X,  BoxExtents.Y,  BoxExtents.Z },
                BoxPosition + vec3 {  BoxExtents.X,  BoxExtents.Y, -BoxExtents.Z },
                BoxPosition + vec3 {  BoxExtents.X, -BoxExtents.Y, -BoxExtents.Z },
                BoxPosition + vec3 { -BoxExtents.X, -BoxExtents.Y, -BoxExtents.Z },
                BoxPosition + vec3 { -BoxExtents.X,  BoxExtents.Y, -BoxExtents.Z }
            };
            u32 VertexCount = ArrayCount(BoxVertices);

            vec3 BoxNormals[] = {
                VecNormalize(vec3 { -1.0f,  1.0f,  1.0f }),
                VecNormalize(vec3 { -1.0f, -1.0f,  1.0f }),
                VecNormalize(vec3 {  1.0f, -1.0f,  1.0f }),
                VecNormalize(vec3 {  1.0f,  1.0f,  1.0f }),
                VecNormalize(vec3 {  1.0f,  1.0f, -1.0f }),
                VecNormalize(vec3 {  1.0f, -1.0f, -1.0f }),
                VecNormalize(vec3 { -1.0f, -1.0f, -1.0f }),
                VecNormalize(vec3 { -1.0f,  1.0f, -1.0f })
            };

            vec4 BoxColors[8] = {};
            for (u32 I = 0; I < 8; ++I)
            {
                BoxColors[I] = vec4 { 1.0f, 1.0f, 0.0f, 1.0f };
            }

#if 0
            i32 BoxIndices[] = {
                0, 1, 3,  3, 1, 2, // front
                4, 5, 7,  7, 5, 6, // back
                7, 0, 4,  4, 0, 3, // top
                1, 6, 2,  2, 6, 5, // bottom
                7, 6, 0,  0, 6, 1, // left
                3, 2, 4,  4, 2, 5  // right
            };
#else
            i32 BoxIndices[] = {
                0, 1,  1, 2,  2, 3,  3, 0,
                4, 5,  5, 6,  6, 7,  7, 4,
                0, 7,  1, 6,  2, 5,  3, 4
            };
#endif
            u32 IndexCount = ArrayCount(BoxIndices);

            render_unit *RenderUnit = &GameState->DebugDrawRenderUnit;
            render_marker *Marker = RenderUnit->Markers + (RenderUnit->MarkerCount++);
            *Marker = {};
            Marker->StateT = RENDER_STATE_DEBUG;
            Marker->BaseVertexIndex = RenderUnit->VertexCount;
            Marker->StartingIndex = RenderUnit->IndexCount;
            Marker->IndexCount = IndexCount;
            Marker->StateD.Debug.IsWireframe = true;

            void *AttribData[16] = {};
            u32 AttribCount = 0;
            AttribData[AttribCount++] = BoxVertices;
            AttribData[AttribCount++] = BoxNormals;
            AttribData[AttribCount++] = BoxColors;
            Assert(AttribCount <= ArrayCount(AttribData));

            SubVertexDataForRenderUnit(RenderUnit, AttribData, AttribCount, BoxIndices, VertexCount, IndexCount);

            BoxPosition.X += 2.5f;
        }
    }
    
    //
    // NOTE: Render
    //
    glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    render_unit *RenderUnits[] = { &GameState->StaticRenderUnit, &GameState->SkinnedRenderUnit, &GameState->DebugDrawRenderUnit };
    
    u32 PreviousShaderID = 0;
    
    for (u32 RenderUnitIndex = 0;
         RenderUnitIndex < ArrayCount(RenderUnits);
         ++RenderUnitIndex)
    {
        render_unit *RenderUnit = RenderUnits[RenderUnitIndex];

        if (PreviousShaderID == 0 || RenderUnit->ShaderID != PreviousShaderID)
        {
            Assert(RenderUnit->ShaderID > 0);
            
            OpenGL_UseShader(RenderUnit->ShaderID);

            mat4 ProjectionMat = Mat4GetPerspecitveProjection(70.0f,
                                                              (f32) GameInput->ScreenWidth / (f32) GameInput->ScreenHeight,
                                                              0.1f, 1000.0f);
            OpenGL_SetUniformMat4F(RenderUnit->ShaderID, "Projection", (f32 *) &ProjectionMat, false);
            
            mat4 ViewMat = GetCameraViewMat(&GameState->Camera);
            OpenGL_SetUniformMat4F(RenderUnit->ShaderID, "View", (f32 *) &ViewMat, false);

            PreviousShaderID = RenderUnit->ShaderID;
        }
        
        glBindVertexArray(RenderUnit->VAO);

        u32 TestCounter = 0;

        u32 PreviousMaterialID = 0;
        for (u32 MarkerIndex = 0;
             MarkerIndex < RenderUnit->MarkerCount;
             ++MarkerIndex)
        {
            render_marker *Marker = RenderUnit->Markers + MarkerIndex;

            switch (Marker->StateT)
            {
                case RENDER_STATE_MESH:
                {
                    render_state_mesh *Mesh = &Marker->StateD.Mesh;
                    
                    if (PreviousMaterialID == 0 || Mesh->MaterialID != PreviousMaterialID)
                    {
                        PreviousMaterialID = Mesh->MaterialID;
                        if (Mesh->MaterialID > 0)
                        {
                            render_data_material *Material = RenderUnit->Materials + Mesh->MaterialID;
                            BindTexturesForMaterial(Material);
                        }
                    }

                    for (u32 InstanceSlotIndex = 0;
                         InstanceSlotIndex < MAX_INSTANCES_PER_MESH;
                         ++InstanceSlotIndex)
                    {
                        u32 InstanceID = Mesh->InstanceIDs[InstanceSlotIndex];
                        if (InstanceID > 0)
                        {
                            // b32 ShouldLog = (InstanceID == 5);
                            world_object_instance *Instance = GameState->WorldObjectInstances + InstanceID;

                            mat4 ModelTransform = Mat4GetFullTransform(Instance->Position, Instance->Rotation, Instance->Scale);

                            if (Instance->AnimationState)
                            {
                                imported_armature *Armature = Instance->AnimationState->Armature;
                                u32 BoneCount = Instance->AnimationState->Armature->BoneCount;
                        
                                MemoryArena_Freeze(&GameState->RenderArena);
                                mat4 *BoneTransforms = MemoryArena_PushArray(&GameState->RenderArena,
                                                                             BoneCount,
                                                                             mat4);
                                                                     
                                ComputeTransformsForAnimation(Instance->AnimationState, BoneTransforms, BoneCount);
                        
                                for (u32 BoneIndex = 0;
                                     BoneIndex < BoneCount;
                                     ++BoneIndex)
                                {
                                    mat4 BoneTransform;
                            
                                    if (BoneIndex == 0)
                                    {
                                        BoneTransform = Mat4Identity();
                                    }
                                    else
                                    {
                                        BoneTransform = BoneTransforms[BoneIndex] * Armature->Bones[BoneIndex].InverseBindTransform;
                                    }

                                    simple_string UniformName;
                                    sprintf_s(UniformName.D, "BoneTransforms[%d]", BoneIndex);
                                    OpenGL_SetUniformMat4F(RenderUnit->ShaderID, UniformName.D, (f32 *) &BoneTransform, false);
                                }
                        
                                MemoryArena_Unfreeze(&GameState->RenderArena);
                            }
                            
                            OpenGL_SetUniformMat4F(RenderUnit->ShaderID, "Model", (f32 *) &ModelTransform, false);
                            // TODO: Should I treat indices as unsigned everywhere?
                            // TODO: Instanced draw?
                            glDrawElementsBaseVertex(GL_TRIANGLES,
                                                     Marker->IndexCount,
                                                     GL_UNSIGNED_INT,
                                                     (void *) (Marker->StartingIndex * sizeof(i32)),
                                                     Marker->BaseVertexIndex);
                        }
                    }
                } break; // case RENDER_STATE_MESH

                case RENDER_STATE_DEBUG:
                {
                    render_state_debug *DebugMarker = &Marker->StateD.Debug;

                    if (DebugMarker->IsWireframe)
                    {
                        // glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                    }
                    
                    glDrawElementsBaseVertex(GL_LINES,
                                             Marker->IndexCount,
                                             GL_UNSIGNED_INT,
                                             (void *) (Marker->StartingIndex * sizeof(i32)),
                                             Marker->BaseVertexIndex);

                    if (DebugMarker->IsWireframe)
                    {
                        // glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
                    }
                    
                } break; // case RENDER_STATE_DEBUG

                default:
                {
                    InvalidCodePath;
                } break;
            }
        } // for loop processing render unit marks

        if (RenderUnit->IsImmediate)
        {
            RenderUnit->VertexCount = 0;
            RenderUnit->IndexCount = 0;
            RenderUnit->MaterialCount = 1;
            RenderUnit->MarkerCount = 0;
        }
    }
}

#include "opusone_camera.cpp"
#include "opusone_assimp.cpp"
#include "opusone_render.cpp"
