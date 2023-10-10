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
#include "opusone_collision.h"

#include <cstdio>
#include <glad/glad.h>

#include "opusone_debug_draw.cpp"

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

        //
        // NOTE: Load models using assimp
        //
        simple_string ModelPaths[] = {
            SimpleString("resources/models/box_room_separate/BoxRoomSeparate.gltf"),
            SimpleString("resources/models/adam/adam_new.gltf"),
            SimpleString("resources/models/complex_animation_keys/AnimationStudy2c.gltf"),
            SimpleString("resources/models/container/Container.gltf"),
            SimpleString("resources/models/snowman/Snowman.gltf"),
        };

        collision_type BlueprintCollisionTypes[] = {
            COLLISION_TYPE_POLYHEDRON_SET,
            COLLISION_TYPE_AABB,
            COLLISION_TYPE_NONE,
            COLLISION_TYPE_POLYHEDRON_SET,
            COLLISION_TYPE_POLYHEDRON_SET
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
            
            switch (Blueprint->CollisionType)
            {
                case COLLISION_TYPE_NONE:
                {
                    Blueprint->CollisionGeometry = 0;
                } break;
                
                case COLLISION_TYPE_AABB:
                {
                    // TODO: Handle properly for different models
                    f32 AdamHeight = 1.8412f;
                    f32 AdamHalfHeight = AdamHeight * 0.5f;

                    Blueprint->CollisionGeometry = MemoryArena_PushStruct(&GameState->WorldArena, collision_geometry);
                    Blueprint->CollisionGeometry->AABB = {};
                    Blueprint->CollisionGeometry->AABB.Center = Vec3(0, AdamHalfHeight, 0);
                    Blueprint->CollisionGeometry->AABB.Extents = Vec3 (0.3f, AdamHalfHeight, 0.3f);
                } break;

                case COLLISION_TYPE_POLYHEDRON_SET:
                {
                    Blueprint->CollisionGeometry = MemoryArena_PushStruct(&GameState->WorldArena, collision_geometry);
                    polyhedron_set *PolyhedronSet = &Blueprint->CollisionGeometry->PolyhedronSet;
                    PolyhedronSet->PolyhedronCount = Blueprint->ImportedModel->MeshCount;
                    PolyhedronSet->Polyhedra = MemoryArena_PushArray(&GameState->WorldArena, PolyhedronSet->PolyhedronCount, polyhedron);
                    
                    imported_mesh *ImportedMeshCursor = Blueprint->ImportedModel->Meshes;
                    polyhedron *PolyhedronCursor = PolyhedronSet->Polyhedra;
                    for (u32 MeshIndex = 0;
                         MeshIndex < Blueprint->ImportedModel->MeshCount;
                         ++MeshIndex, ++ImportedMeshCursor, ++PolyhedronCursor)
                    {
                        ComputePolyhedronFromVertices(&GameState->WorldArena, &GameState->TransientArena,
                                                      ImportedMeshCursor->VertexPositions, ImportedMeshCursor->VertexCount,
                                                      ImportedMeshCursor->Indices, ImportedMeshCursor->IndexCount,
                                                      PolyhedronCursor);
                    }
                } break;
                
                default:
                {
                    InvalidCodePath;
                } break;
            }
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
        // NOTE: Prepare render data from the imported data
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

        DD_InitializeQuickDraw(&GameState->DebugDrawRenderUnit);
        
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
            world_object_instance { 1, Vec3(0.0f, 0.0f, 0.0f), Quat(), Vec3(1.0f, 1.0f, 1.0f) },
            world_object_instance { 1, Vec3(20.0f,0.0f, 0.0f), Quat(Vec3(0.0f, 1.0f, 1.0f), ToRadiansF(45.0f)), Vec3(0.5f, 1.0f, 2.0f) },
            world_object_instance { 1, Vec3(0.0f,0.0f,-30.0f), Quat(Vec3(1.0f, 1.0f,1.0f), ToRadiansF(160.0f)), Vec3(1.0f, 1.0f, 5.0f) },
            world_object_instance { 2, Vec3(0.0f, 0.0f, 0.0f), Quat(), Vec3(1.0f, 1.0f, 1.0f) },
            world_object_instance { 2, Vec3(-1.0f, 0.0f, 0.0f), Quat(), Vec3(1.0f, 1.0f, 1.0f) },
            world_object_instance { 2, Vec3(1.0f, 0.0f, 0.0f), Quat(), Vec3(1.0f, 1.0f, 1.0f) },
            world_object_instance { 2, Vec3(2.0f, 0.0f, 0.0f), Quat(), Vec3(1.0f, 1.0f, 1.0f) },
            world_object_instance { 3, Vec3(0.0f, 0.0f, -3.0f), Quat(), Vec3(0.3f, 0.3f, 0.3f) },
            world_object_instance { 4, Vec3(-3.0f, 0.0f, -3.0f), Quat(Vec3(0,1,0), ToRadiansF(45.0f)), Vec3(1.0f, 1.0f, 1.0f) },
            world_object_instance { 2, Vec3(0.0f, 0.1f, 5.0f), Quat(Vec3(0,1,0), ToRadiansF(180.0f)), Vec3(1.0f, 1.0f, 1.0f) },
            world_object_instance { 4, Vec3(-3.0f, 0.0f, 3.0f), Quat(), Vec3(1.0f, 1.0f, 1.0f) },
            world_object_instance { 5, Vec3(5.0f, 0.0f, 5.0f), Quat(), Vec3(3,3,3) },
            world_object_instance { 4, Vec3(3,0,-3), Quat(Vec3(1,1,0), ToRadiansF(60)), Vec3(1,1,1) },

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

        // NOTE: Set the player movement spec
        GameState->PlayerSpecJumpVelocity = 100.0f;
        GameState->PlayerSpecAccelerationValue = 300.0f;
        GameState->PlayerSpecGravityValue = -500.0f;
        GameState->PlayerSpecDragValue = 50.0f;

    }

    //
    // NOTE: Process controls
    //
    if (Platform_KeyIsDown(GameInput, SDL_SCANCODE_ESCAPE))
    {
        *GameShouldQuit = true;
    }
    
    // NOTE: Debug keys
    b32 IgnoreCollisions = Platform_KeyIsDown(GameInput, SDL_SCANCODE_LALT);
    if (Platform_KeyJustPressed(GameInput, SDL_SCANCODE_F1))
    {
        GameState->MouseControlledTemp = !GameState->MouseControlledTemp;
        Platform_SetRelativeMouse(GameState->MouseControlledTemp);
    }
    if (Platform_KeyJustPressed(GameInput, SDL_SCANCODE_F2))
    {
        GameState->ForceFirstPersonTemp = !GameState->ForceFirstPersonTemp;
    }
    if (Platform_KeyJustPressed(GameInput, SDL_SCANCODE_F3))
    {
        GameState->GravityDisabledTemp = !GameState->GravityDisabledTemp;
    }
    
    // NOTE: Gameplay keys
    game_requested_controls *RequestedControls = &GameState->RequestedControls;

    RequestedControls->PlayerForward = Platform_KeyIsDown(GameInput, SDL_SCANCODE_W);
    RequestedControls->PlayerBackward = Platform_KeyIsDown(GameInput, SDL_SCANCODE_S);
    RequestedControls->PlayerLeft = Platform_KeyIsDown(GameInput, SDL_SCANCODE_A);
    RequestedControls->PlayerRight = Platform_KeyIsDown(GameInput, SDL_SCANCODE_D);
    RequestedControls->PlayerUp = Platform_KeyIsDown(GameInput, SDL_SCANCODE_SPACE);
    RequestedControls->PlayerDown = Platform_KeyIsDown(GameInput, SDL_SCANCODE_LSHIFT);
    RequestedControls->PlayerJump = Platform_KeyJustPressed(GameInput, SDL_SCANCODE_SPACE);
    
    RequestedControls->CameraForward = Platform_KeyIsDown(GameInput, SDL_SCANCODE_V);
    RequestedControls->CameraBackward = Platform_KeyIsDown(GameInput, SDL_SCANCODE_C);
    RequestedControls->CameraLeft = Platform_KeyIsDown(GameInput, SDL_SCANCODE_Q);
    RequestedControls->CameraRight = Platform_KeyIsDown(GameInput, SDL_SCANCODE_E);
    RequestedControls->CameraUp = Platform_KeyIsDown(GameInput, SDL_SCANCODE_Z);
    RequestedControls->CameraDown = Platform_KeyIsDown(GameInput, SDL_SCANCODE_X);
    RequestedControls->CameraIsIndependent = Platform_KeyIsDown(GameInput, SDL_SCANCODE_TAB) || Platform_MouseButtonIsDown(GameInput, MouseButton_Middle);

    // NOTE: This will come from config or whatever
    f32 KeyboardLookSensitivity = 150.0f;
    f32 KeyboardDollySensitivity = 5.0f;
    f32 MouseLookSensitivity = 0.035f;

    //
    // NOTE: Game logic update
    //

    // NOTE: Updates requested by controls (camera)
    f32 CameraDeltaTheta = 0.0f;
    f32 CameraDeltaPhi = 0.0f;
    f32 CameraDeltaRadius = 0.0f;
    if (RequestedControls->CameraLeft) CameraDeltaTheta += 1.0f;
    if (RequestedControls->CameraRight) CameraDeltaTheta -= 1.0f;
    if (RequestedControls->CameraUp) CameraDeltaPhi += 1.0f;
    if (RequestedControls->CameraDown) CameraDeltaPhi -= 1.0f;
    if (RequestedControls->CameraForward) CameraDeltaRadius -= 1.0f;
    if (RequestedControls->CameraBackward) CameraDeltaRadius += 1.0f;
    CameraDeltaTheta *= KeyboardLookSensitivity * GameInput->DeltaTime;
    CameraDeltaPhi *= KeyboardLookSensitivity * GameInput->DeltaTime;
    CameraDeltaRadius *= KeyboardDollySensitivity * GameInput->DeltaTime;
    if (GameState->MouseControlledTemp)
    {
        CameraDeltaTheta += MouseLookSensitivity * (f32) (-GameInput->MouseDeltaX);
        CameraDeltaPhi += MouseLookSensitivity * (f32) (-GameInput->MouseDeltaY);
    }
    UpdateCameraSphericalOrientation(&GameState->Camera, CameraDeltaRadius, CameraDeltaTheta, CameraDeltaPhi);
    
    if (GameState->ForceFirstPersonTemp)
    {
        UpdateCameraForceRadius(&GameState->Camera, 0);
    }

    // NOTE: Updates requested by controls (player)
    world_object_instance *PlayerInstance = GameState->WorldObjectInstances + GameState->PlayerWorldInstanceID;
    world_object_blueprint *PlayerBlueprint = GameState->WorldObjectBlueprints + PlayerInstance->BlueprintID;
    
    if (!RequestedControls->CameraIsIndependent)
    {
        PlayerInstance->Rotation = Quat(Vec3(0,1,0), ToRadiansF(GameState->Camera.Theta + 180.0f));
    }

    vec3 PlayerAcceleration = {};
    if (RequestedControls->PlayerForward) PlayerAcceleration.Z += 1.0f;
    if (RequestedControls->PlayerBackward) PlayerAcceleration.Z -= 1.0f;
    if (RequestedControls->PlayerLeft) PlayerAcceleration.X += 1.0f;
    if (RequestedControls->PlayerRight) PlayerAcceleration.X -= 1.0f;
    if (GameState->GravityDisabledTemp)
    {
        if (RequestedControls->PlayerDown) PlayerAcceleration.Y -= 1.0f;
        if (RequestedControls->PlayerUp) PlayerAcceleration.Y += 1.0f;
    }
    PlayerAcceleration = GameState->PlayerSpecAccelerationValue * RotateVecByQuatSlow(VecNormalize(PlayerAcceleration), PlayerInstance->Rotation);

    if (!GameState->GravityDisabledTemp)
    {
        // NOTE: Gravity
        PlayerAcceleration.Y = GameState->PlayerSpecGravityValue;
        if (RequestedControls->PlayerJump && !GameState->PlayerAirborne)
        {
            GameState->PlayerAirborne = true;
            GameState->PlayerVelocity.Y += GameState->PlayerSpecJumpVelocity;
        }
        if (GameState->PlayerAirborne && PlayerInstance->Position.Y < 0.1f)
        {
            GameState->PlayerAirborne = false;
        }
    }
    else
    {
        GameState->PlayerAirborne = false;
    }

    // NOTE: Integration for player movement
    PlayerAcceleration -= GameState->PlayerSpecDragValue * GameState->PlayerVelocity; // NOTE: Drag
    
    vec3 PlayerTranslation = (0.5f * PlayerAcceleration * GameInput->DeltaTime * GameInput->DeltaTime +
                              GameState->PlayerVelocity * GameInput->DeltaTime);

    GameState->PlayerVelocity += PlayerAcceleration * GameInput->DeltaTime;

    ImmText_DrawQuickString(SimpleStringF("Player velocity: {%0.3f,%0.3f,%0.3f}",
                                          GameState->PlayerVelocity.X, GameState->PlayerVelocity.Y, GameState->PlayerVelocity.Z).D);
    ImmText_DrawQuickString(SimpleStringF("Player position: {%0.3f,%0.3f,%0.3f}",
                                          PlayerInstance->Position.X, PlayerInstance->Position.Y, PlayerInstance->Position.Z).D);

    // NOTE: Entity AI updates
    // ....

    // NOTE: Process entity movement
#if 0
    for (u32 InstanceIndex = 1;
         InstanceIndex < GameState->WorldObjectInstanceCount;
         ++InstanceIndex)
    {
        world_object_instance *Instance = GameState->WorldObjectInstances + InstanceIndex;
        world_object_blueprint *Blueprint = GameState->WorldObjectBlueprints + Instance->BlueprintID;

        if (Blueprint->CollisionType == COLLISION_TYPE_POLYHEDRON_SET)
        {
            Assert(Blueprint->CollisionGeometry);
            
            polyhedron_set *PolyhedronSet = CopyAndTransformPolyhedronSet(&GameState->TransientArena,
                                                                          &Blueprint->CollisionGeometry->PolyhedronSet,
                                                                          Instance->Position,
                                                                          Instance->Rotation,
                                                                          Instance->Scale);

            Assert(PolyhedronSet->PolyhedronCount > 0);

            for (u32 PolyhedronIndex = 0;
                 PolyhedronIndex < PolyhedronSet->PolyhedronCount;
                 ++PolyhedronIndex)
            {
                polyhedron *Polyhedron = PolyhedronSet->Polyhedra + PolyhedronIndex;

                edge *EdgeCursor = Polyhedron->Edges;
                for (u32 TestPolyhedronEdgeIndex = 0;
                     TestPolyhedronEdgeIndex < Polyhedron->EdgeCount;
                     ++TestPolyhedronEdgeIndex, ++EdgeCursor)
                {
                    DD_DrawQuickVector(Polyhedron->Vertices[EdgeCursor->AIndex],
                                       Polyhedron->Vertices[EdgeCursor->BIndex],
                                       Vec3(1,0,1));
                }

                vec3 *VertexCursor = Polyhedron->Vertices;
                for (u32 VertexIndex = 0;
                     VertexIndex < Polyhedron->VertexCount;
                     ++VertexIndex, ++VertexCursor)
                {
                    DD_DrawQuickPoint(*VertexCursor, Vec3(1,0,0));
                }
                
                polygon *FaceCursor = Polyhedron->Faces;
                for (u32 FaceIndex = 0;
                     FaceIndex < Polyhedron->FaceCount;
                     ++FaceIndex, ++FaceCursor)
                {
                    vec3 FaceCentroid = Vec3();
                    for (u32 VertexIndex = 0;
                         VertexIndex < FaceCursor->VertexCount;
                         ++VertexIndex)
                    {
                        FaceCentroid += Polyhedron->Vertices[FaceCursor->VertexIndices[VertexIndex]];
                    }
                    FaceCentroid /= (f32) FaceCursor->VertexCount;
                    DD_DrawQuickVector(FaceCentroid, FaceCentroid + 0.25f*FaceCursor->Plane.Normal, Vec3(1,1,1));
                }
            }
        }
    }
#endif

    Assert(PlayerBlueprint->CollisionGeometry);
    vec3 CardinalAxes[] = { Vec3(1,0,0), Vec3(0,1,0), Vec3(0,0,1) };

    u32 CollisionIterations = COLLISION_ITERATIONS + 1; // NOTE: Last iteration is only to check if can move after prev iteration
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

        box PlayerBox = {};
        PlayerBox.Center = PlayerInstance->Position + PlayerTranslation + PlayerBlueprint->CollisionGeometry->AABB.Center;
        PlayerBox.Extents = PlayerBlueprint->CollisionGeometry->AABB.Extents;
        PlayerBox.Axes[0] = CardinalAxes[0];
        PlayerBox.Axes[1] = CardinalAxes[1];
        PlayerBox.Axes[2] = CardinalAxes[2];

        f32 SmallestPenetrationDepth = FLT_MAX;
        vec3 CollisionNormal = {};
        u32 InstanceToResolveThisIteration = 0;
        
        b32 TranslationWillWorsenACollision = false;
        
        if (Platform_KeyJustPressed(GameInput, SDL_SCANCODE_B) && IterationIndex == 0)
        {
            Breakpoint;
        }
            
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

                    case COLLISION_TYPE_POLYHEDRON_SET:
                    {
                        polyhedron_set *PolyhedronSet = CopyAndTransformPolyhedronSet(&GameState->TransientArena,
                                                                                      &Blueprint->CollisionGeometry->PolyhedronSet,
                                                                                      Instance->Position,
                                                                                      Instance->Rotation,
                                                                                      Instance->Scale);
                        
                        polyhedron *Polyhedron = PolyhedronSet->Polyhedra;
                        for (u32 PolyhedronIndex = 0;
                             PolyhedronIndex < PolyhedronSet->PolyhedronCount;
                             ++PolyhedronIndex, ++Polyhedron)
                        {
                            f32 ThisPenetrationDepth;
                            vec3 ThisCollisionNormal;
                            
                            b32 AreSeparated =
                                AreSeparatedBoxPolyhedron(Polyhedron, &PlayerBox, &ThisCollisionNormal, &ThisPenetrationDepth);

                            if (!AreSeparated &&
                                VecDot(ThisCollisionNormal, PlayerTranslation) < -FLT_EPSILON &&
                                ThisPenetrationDepth < SmallestPenetrationDepth)
                            {
                                SmallestPenetrationDepth = ThisPenetrationDepth;
                                CollisionNormal =ThisCollisionNormal;
                                TranslationWillWorsenACollision = true;
                                InstanceToResolveThisIteration = InstanceIndex;

                                edge *EdgeCursor = Polyhedron->Edges;
                                for (u32 TestPolyhedronEdgeIndex = 0;
                                     TestPolyhedronEdgeIndex < Polyhedron->EdgeCount;
                                     ++TestPolyhedronEdgeIndex, ++EdgeCursor)
                                {
                                    DD_DrawQuickVector(Polyhedron->Vertices[EdgeCursor->AIndex],
                                                       Polyhedron->Vertices[EdgeCursor->BIndex],
                                                       Vec3(1,0,1));
                                }
                
                                polygon *FaceCursor = Polyhedron->Faces;
                                for (u32 FaceIndex = 0;
                                     FaceIndex < Polyhedron->FaceCount;
                                     ++FaceIndex, ++FaceCursor)
                                {
                                    vec3 FaceCentroid = Vec3();
                                    for (u32 VertexIndex = 0;
                                         VertexIndex < FaceCursor->VertexCount;
                                         ++VertexIndex)
                                    {
                                        FaceCentroid += Polyhedron->Vertices[FaceCursor->VertexIndices[VertexIndex]];
                                    }
                                    FaceCentroid /= (f32) FaceCursor->VertexCount;
                                    DD_DrawQuickVector(FaceCentroid, FaceCentroid + 0.25f*FaceCursor->Plane.Normal, Vec3(1,1,1));
                                }
                            }
                        }

                        Noop;
                    } break;
                
                    case COLLISION_TYPE_AABB:
                    {
                        Assert(Blueprint->CollisionGeometry);
                        aabb *AABB = &Blueprint->CollisionGeometry->AABB;

                        f32 ThisPenetrationDepth = FLT_MAX;
                        vec3 ThisCollisionNormal = {};
                        
                        b32 SeparatingAxisFound = false;
                        for (u32 AxisIndex = 0;
                             AxisIndex < 3;
                             ++AxisIndex)
                        {
                            f32 PlayerCenter = PlayerBox.Center.E[AxisIndex];
                            f32 PlayerMin = PlayerCenter - PlayerBox.Extents.E[AxisIndex];
                            f32 PlayerMax = PlayerCenter + PlayerBox.Extents.E[AxisIndex];

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
                            InstanceToResolveThisIteration = InstanceIndex;
                        }

                        DD_DrawQuickAABox(Instance->Position + AABB->Center, AABB->Extents, SeparatingAxisFound ? Vec3(0,1,0) : Vec3(1,0,0));
                    } break;

                    default:
                    {
                        InvalidCodePath;
                    } break;
                }
            }
        }

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

            DD_DrawQuickVector(PlayerInstance->Position, PlayerInstance->Position + CollisionNormal, Vec3(IterationIndex == 0, IterationIndex == 1, IterationIndex == 2));
            ImmText_DrawQuickString(SimpleStringF("Collision: Iteration#%d: Instance#%d",
                                                  IterationIndex, InstanceToResolveThisIteration).D);
        }
    }

    if (TranslationIsSafe)
    {
        PlayerInstance->Position += PlayerTranslation;
        SetThirdPersonCameraTarget(&GameState->Camera, PlayerInstance->Position + Vec3(0,GameState->PlayerCameraYOffset,0));
    }

    DD_DrawQuickAABox(PlayerInstance->Position + PlayerBlueprint->CollisionGeometry->AABB.Center,
                      PlayerBlueprint->CollisionGeometry->AABB.Extents,
                      PlayerTranslationWasAdjusted ? Vec3(1,0,0) : Vec3(0,1,0));

    // NOTE: Mouse picking
    vec3 CameraFront = -VecSphericalToCartesian(GameState->Camera.Theta, GameState->Camera.Phi);

    vec3 RayPosition = PlayerInstance->Position + Vec3(0,GameState->PlayerCameraYOffset,0);
    vec3 RayDirection = VecNormalize(CameraFront);
    f32 RayTMin = FLT_MAX;
    u32 ClosestInstanceIndex = 0;
    
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
                case COLLISION_TYPE_AABB:
                {
                    Assert(Blueprint->CollisionGeometry);
                    aabb *AABB = &Blueprint->CollisionGeometry->AABB;

                    f32 ThisTMin;
                    b32 IsRayIntersecting =
                        IntersectRayAABB(RayPosition, RayDirection, AABB->Center + Instance->Position, AABB->Extents, &ThisTMin, 0);

                    if (IsRayIntersecting && ThisTMin < RayTMin)
                    {
                        RayTMin = ThisTMin;
                        ClosestInstanceIndex = InstanceIndex;
                    }
                } break;

                default:
                {
                    continue;
                } break;
            }
        }
    }

    if (ClosestInstanceIndex > 0)
    {
        ImmText_DrawQuickString(SimpleStringF("Looking at inst#%d", ClosestInstanceIndex).D);
    }

    if (GameState->ForceFirstPersonTemp)
    {
        DD_DrawPoint(&GameState->DebugDrawRenderUnit, GameState->Camera.Position + CameraFront, Vec3(1), 4);
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

                            if (Instance->IsInvisible)
                            {
                                continue;
                            }

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
#include "opusone_collision.cpp"
