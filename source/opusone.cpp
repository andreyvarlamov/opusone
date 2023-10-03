#include "opusone_common.h"
#include "opusone_platform.h"
#include "opusone.h"
#include "opusone_math.h"
#include "opusone_linmath.h"
#include "opusone_camera.h"
#include "opusone_assimp.h"
#include "opusone_render.h"
#include "opusone_animation.h"
#include "opusone_immtext.h"

#include <cstdio>
#include <glad/glad.h>

#include "opusone_debug_draw.cpp"
#include "opusone_geometry.cpp"

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
        GameState->TransientArena = MemoryArenaNested(&GameState->RootArena, Megabytes(4));

        GameState->ContrailOne = ImmText_LoadFont(&GameState->AssetArena, "resources/fonts/ContrailOne-Regular.ttf", 36);
        // GameState->MajorMono = ImmText_LoadFont(&GameState->AssetArena, "resources/fonts/MajorMonoDisplay-Regular.ttf", 72);

        u32 StaticShader = OpenGL_BuildShaderProgram("resources/shaders/StaticMesh.vs", "resources/shaders/Basic.fs");
        OpenGL_SetUniformInt(StaticShader, "DiffuseMap", 0, true);
        OpenGL_SetUniformInt(StaticShader, "SpecularMap", 1, false);
        OpenGL_SetUniformInt(StaticShader, "EmissionMap", 2, false);
        OpenGL_SetUniformInt(StaticShader, "NormalMap", 3, false);
        vec3 LightDirection = VecNormalize(Vec3(-1.0f, -1.0f, -1.0f));
        OpenGL_SetUniformVec3F(StaticShader, "LightDirection", (f32 *) &LightDirection, false);
        u32 SkinnedShader = OpenGL_BuildShaderProgram("resources/shaders/SkinnedMesh.vs", "resources/shaders/Basic.fs");
        OpenGL_SetUniformInt(SkinnedShader, "DiffuseMap", 0, true);
        OpenGL_SetUniformInt(SkinnedShader, "SpecularMap", 1, false);
        OpenGL_SetUniformInt(SkinnedShader, "EmissionMap", 2, false);
        OpenGL_SetUniformInt(SkinnedShader, "NormalMap", 3, false);
        OpenGL_SetUniformVec3F(SkinnedShader, "LightDirection", (f32 *) &LightDirection, false);
        u32 DebugDrawShader = OpenGL_BuildShaderProgram("resources/shaders/DebugDraw.vs", "resources/shaders/DebugDraw.fs");
        u32 ImmTextShader = OpenGL_BuildShaderProgram("resources/shaders/ImmText.vs", "resources/shaders/ImmText.fs");
        OpenGL_SetUniformInt(SkinnedShader, "FontAtlas", 0, true);

        InitializeCamera(&GameState->Camera, vec3 { 0.0f, 1.7f, 10.0f }, 0.0f, 75.0f, 5.0f);
        GameState->DebugCollisions = true;

        //
        // NOTE: Load models using assimp
        //
        simple_string ModelPaths[] = {
            SimpleString("resources/models/box_room/BoxRoom.gltf"),
            SimpleString("resources/models/adam/adam_new.gltf"),
            SimpleString("resources/models/complex_animation_keys/AnimationStudy2c.gltf"),
            SimpleString("resources/models/container/Container.gltf")
        };

        collision_type BlueprintCollisionTypes[] = {
            COLLISION_TYPE_MESH,
            COLLISION_TYPE_BV_AABB,
            COLLISION_TYPE_NONE,
            COLLISION_TYPE_MESH
        };

        i32 ModelCount = ArrayCount(ModelPaths);
        Assert(ModelCount == ArrayCount(BlueprintCollisionTypes));

        GameState->WorldObjectBlueprintCount = ModelCount + 1;
        GameState->WorldObjectBlueprints = MemoryArena_PushArray(&GameState->WorldArena,
                                                                 GameState->WorldObjectBlueprintCount,
                                                                 world_object_blueprint);
        
        for (u32 BlueprintIndex = 1;
             BlueprintIndex < GameState->WorldObjectBlueprintCount;
             ++BlueprintIndex)
        {
            world_object_blueprint *Blueprint = GameState->WorldObjectBlueprints + BlueprintIndex;
            *Blueprint = {};
            
            Blueprint->ImportedModel = Assimp_LoadModel(&GameState->AssetArena, ModelPaths[BlueprintIndex-1].D);
            Blueprint->CollisionType = BlueprintCollisionTypes[BlueprintIndex-1];
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
                                 false, false, StaticShader, &GameState->RenderArena);
        }

        if (VerticesForSkinnedCount > 0)
        {
            InitializeRenderUnit(&GameState->SkinnedRenderUnit, VERT_SPEC_SKINNED_MESH,
                                 MaterialsForSkinnedCount, MeshesForSkinnedCount,
                                 VerticesForSkinnedCount, IndicesForSkinnedCount,
                                 false, false, SkinnedShader, &GameState->RenderArena);
        }

        // Debug draw render unit
        {
            u32 MaxMarkers = 1024 * 3;
            u32 MaxVertices = 16384 * 3;
            u32 MaxIndices = 32768 * 3;
            
            InitializeRenderUnit(&GameState->DebugDrawRenderUnit, VERT_SPEC_DEBUG_DRAW,
                                 0, MaxMarkers, MaxVertices, MaxIndices, true, false, DebugDrawShader,
                                 &GameState->RenderArena);
        }

        // ImmText render unit
        {
            u32 MaxMarkers = 1024;
            u32 MaxVertices = 16384;
            u32 MaxIndices = 32768;
            
            InitializeRenderUnit(&GameState->ImmTextRenderUnit, VERT_SPEC_IMM_TEXT,
                                 0, MaxMarkers, MaxVertices, MaxIndices, true, true, ImmTextShader,
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
                        platform_image LoadImageResult = Platform_LoadImage(Path.D);

                        Material->TextureIDs[TexturePathIndex] =
                            OpenGL_LoadTexture(LoadImageResult.ImageData,
                                               LoadImageResult.Width, LoadImageResult.Height,
                                               LoadImageResult.Pitch, LoadImageResult.BytesPerPixel);

                        Platform_FreeImage(&LoadImageResult);
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

            switch (Blueprint->CollisionType)
            {
                case COLLISION_TYPE_NONE:
                case COLLISION_TYPE_MESH:
                {
                    Blueprint->BoundingVolume = 0;
                } break;
                
                case COLLISION_TYPE_BV_AABB:
                {
                    // TODO: Handle properly for different models
                    f32 AdamHeight = 1.8412f;
                    f32 AdamHalfHeight = AdamHeight * 0.5f;

                    collision_geometry *Collision = MemoryArena_PushStruct(&GameState->WorldArena, collision_geometry);
                    Collision->AABB = {};
                    Collision->AABB.Center = Vec3(0, AdamHalfHeight, 0);
                    Collision->AABB.Extents = Vec3 (0.3f, AdamHalfHeight, 0.3f);
                    Blueprint->BoundingVolume = Collision;
                } break;
                
                default:
                {
                    InvalidCodePath;
                } break;
            }

            Blueprint->RenderUnit = RenderUnit;
            Blueprint->BaseMeshID = RenderUnit->MarkerCount;
            Blueprint->MeshCount = ImportedModel->MeshCount;

            RenderUnit->MaterialCount += ImportedModel->MaterialCount - ((ImportedModel->MaterialCount == 0) ? 0 : 1);
            RenderUnit->MarkerCount += ImportedModel->MeshCount;
        }

        ImmText_InitializeQuickDraw(GameState->ContrailOne,
                                    5, 5, 2560, 1440,
                                    Vec3(1), Vec3(),
                                    &GameState->ImmTextRenderUnit, &GameState->RenderArena);

        //
        // NOTE: OpenGL init state
        //
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_LINE_SMOOTH);

        //
        // NOTE: Add instances of world object blueprints
        //
        world_object_instance Instances[] = {
            world_object_instance { 1, Vec3(0.0f, 0.0f, 0.0f), Quat(), Vec3(1.0f, 1.0f, 1.0f), 0 },
            world_object_instance { 1, Vec3(20.0f,0.0f, 0.0f), Quat(Vec3(0.0f, 1.0f, 1.0f), ToRadiansF(45.0f)), Vec3(0.5f, 1.0f, 2.0f), 0 },
            world_object_instance { 1, Vec3(0.0f,0.0f,-30.0f), Quat(Vec3(1.0f, 1.0f,1.0f), ToRadiansF(160.0f)), Vec3(1.0f, 1.0f, 5.0f), 0 },
            world_object_instance { 2, Vec3(0.0f, 0.0f, 0.0f), Quat(), Vec3(1.0f, 1.0f, 1.0f), 0 },
            world_object_instance { 2, Vec3(-1.0f, 0.0f, 0.0f), Quat(), Vec3(1.0f, 1.0f, 1.0f), 0 },
            world_object_instance { 2, Vec3(1.0f, 0.0f, 0.0f), Quat(), Vec3(1.0f, 1.0f, 1.0f), 0 },
            world_object_instance { 2, Vec3(2.0f, 0.0f, 0.0f), Quat(), Vec3(1.0f, 1.0f, 1.0f), 0 },
            world_object_instance { 3, Vec3(0.0f, 0.0f, -3.0f), Quat(), Vec3(0.3f, 0.3f, 0.3f), 0 },
            world_object_instance { 4, Vec3(-3.0f, 0.0f, -3.0f), Quat(Vec3(0,1,0), ToRadiansF(45.0f)), Vec3(1.0f, 1.0f, 1.0f), 0 },
            world_object_instance { 2, Vec3(0.0f, 0.1f, 5.0f), Quat(Vec3(0,1,0), ToRadiansF(180.0f)), Vec3(1.0f, 1.0f, 1.0f), 0 },
            world_object_instance { 4, Vec3(-3.0f, 0.0f, 3.0f), Quat(), Vec3(1.0f, 1.0f, 1.0f), 0 },
        };


        GameState->WorldObjectInstanceCount = ArrayCount(Instances) + 1;
        GameState->WorldObjectInstances = MemoryArena_PushArray(&GameState->WorldArena,
                                                                GameState->WorldObjectInstanceCount,
                                                                world_object_instance);

        GameState->PlayerCameraYOffset = 1.7f;
        GameState->PlayerWorldInstanceID = 10;
        
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

                if (InstanceIndex == GameState->PlayerWorldInstanceID)
                {
                    Instance->AnimationState = 0;
                }
                else
                {
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
    b32 IgnoreCollisions = GameInput->CurrentKeyStates[SDL_SCANCODE_LALT];
    if (GameInput->CurrentKeyStates[SDL_SCANCODE_F1] && !GameInput->KeyWasDown[SDL_SCANCODE_F1])
    {
        GameState->MouseControlledTemp = !GameState->MouseControlledTemp;
        Platform_SetRelativeMouse(GameState->MouseControlledTemp);
        GameInput->KeyWasDown[SDL_SCANCODE_F1] = true;
    }
    else if (!GameInput->CurrentKeyStates[SDL_SCANCODE_F1])
    {
        GameInput->KeyWasDown[SDL_SCANCODE_F1] = false;
    }
    if (GameInput->CurrentKeyStates[SDL_SCANCODE_F2] && !GameInput->KeyWasDown[SDL_SCANCODE_F2])
    {
        GameState->ForceFirstPersonTemp = !GameState->ForceFirstPersonTemp;
        GameInput->KeyWasDown[SDL_SCANCODE_F2] = true;
    }
    else if (!GameInput->CurrentKeyStates[SDL_SCANCODE_F2])
    {
        GameInput->KeyWasDown[SDL_SCANCODE_F2] = false;
    }
    if (GameInput->CurrentKeyStates[SDL_SCANCODE_F3] && !GameInput->KeyWasDown[SDL_SCANCODE_F3])
    {
        GameState->DebugCollisions = !GameState->DebugCollisions;
        GameInput->KeyWasDown[SDL_SCANCODE_F3] = true;
    }
    else if (!GameInput->CurrentKeyStates[SDL_SCANCODE_F3])
    {
        GameInput->KeyWasDown[SDL_SCANCODE_F3] = false;
    }

    //
    // NOTE: Game logic update
    //
    // GameState->WorldObjectInstances[9].Rotation *= Quat(Vec3(0.0f, 1.0f, 0.0f), ToRadiansF(30.0f) * GameInput->DeltaTime);

    world_object_instance *PlayerInstance = GameState->WorldObjectInstances + GameState->PlayerWorldInstanceID;
    world_object_blueprint *PlayerBlueprint = GameState->WorldObjectBlueprints + PlayerInstance->BlueprintID;
    
    f32 KeyboardLookSensitivity = 150.0f;
    f32 KeyboardDollySensitivity = 5.0f;
    f32 MouseLookSensitivity = 0.035f;
    
    f32 CameraDeltaTheta = 0.0f;
    f32 CameraDeltaPhi = 0.0f;

    if (GameInput->CurrentKeyStates[SDL_SCANCODE_Q] ||
        GameInput->CurrentKeyStates[SDL_SCANCODE_E] ||
        GameInput->CurrentKeyStates[SDL_SCANCODE_Z] ||
        GameInput->CurrentKeyStates[SDL_SCANCODE_X])
    {
        if (GameInput->CurrentKeyStates[SDL_SCANCODE_Q])
        {
            CameraDeltaTheta += 1.0f;
        }
        if (GameInput->CurrentKeyStates[SDL_SCANCODE_E])
        {
            CameraDeltaTheta -= 1.0f;
        }
        if (GameInput->CurrentKeyStates[SDL_SCANCODE_Z])
        {
            CameraDeltaPhi += 1.0f;
        }
        if (GameInput->CurrentKeyStates[SDL_SCANCODE_X])
        {
            CameraDeltaPhi -= 1.0f;
        }
        CameraDeltaTheta *= KeyboardLookSensitivity * GameInput->DeltaTime;
        CameraDeltaPhi *= KeyboardLookSensitivity * GameInput->DeltaTime;
    }

    if (GameState->MouseControlledTemp)
    {
        CameraDeltaTheta += (f32) -GameInput->MouseDeltaX * MouseLookSensitivity;
        CameraDeltaPhi += (f32) -GameInput->MouseDeltaY * MouseLookSensitivity;
    }
    
    f32 CameraDeltaRadius = 0.0f;
    if (GameInput->CurrentKeyStates[SDL_SCANCODE_V])
    {
        CameraDeltaRadius -= 1.0f;
    }
    if (GameInput->CurrentKeyStates[SDL_SCANCODE_C])
    {
        CameraDeltaRadius += 1.0f;
    }
    CameraDeltaRadius *= KeyboardDollySensitivity * GameInput->DeltaTime;
    
    UpdateCameraSphericalOrientation(&GameState->Camera, CameraDeltaRadius, CameraDeltaTheta, CameraDeltaPhi);
    
    if (GameState->ForceFirstPersonTemp)
    {
        UpdateCameraForceRadius(&GameState->Camera, 0);
    }

    f32 TestTriangleRotSensitivity = 100.0f;
    f32 TestTriangleDeltaYaw = 0.0f;
    f32 TestTriangleDeltaPitch = 0.0f;
    if (GameInput->CurrentKeyStates[SDL_SCANCODE_LEFT])
    {
        TestTriangleDeltaYaw -= 1.0f;
    }
    if (GameInput->CurrentKeyStates[SDL_SCANCODE_RIGHT])
    {
        TestTriangleDeltaYaw += 1.0f;
    }
    if (GameInput->CurrentKeyStates[SDL_SCANCODE_UP])
    {
        TestTriangleDeltaPitch += 1.0f;
    }
    if (GameInput->CurrentKeyStates[SDL_SCANCODE_DOWN])
    {
        TestTriangleDeltaPitch -= 1.0f;
    }
    GameState->TestTriangleYaw += TestTriangleDeltaYaw * TestTriangleRotSensitivity * GameInput->DeltaTime;
    GameState->TestTrianglePitch += TestTriangleDeltaPitch * TestTriangleRotSensitivity * GameInput->DeltaTime;
    if (GameState->TestTriangleYaw > 360.0f)
    {
        GameState->TestTriangleYaw -= 360.0f;
    }
    if (GameState->TestTriangleYaw < 0.0f)
    {
        GameState->TestTriangleYaw += 360.0f;
    }
    if (GameState->TestTrianglePitch > 360.0f)
    {
        GameState->TestTrianglePitch -= 360.0f;
    }
    if (GameState->TestTrianglePitch < 0.0f)
    {
        GameState->TestTrianglePitch += 360.0f;
    }

    if (!GameInput->CurrentKeyStates[SDL_SCANCODE_TAB] && !GameInput->MouseButtonState[1])
    {
        PlayerInstance->Rotation = Quat(Vec3(0,1,0), ToRadiansF(GameState->Camera.Theta + 180.0f));
    }
    
    vec3 PlayerTranslation = Vec3();
    if (GameInput->CurrentKeyStates[SDL_SCANCODE_W])
    {
        PlayerTranslation.Z += 1.0f;
    }
    if (GameInput->CurrentKeyStates[SDL_SCANCODE_S])
    {
        PlayerTranslation.Z -= 1.0f;
    }
    if (GameInput->CurrentKeyStates[SDL_SCANCODE_A])
    {
        PlayerTranslation.X += 1.0f;
    }
    if (GameInput->CurrentKeyStates[SDL_SCANCODE_D])
    {
        PlayerTranslation.X -= 1.0f;
    }
    if (GameInput->CurrentKeyStates[SDL_SCANCODE_LSHIFT])
    {
        PlayerTranslation.Y -= 1.0f;
    }
    if (GameInput->CurrentKeyStates[SDL_SCANCODE_SPACE])
    {
        PlayerTranslation.Y += 1.0f;
    }
    PlayerTranslation = RotateVecByQuatSlow(VecNormalize(PlayerTranslation), PlayerInstance->Rotation);
    PlayerTranslation = 5.0f * GameInput->DeltaTime * PlayerTranslation;
    // TODO: Make this work with collisions
    if (GameInput->CurrentKeyStates[SDL_SCANCODE_LCTRL])
    {
        PlayerInstance->Position.Y = 0.0f;
    }
    
    Assert(PlayerBlueprint->BoundingVolume);
    aabb *PlayerAABB = &PlayerBlueprint->BoundingVolume->AABB;
    vec3 AABoxAxes[] = { Vec3(1,0,0), Vec3(0,1,0), Vec3(0,0,1) };

    enum collision_debug_state
    {
        COLLISION_DEBUG_NONE,
        COLLISION_DEBUG_PICKED,
        COLLISION_DEBUG_COLLIDED
    };

    enum collision_debug_type
    {
        COLLISION_DEBUG_BOX,
        COLLISION_DEBUG_TRI
    };
    
    struct collision_debug_entry
    {
        u32 InstanceIndex;
        collision_debug_state State;

        collision_debug_type T;
        union
        {
            struct
            {
                vec3 BoxCenter;
                vec3 BoxExtents;
            };

            struct
            {
                u32 MeshIndex;
                u32 TriIndex;
                vec3 Tri[3];
            };
        } D;
    };

    collision_debug_entry *CollisionDebugEntries = 0;
    u32 MaxCollisionDebugEntries = 0;
    u32 CurrentCollisionDebugEntry = 0;
    if (GameState->DebugCollisions)
    {
        MaxCollisionDebugEntries = 4096;
        CollisionDebugEntries =
            MemoryArena_PushArray(&GameState->TransientArena, MaxCollisionDebugEntries, collision_debug_entry);
    }

    vec3 TestTrianglePosition = Vec3(0,0,10);
    vec3 TestA = Vec3(-5,0,0);
    vec3 TestB = Vec3(5,0,0);
    vec3 TestC = Vec3(0,6,0);
    quat TestTriangleQuat = Quat(Vec3(0,1,0),ToRadiansF(GameState->TestTriangleYaw)) * Quat(Vec3(1,0,0), ToRadiansF(GameState->TestTrianglePitch));
    TestA = RotateVecByQuatSlow(TestA, TestTriangleQuat) + TestTrianglePosition;
    TestB = RotateVecByQuatSlow(TestB, TestTriangleQuat) + TestTrianglePosition;
    TestC = RotateVecByQuatSlow(TestC, TestTriangleQuat) + TestTrianglePosition;
    DD_PushTriangle(&GameState->DebugDrawRenderUnit, TestA, TestB, TestC, Vec3(0,1,0));

    u32 CollisionIterations = 4; // NOTE: Iteration #4 is only to check if iteration #3 solved collisions
    b32 PlayerTranslationWasAdjusted = false;
    b32 TranslationIsSafe = false;
    for (u32 IterationIndex = 0;
         IterationIndex < CollisionIterations;
         ++IterationIndex)
    {
        // TODO: Make this proper
        if (IgnoreCollisions)
        {
            TranslationIsSafe = true;
            break;
        }
        
        f32 SmallestPenetrationDepth = FLT_MAX;
        vec3 CollisionNormal = {};
        
        b32 TranslationWillWorsenACollision = false;
        
        for (u32 InstanceIndex = 1;
             InstanceIndex < GameState->WorldObjectInstanceCount;
             ++InstanceIndex)
        {
            if (InstanceIndex != GameState->PlayerWorldInstanceID)
            {
                world_object_instance *Instance = GameState->WorldObjectInstances + InstanceIndex;
                world_object_blueprint *Blueprint = GameState->WorldObjectBlueprints + Instance->BlueprintID;

                switch (Blueprint->CollisionType)
                {
                    case COLLISION_TYPE_NONE:
                    {
                        continue;
                    } break;

                    case COLLISION_TYPE_MESH:
                    {
                        imported_model *ImportedModel = Blueprint->ImportedModel;

                        vec3 InstanceTranslation = Instance->Position;
                        mat3 InstanceTransform = Mat3GetRotationAndScale(Instance->Rotation, Instance->Scale);

                        for (u32 ImportedMeshIndex = 0;
                             ImportedMeshIndex < ImportedModel->MeshCount;
                             ++ImportedMeshIndex)
                        {
                            imported_mesh *ImportedMesh = ImportedModel->Meshes + ImportedMeshIndex;

                            u32 TriangleCount = ImportedMesh->IndexCount / 3;
                            Assert(ImportedMesh->IndexCount % 3 == 0);

                            vec3 MeshSmallestOverlapAxis = {};
                            f32 MeshSmallestOverlap = FLT_MAX;
                            b32 MeshTriNormalOverlapFound = false;
                            b32 CollidingTriangleIsFound = false;

                            for (u32 TriangleIndex = 0;
                                 TriangleIndex < TriangleCount;
                                 ++TriangleIndex)
                            {
                                u32 BaseIndexIndex = TriangleIndex * 3;
                                i32 IndexA = ImportedMesh->Indices[BaseIndexIndex];
                                i32 IndexB = ImportedMesh->Indices[BaseIndexIndex+1];
                                i32 IndexC = ImportedMesh->Indices[BaseIndexIndex+2];
                                vec3 A = InstanceTransform * ImportedMesh->VertexPositions[IndexA] + InstanceTranslation;
                                vec3 B = InstanceTransform * ImportedMesh->VertexPositions[IndexB] + InstanceTranslation;
                                vec3 C = InstanceTransform * ImportedMesh->VertexPositions[IndexC] + InstanceTranslation;

                                vec3 TriSmallestOverlapAxis;
                                f32 TriSmallestOverlap;
                                b32 TriOverlapAxisIsTriNormal;

                                vec3 TriNormal = VecNormalize(VecCross(B - A, C - B));

                                b32 IsSeparated;

                                if (VecDot(TriNormal, PlayerTranslation) > FLT_EPSILON)
                                {
                                    IsSeparated = 
                                        IsThereASeparatingAxisTriBoxFast(A, B, C,
                                                                         PlayerInstance->Position + PlayerTranslation + PlayerAABB->Center,
                                                                         PlayerAABB->Extents, AABoxAxes,
                                                                         &TriSmallestOverlap);
                                    TriSmallestOverlapAxis = TriNormal;
                                    TriOverlapAxisIsTriNormal = true;
                                }
                                else
                                {
                                    IsSeparated =
                                        IsThereASeparatingAxisTriBox(A, B, C,
                                                                     PlayerInstance->Position + PlayerTranslation + PlayerAABB->Center,
                                                                     PlayerAABB->Extents, AABoxAxes,
                                                                     &TriSmallestOverlapAxis,
                                                                     &TriSmallestOverlap, &TriOverlapAxisIsTriNormal);
                                }

                                if (!IsSeparated)
                                {
                                    CollidingTriangleIsFound = true;

                                    if (TriOverlapAxisIsTriNormal)
                                    {
                                        if (MeshTriNormalOverlapFound)
                                        {
                                            if (TriSmallestOverlap < MeshSmallestOverlap)
                                            {
                                                MeshSmallestOverlap = TriSmallestOverlap;
                                                MeshSmallestOverlapAxis = TriSmallestOverlapAxis;
                                            }
                                        }
                                        else
                                        {
                                            MeshSmallestOverlap = TriSmallestOverlap;
                                            MeshSmallestOverlapAxis = TriSmallestOverlapAxis;
                                            MeshTriNormalOverlapFound = true;
                                        }
                                    }
                                    else
                                    {
                                        if (!MeshTriNormalOverlapFound)
                                        {
                                            if (TriSmallestOverlap < MeshSmallestOverlap)
                                            {
                                                MeshSmallestOverlap = TriSmallestOverlap;
                                                MeshSmallestOverlapAxis = TriSmallestOverlapAxis;
                                            }
                                        }
                                    }
                                }

                                if (CollisionDebugEntries)
                                {
                                    collision_debug_entry *Entry = CollisionDebugEntries + CurrentCollisionDebugEntry++;
                                    *Entry = {};

                                    if (!IsSeparated)
                                    {
                                        Entry->State = COLLISION_DEBUG_COLLIDED;
                                    }
                                    Entry->InstanceIndex = InstanceIndex;
                                    Entry->T = COLLISION_DEBUG_TRI;
                                    
                                    Entry->D.MeshIndex = ImportedMeshIndex;
                                    Entry->D.TriIndex = TriangleIndex;
                                    Entry->D.Tri[0] = A;
                                    Entry->D.Tri[1] = B;
                                    Entry->D.Tri[2] = C;
                                }
                            }

                            if (CollidingTriangleIsFound)
                            {
                                if (VecDot(MeshSmallestOverlapAxis, PlayerTranslation) <= FLT_EPSILON)
                                {
                                    if (MeshSmallestOverlap < SmallestPenetrationDepth)
                                    {
                                        SmallestPenetrationDepth = MeshSmallestOverlap;
                                        CollisionNormal = MeshSmallestOverlapAxis;
                                        TranslationWillWorsenACollision = true;
                                    }
                                }
                            }
                        }
                    } break;
                
                    case COLLISION_TYPE_BV_AABB:
                    {
                        Assert(Blueprint->BoundingVolume);
                        aabb *AABB = &Blueprint->BoundingVolume->AABB;

                        f32 ThisPenetrationDepth = FLT_MAX;
                        vec3 ThisCollisionNormal = {};
                        
                        b32 SeparatingAxisFound = false;
                        for (u32 AxisIndex = 0;
                             AxisIndex < 3;
                             ++AxisIndex)
                        {
                            f32 PlayerCenter = PlayerInstance->Position.E[AxisIndex] + PlayerTranslation.E[AxisIndex] + PlayerAABB->Center.E[AxisIndex];
                            f32 PlayerMin = PlayerCenter - PlayerAABB->Extents.E[AxisIndex];
                            f32 PlayerMax = PlayerCenter + PlayerAABB->Extents.E[AxisIndex];

                            f32 OtherCenter = Instance->Position.E[AxisIndex] + AABB->Center.E[AxisIndex];
                            f32 OtherMin = OtherCenter - AABB->Extents.E[AxisIndex];
                            f32 OtherMax = OtherCenter + AABB->Extents.E[AxisIndex];

                            f32 PositiveAxisPenetrationDepth = OtherMax - PlayerMin;
                            f32 NegativeAxisPenetrationDepth = PlayerMax - OtherMin;

                            b32 NegativeAxisIsShortest = (NegativeAxisPenetrationDepth < PositiveAxisPenetrationDepth);
                            f32 AxisPenetrationDepth = (NegativeAxisIsShortest ?
                                                        NegativeAxisPenetrationDepth : PositiveAxisPenetrationDepth);

                            if (AxisPenetrationDepth < 0.0f)
                            {
                                SeparatingAxisFound = true;
                                break;
                            }

                            if (AxisPenetrationDepth < ThisPenetrationDepth)
                            {
                                ThisPenetrationDepth = AxisPenetrationDepth;
                                ThisCollisionNormal = Vec3();
                                ThisCollisionNormal.E[AxisIndex] = NegativeAxisIsShortest ? -1.0f : 1.0f;
                            }
                        }

                        if (!SeparatingAxisFound &&
                            VecDot(ThisCollisionNormal, PlayerTranslation) < -FLT_EPSILON &&
                            ThisPenetrationDepth < SmallestPenetrationDepth)
                        {
                            SmallestPenetrationDepth = ThisPenetrationDepth;
                            CollisionNormal = ThisCollisionNormal;
                            TranslationWillWorsenACollision = true;
                        }

                        if (CollisionDebugEntries)
                        {
                            collision_debug_entry *Entry = CollisionDebugEntries + CurrentCollisionDebugEntry++;
                            *Entry = {};

                            if (!SeparatingAxisFound)
                            {
                                Entry->State = COLLISION_DEBUG_COLLIDED;
                            }
                            Entry->InstanceIndex = InstanceIndex;
                            Entry->T = COLLISION_DEBUG_BOX;
                                    
                            Entry->D.BoxCenter = Instance->Position + AABB->Center;
                            Entry->D.BoxExtents = AABB->Extents;
                        }
                    } break;

                    default:
                    {
                        InvalidCodePath;
                    } break;
                }
            }
        }

        // TODO: Keep track of up to 4 collisions each iteration, then choose one of those to be the collision
        // solved in this iteration. Take each of the 4 collisions, and calculate: if we resolve that collision, what is the resulting
        // greatest collision depth on the others of the 4 collisions. Pick the collision with the least resulting greatest collision depth.
        // If we keep track of collision normals and penetration depths of all 4 of those collisions, we can project onto them, and see
        // the resulting penetration after solving one of those collisions, without having to recalculate all of the collisions.
        // Each collision has to be for one convex polyhedron. Pick the collision for that convex polyhedron the way it's already done:
        // The normal with the shortest penetration. If the polyhedron is indeed convex, that will definitely resolve the collision with
        // that polyhedron.
        // That means, that if a mesh is concave, it has to be split into separate convex meshes, or a convex hull has to made around it.
        // If the shortest penetration method is applied to a mesh that is concave, some collisions with some surfaces will be missed, and
        // the geometry will go through that triangle. On the other hand, if each triangle is treated as a convex mesh to collide against,
        // and we store each collision with each triangle separately, we will run into the same problem as we already have with AABBs.
        // And it's possible to get stuck, even though with a different order of resolution, we could... Wait ????????????? Maybe the latter
        // is possible, look into it later. But it will mean that we will saturate that limit of 4 collisions per iteration very easily.
        // Nvm this probably won't work. Solving 

        #if 0
        {
            if (GameInput->CurrentKeyStates[SDL_SCANCODE_B] && IterationIndex == 0)
            {
                Noop;
            }

            vec3 TriNormal = VecNormalize(VecCross(TestB - TestA, TestC - TestB));
            
            vec3 ThisCollisionNormal;
            f32 ThisPenetrationDepth;
            f32 PDs[13] = {};
            u32 AI;
            b32 IsPlayerAndTriSeparated =
                IsThereASeparatingAxisTriBox(TriNormal, TestA, TestB, TestC,
                                             PlayerInstance->Position + PlayerTranslation + PlayerAABB->Center,
                                             PlayerAABB->Extents, AABoxAxes, &ThisCollisionNormal, &ThisPenetrationDepth, PDs, &AI,
                                             GameInput->CurrentKeyStates[SDL_SCANCODE_O]);

            
            
            if (!IsPlayerAndTriSeparated)
            {
                if (VecDot(TriNormal, PlayerTranslation) > FLT_EPSILON || IsZeroVector(PlayerTranslation))
                {
                    ThisCollisionNormal = TriNormal;
                }
                
                if (VecDot(ThisCollisionNormal, PlayerTranslation) < -FLT_EPSILON &&
                    ThisPenetrationDepth < ShortestPenetrationDepth)
                {
                    ShortestPenetrationDepth  = ThisPenetrationDepth;
                    CollisionNormal = ThisCollisionNormal;
                    TranslationWillWorsenACollision = true;
                }
                // ImmText_DrawQuickString(SimpleStringF("PDs{%0.3f,%0.3f,%0.3f,%0.3f,%0.3f,%0.3f,%0.3f,%0.3f,%0.3f,%0.3f,%0.3f,%0.3f,%0.3f}",
                //                                       PDs[0],PDs[1],PDs[2],PDs[3],PDs[4],PDs[5],PDs[6],PDs[7],PDs[8],PDs[9],PDs[10],PDs[11],PDs[12]).D);
                // ImmText_DrawQuickString(SimpleStringF("CollisionNormal=<%0.6f,%0.6f,%0.6f>[%u]", CollisionNormal.X, CollisionNormal.Y, CollisionNormal.Z, AI).D);
            }
        }
        #endif
        
        if (!TranslationWillWorsenACollision)
        {
            TranslationIsSafe = true;
            if (IterationIndex > 0)
            {
                PlayerTranslationWasAdjusted = true;
            }
            break;
        }
        else
        {
            if (IterationIndex == (CollisionIterations - 1))
            {
                // NOTE: If we were not able to find an adjustment to the translation that didn't worsen collisions after n-1 iterations,
                // then the translation is not safe, it will not be applied.
                break;
            }
            
            vec3 CollidingTranslationComponentDir = -CollisionNormal;
            f32 CollidingTranslationComponentMag = AbsF(VecDot(CollidingTranslationComponentDir, PlayerTranslation));
            vec3 CollidingTranslationComponent = CollidingTranslationComponentMag * CollidingTranslationComponentDir;

            PlayerTranslation -= CollidingTranslationComponent;

            DD_PushSimpleVector(&GameState->DebugDrawRenderUnit, PlayerInstance->Position, PlayerInstance->Position + CollisionNormal, Vec3(IterationIndex == 0, IterationIndex == 1, IterationIndex == 2));
        }
    }


    if (TranslationIsSafe)
    {
        PlayerInstance->Position += PlayerTranslation;
        SetThirdPersonCameraTarget(&GameState->Camera, PlayerInstance->Position + Vec3(0,GameState->PlayerCameraYOffset,0));
    }

    //
    // NOTE: Mouse picking
    //
    vec3 CameraFront = -VecSphericalToCartesian(GameState->Camera.Theta, GameState->Camera.Phi);

    vec3 RayPosition = GameState->Camera.Position;
    vec3 RayDirection = VecNormalize(CameraFront);
    f32 RayTMin = FLT_MAX;
    u32 ClosestInstanceIndex = 0;
    u32 ClosestMeshIndex = 0;
    u32 ClosestTriangleIndex = 0;
    b32 ClosestIsTriangle = false;
    vec3 ClosestTriVerts[3] = {};
    vec3 ClosestBoxCenter = {};
    vec3 ClosestBoxExtents = {};
    
    u64 TrianglesChecked = 0;
    for (u32 InstanceIndex = 1;
         InstanceIndex < GameState->WorldObjectInstanceCount;
         ++InstanceIndex)
    {
        if (InstanceIndex != GameState->PlayerWorldInstanceID)
        {
            world_object_instance *Instance = GameState->WorldObjectInstances + InstanceIndex;
            world_object_blueprint *Blueprint = GameState->WorldObjectBlueprints + Instance->BlueprintID;

            switch (Blueprint->CollisionType)
            {
                case COLLISION_TYPE_NONE:
                {
                    continue;
                } break;

                case COLLISION_TYPE_NONE_MOUSE:
                case COLLISION_TYPE_MESH:
                {
                    imported_model *ImportedModel = Blueprint->ImportedModel;

                    vec3 InstanceTranslation = Instance->Position;
                    mat3 InstanceTransform = Mat3GetRotationAndScale(Instance->Rotation, Instance->Scale);

                    mat4 *BoneTransforms = 0;
                    if (Instance->AnimationState != 0)
                    {
                        u32 BoneCount = Instance->AnimationState->Armature->BoneCount;
                        BoneTransforms = MemoryArena_PushArrayAndZero(&GameState->TransientArena, BoneCount, mat4);
                        ComputeTransformsForAnimation(Instance->AnimationState, BoneTransforms, BoneCount);
                        for (u32 BoneIndex = 0;
                             BoneIndex < BoneCount;
                             ++BoneIndex)
                        {
                            if (BoneIndex == 0)
                            {
                                BoneTransforms[BoneIndex] = Mat4(1.0f);
                            }
                            else
                            {
                                BoneTransforms[BoneIndex] = (BoneTransforms[BoneIndex] *
                                                             Instance->AnimationState->Armature->Bones[BoneIndex].InverseBindTransform);
                            }
                        }
                    }

                    for (u32 ImportedMeshIndex = 0;
                         ImportedMeshIndex < ImportedModel->MeshCount;
                         ++ImportedMeshIndex)
                    {
                        imported_mesh *ImportedMesh = ImportedModel->Meshes + ImportedMeshIndex;

                        vec3 *Vertices = ImportedMesh->VertexPositions;
                        u32 VertexCount = ImportedMesh->VertexCount;

                        if (BoneTransforms)
                        {
                            vec3 *AnimationTransformedVertices =
                                MemoryArena_PushArrayAndZero(&GameState->TransientArena, VertexCount, vec3);
                            
                            for (u32 VertexIndex = 0;
                                 VertexIndex < VertexCount;
                                 ++VertexIndex)
                            {
                                vert_bone_ids *VertBoneIDs = ImportedMesh->VertexBoneIDs + VertexIndex;
                                vert_bone_weights *VertBoneWeights = ImportedMesh->VertexBoneWeights + VertexIndex;

                                vec4 Vertex = Vec4(Vertices[VertexIndex], 1);
                                vec3 *AnimationTransformedVertex = AnimationTransformedVertices + VertexIndex;

                                b32 FoundBoneForVert = false;
                                f32 TotalWeight = 0.0f;
                                for (u32 BoneIndex = 0;
                                     BoneIndex < MAX_BONES_PER_VERTEX;
                                     ++BoneIndex)
                                {
                                    u32 BoneID = VertBoneIDs->D[BoneIndex];
                                    if (BoneID > 0)
                                    {
                                        mat4 *BoneTransform = BoneTransforms + BoneID;
                                        f32 BoneWeight = VertBoneWeights->D[BoneIndex];
                                        (*AnimationTransformedVertex) += BoneWeight * Vec3((*BoneTransform) * Vertex);
                                        TotalWeight += BoneWeight;
                                        FoundBoneForVert = true;
                                    }
                                }
                                Assert(TotalWeight >= 1 - FLT_EPSILON);
                                Assert(FoundBoneForVert);
                            }

                            Vertices = AnimationTransformedVertices;
                        }

                        u32 TriangleCount = ImportedMesh->IndexCount / 3;
                        Assert(ImportedMesh->IndexCount % 3 == 0);

                        for (u32 TriangleIndex = 0;
                             TriangleIndex < TriangleCount;
                             ++TriangleIndex)
                        {
                            u32 BaseIndexIndex = TriangleIndex * 3;
                            i32 IndexA = ImportedMesh->Indices[BaseIndexIndex];
                            i32 IndexB = ImportedMesh->Indices[BaseIndexIndex+1];
                            i32 IndexC = ImportedMesh->Indices[BaseIndexIndex+2];
                            vec3 A = InstanceTransform * Vertices[IndexA] + InstanceTranslation;
                            vec3 B = InstanceTransform * Vertices[IndexB] + InstanceTranslation;
                            vec3 C = InstanceTransform * Vertices[IndexC] + InstanceTranslation;
                            
                            // TODO: Cast a ray against triangle
                            f32 ThisTMin;
                            b32 IsRayIntersecting =
                                IntersectRayTri(RayPosition, RayDirection, A, B, C, 0, 0, 0, &ThisTMin);
                            
                            if (IsRayIntersecting && ThisTMin < RayTMin)
                            {
                                RayTMin = ThisTMin;
                                ClosestInstanceIndex = InstanceIndex;
                                ClosestMeshIndex = ImportedMeshIndex;
                                ClosestTriangleIndex = TriangleIndex;
                                ClosestIsTriangle = true;
                                ClosestTriVerts[0] = A;
                                ClosestTriVerts[1] = B;
                                ClosestTriVerts[2] = C;
                            }

                            TrianglesChecked++;
                        }
                    }
                } break;
                
                case COLLISION_TYPE_BV_AABB:
                {
                    Assert(Blueprint->BoundingVolume);
                    aabb *AABB = &Blueprint->BoundingVolume->AABB;

                    f32 ThisTMin;
                    b32 IsRayIntersecting =
                        IntersectRayAABB(RayPosition, RayDirection, AABB->Center + Instance->Position, AABB->Extents, &ThisTMin, 0);

                    if (IsRayIntersecting && ThisTMin < RayTMin)
                    {
                        RayTMin = ThisTMin;
                        ClosestInstanceIndex = InstanceIndex;
                        ClosestIsTriangle = false;
                        ClosestBoxCenter = AABB->Center;
                        ClosestBoxExtents = AABB->Extents;
                    }
                } break;

                default:
                {
                    InvalidCodePath;
                } break;
            }
        }
    }

    if (ClosestInstanceIndex > 0)
    {
        if (CollisionDebugEntries)
        {
            if (ClosestIsTriangle)
            {
                collision_debug_entry *Entry = CollisionDebugEntries;
                for (u32 EntryIndex = 0;
                     EntryIndex < CurrentCollisionDebugEntry;
                     ++EntryIndex, ++Entry)
                {
                    if (Entry->InstanceIndex == ClosestInstanceIndex &&
                        Entry->T == COLLISION_DEBUG_TRI &&
                        Entry->D.MeshIndex == ClosestMeshIndex &&
                        Entry->D.TriIndex == ClosestTriangleIndex)
                    {
                        if (Entry->State != COLLISION_DEBUG_COLLIDED) Entry->State = COLLISION_DEBUG_PICKED;
                    }
                }
            }
            else
            {
                collision_debug_entry *Entry = CollisionDebugEntries;
                for (u32 EntryIndex = 0;
                     EntryIndex < CurrentCollisionDebugEntry;
                     ++EntryIndex, ++Entry)
                {
                    if (Entry->InstanceIndex == ClosestInstanceIndex)
                    {
                        if (Entry->State != COLLISION_DEBUG_COLLIDED) Entry->State = COLLISION_DEBUG_PICKED;
                    }
                }
            }
        }

        if (ClosestIsTriangle)
        {
            ImmText_DrawQuickString(SimpleStringF("Looking at inst#%d [mesh#%d;tri#%d]",
                                                  ClosestInstanceIndex, ClosestMeshIndex, ClosestTriangleIndex).D);
        }
        else
        {
            ImmText_DrawQuickString(SimpleStringF("Looking at inst#%d [aabb]", ClosestInstanceIndex).D);
        }
    }

    //
    // NOTE: Draw debug collisions if enabled
    //
    if (GameState->DebugCollisions)
    {
        ImmText_DrawQuickString(SimpleStringF("Triangles checked for mouse picking %llu", TrianglesChecked).D);
        
        // NOTE: Add player's collision box
        {
            collision_debug_entry *Entry = CollisionDebugEntries + CurrentCollisionDebugEntry++;
            Entry->InstanceIndex = GameState->PlayerWorldInstanceID;
            Entry->State = ((PlayerTranslationWasAdjusted || !TranslationIsSafe) ? COLLISION_DEBUG_COLLIDED : COLLISION_DEBUG_NONE);
            Entry->T = COLLISION_DEBUG_BOX;
            Entry->D.BoxCenter = PlayerInstance->Position + PlayerAABB->Center;
            Entry->D.BoxExtents = PlayerAABB->Extents;
        } 

        collision_debug_entry *Entry = CollisionDebugEntries;
        // NOTE: First only draw non-affected geometry
        #if 0
        for (u32 EntryIndex = 0;
             EntryIndex < CurrentCollisionDebugEntry;
             ++EntryIndex, ++Entry)
        {
            if (Entry->State == COLLISION_DEBUG_NONE)
            {
                vec3 Color = Vec3(0,1,0);
            
                switch (Entry->T)
                {
                    case COLLISION_DEBUG_BOX:
                    {
                        DD_PushAABox(&GameState->DebugDrawRenderUnit, Entry->D.BoxCenter, Entry->D.BoxExtents, Color);
                    } break;

                    case COLLISION_DEBUG_TRI:
                    {
                        DD_PushTriangle(&GameState->DebugDrawRenderUnit, Entry->D.Tri[0], Entry->D.Tri[1], Entry->D.Tri[2], Color);
                    } break;

                    default:
                    {
                        InvalidCodePath;
                    } break;
                }
            }
        }
        #endif

        // NOTE: Then draw picked/collided geometry (This is so piggy, just want to draw them in the right order quickly)
        Entry = CollisionDebugEntries;
        for (u32 EntryIndex = 0;
             EntryIndex < CurrentCollisionDebugEntry;
             ++EntryIndex, ++Entry)
        {
            if (Entry->State != COLLISION_DEBUG_NONE)
            {
                vec3 Color;
                switch (Entry->State)
                {
                    case COLLISION_DEBUG_PICKED:
                    {
                        Color = Vec3(0,0,1);
                    } break;

                    case COLLISION_DEBUG_COLLIDED:
                    {
                        Color = Vec3(1,0,0);
                    } break;

                    default:
                    {
                        Color = Vec3(0,1,0);
                    } break;
                }
            
                switch (Entry->T)
                {
                    case COLLISION_DEBUG_BOX:
                    {
                        DD_PushAABox(&GameState->DebugDrawRenderUnit, Entry->D.BoxCenter, Entry->D.BoxExtents, Color);
                        if (Entry->State == COLLISION_DEBUG_COLLIDED)
                        {
                            ImmText_DrawQuickString(SimpleStringF("Collision: inst#%d [aabb], %0.3f,%0.3f,%0.3f", Entry->InstanceIndex,
                                                                  Color.R, Color.G, Color.B).D);
                        }
                    } break;

                    case COLLISION_DEBUG_TRI:
                    {
                        DD_PushTriangle(&GameState->DebugDrawRenderUnit, Entry->D.Tri[0], Entry->D.Tri[1], Entry->D.Tri[2], Color);
                        if (Entry->State == COLLISION_DEBUG_COLLIDED)
                        {
                            ImmText_DrawQuickString(SimpleStringF("Collision: inst#%d [mesh#%d;tri#%d], %0.3f,%0.3f,%0.3f",
                                                                  Entry->InstanceIndex, Entry->D.MeshIndex, Entry->D.TriIndex,
                                                                  Color.R, Color.G, Color.B).D);
                        }
                    } break;

                    default:
                    {
                        InvalidCodePath;
                    } break;
                }
            }
        }
    }

#if 0
    DD_PushCoordinateAxes(&GameState->DebugDrawRenderUnit,
                          Vec3(-7.5f, 0.0f, -7.5f),
                          Vec3(1.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f),
                          3.0f);
#endif

    if (GameState->ForceFirstPersonTemp)
    {
        DD_PushPoint(&GameState->DebugDrawRenderUnit, GameState->Camera.Position + CameraFront, Vec3(1), 4);
    }
    
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

    //
    // NOTE: UI
    //
    Noop;
    
    OpenGL_SetUniformVec3F(GameState->StaticRenderUnit.ShaderID, "ViewPosition", (f32 *) &GameState->Camera.Position, true);
    OpenGL_SetUniformVec3F(GameState->SkinnedRenderUnit.ShaderID, "ViewPosition", (f32 *) &GameState->Camera.Position, true);
    
    //
    // NOTE: Render
    //
    glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    render_unit *RenderUnits[] = {
        &GameState->StaticRenderUnit,
        &GameState->SkinnedRenderUnit,
        &GameState->DebugDrawRenderUnit,
        &GameState->ImmTextRenderUnit
    };
    
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

        if (RenderUnit->IsOverlay)
        {
            glDisable(GL_DEPTH_TEST);
        }
        else if (!RenderUnit->IsOverlay)
        {
            glEnable(GL_DEPTH_TEST);
        }

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
                        if (InstanceID > 0 &&
                            (InstanceID != GameState->PlayerWorldInstanceID || !GameState->ForceFirstPersonTemp))
                        {
                            // b32 ShouldLog = (InstanceID == 5);
                            world_object_instance *Instance = GameState->WorldObjectInstances + InstanceID;

                            mat4 ModelTransform = Mat4GetFullTransform(Instance->Position, Instance->Rotation, Instance->Scale);

                            if (InstanceID == GameState->PlayerWorldInstanceID)
                            {
                                Noop;
                            }

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
                                        BoneTransform = Mat4(1.0f);
                                    }
                                    else
                                    {
                                        BoneTransform = BoneTransforms[BoneIndex] * Armature->Bones[BoneIndex].InverseBindTransform;
                                    }

                                    simple_string UniformName = SimpleStringF("BoneTransforms[%d]", BoneIndex);
                                    OpenGL_SetUniformMat4F(RenderUnit->ShaderID, UniformName.D, (f32 *) &BoneTransform, false);
                                }
                        
                                MemoryArena_Unfreeze(&GameState->RenderArena);
                            }
                            else
                            {
                                // TODO: Handle a skinned model in a rest pose better.
                                // For now it just clears out all bones in uniforms, or it will keep the values of previous
                                // skinned models being rendered.
                                for (u32 BoneIndex = 0;
                                     BoneIndex < MAX_BONES_PER_MODEL;
                                     ++BoneIndex)
                                {
                                    mat4 BoneTransform = Mat4(1.0f);

                                    simple_string UniformName = SimpleStringF("BoneTransforms[%d]", BoneIndex);
                                    OpenGL_SetUniformMat4F(RenderUnit->ShaderID, UniformName.D, (f32 *) &BoneTransform, false);
                                }
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

                    if (DebugMarker->LineWidth >= 1.0f)
                    {
                        glLineWidth(DebugMarker->LineWidth);
                    }
                    else
                    {
                        glLineWidth(1.0f);
                    }

                    if (DebugMarker->PointSize >= 1.0f)
                    {
                        glPointSize(DebugMarker->PointSize);
                    }
                    else
                    {
                        glPointSize(1.0f);
                    }

                    if (DebugMarker->IsOverlay)
                    {
                        glDisable(GL_DEPTH_TEST);
                    }
                    else if (!DebugMarker->IsOverlay)
                    {
                        glEnable(GL_DEPTH_TEST);
                    }
                   
                    
                    glDrawElementsBaseVertex(DebugMarker->IsPointMode ? GL_POINTS : GL_LINES,
                                             Marker->IndexCount,
                                             GL_UNSIGNED_INT,
                                             (void *) (Marker->StartingIndex * sizeof(i32)),
                                             Marker->BaseVertexIndex);

                    if (DebugMarker->IsOverlay)
                    {
                        glEnable(GL_DEPTH_TEST);
                    }
                    else if (!DebugMarker->IsOverlay)
                    {
                        glDisable(GL_DEPTH_TEST);
                    }
                    
                    if (DebugMarker->LineWidth >= 1.0f)
                    {
                        glLineWidth(1.0f);
                    }
                    if (DebugMarker->PointSize >= 1.0f)
                    {
                        glPointSize(1.0f);
                    }
                    
                } break; // case RENDER_STATE_DEBUG

                case RENDER_STATE_IMM_TEXT:
                {
                    render_state_imm_text *ImmText = &Marker->StateD.ImmText;
                    
                    OpenGL_BindAndActivateTexture(0, ImmText->AtlasTextureID);

                    glDrawElementsBaseVertex(GL_TRIANGLES,
                                             Marker->IndexCount,
                                             GL_UNSIGNED_INT,
                                             (void *) (Marker->StartingIndex * sizeof(i32)),
                                             Marker->BaseVertexIndex);
                } break; // case RENDER_STATE_IMM_TEXT

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

    ImmText_ResetQuickDraw();
    MemoryArena_Reset(&GameState->TransientArena);
}

#include "opusone_camera.cpp"
#include "opusone_assimp.cpp"
#include "opusone_render.cpp"
#include "opusone_animation.cpp"
#include "opusone_immtext.cpp"
