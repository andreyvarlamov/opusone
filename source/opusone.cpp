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

internal inline b32
IsSeparatingAxisTriBox(vec3 TestAxis, vec3 A, vec3 B, vec3 C, vec3 BoxExtents, vec3 *BoxAxes)
{
    f32 AProj = VecDot(A, TestAxis);
    f32 BProj = VecDot(B, TestAxis);
    f32 CProj = VecDot(C, TestAxis);
#if 0
    f32 MinTriProj = Min(Min(AProj, BProj), CProj);
    f32 MaxTriProj = Max(Max(AProj, BProj), CProj);
    f32 MaxBoxProj = (BoxExtents.E[0] * AbsF(VecDot(BoxAxes[0], TestAxis)) +
                      BoxExtents.E[1] * AbsF(VecDot(BoxAxes[1], TestAxis)) +
                      BoxExtents.E[2] * AbsF(VecDot(BoxAxes[2], TestAxis)));
    f32 MinBoxProj = -MaxBoxProj;
    b32 Result = (MaxTriProj < MinBoxProj || MinTriProj > MaxBoxProj);
#else
    f32 BoxRProj = (BoxExtents.E[0] * AbsF(VecDot(BoxAxes[0], TestAxis)) +
                    BoxExtents.E[1] * AbsF(VecDot(BoxAxes[1], TestAxis)) +
                    BoxExtents.E[2] * AbsF(VecDot(BoxAxes[2], TestAxis)));
    f32 MaxTriVertProj = Max(-Max(Max(AProj, BProj), CProj), Min(Min(AProj, BProj), CProj));
    b32 Result = (MaxTriVertProj > BoxRProj);
#endif
    return Result;
}

internal inline b32
IsThereASeparatingAxisTriBox(vec3 A, vec3 B, vec3 C, vec3 BoxCenter, vec3 BoxExtents, vec3 *BoxAxes)
{
    // NOTE: Translate triangle as conceptually moving AABB to origin
    A -= BoxCenter;
    B -= BoxCenter;
    C -= BoxCenter;

    // NOTE: Compute edge vectors of triangle
    vec3 AB = B - A;
    vec3 BC = C - B;
    vec3 CA = A - C;

    //
    // NOTE: Test exes a00..a22 (category 3)
    //
    vec3 Edges[] = { AB, BC, CA };
    for (u32 BoxAxisIndex = 0;
         BoxAxisIndex < 3;
         ++BoxAxisIndex)
    {
        for (u32 EdgeIndex = 0;
             EdgeIndex < 3;
             ++EdgeIndex)
        {
            vec3 TestAxis = VecCross(BoxAxes[BoxAxisIndex], Edges[EdgeIndex]);

            if (!IsZeroVector(TestAxis) && IsSeparatingAxisTriBox(TestAxis, A, B, C, BoxExtents, BoxAxes))
            {
                return true;
            }
        }
    }

    //
    // NOTE: Test box's 3 normals (category 1)
    //
    for (u32 BoxAxisIndex = 0;
         BoxAxisIndex < 3;
         ++BoxAxisIndex)
    {
        vec3 TestAxis = BoxAxes[BoxAxisIndex];
        if (IsSeparatingAxisTriBox(TestAxis, A, B, C, BoxExtents, BoxAxes))
        {
            return true;
        }
    }

    //
    // NOTE: Test triangle's normal (category 2)
    //
    vec3 TriNormal = VecCross(AB, BC);
    if (!IsZeroVector(TriNormal) && IsSeparatingAxisTriBox(TriNormal, A, B, C, BoxExtents, BoxAxes))
    {
        return true;
    }

    return false;
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

        InitializeCamera(&GameState->Camera, vec3 { 0.0f, 1.7f, 10.0f }, 0.0f, 90.0f, 2.0f, CAMERA_CONTROL_FPS);

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
            COLLISION_TYPE_NONE
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
            u32 MaxMarkers = 1024;
            u32 MaxVertices = 16384;
            u32 MaxIndices = 32768;
            
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
            world_object_instance { 4, Vec3(-3.0f, 0.0f, -3.0f), Quat(), Vec3(1.0f, 1.0f, 1.0f), 0 },
            world_object_instance { 2, Vec3(0.0f, 0.1f, 5.0f), Quat(Vec3(0,1,0), ToRadiansF(180.0f)), Vec3(1.0f, 1.0f, 1.0f), 0 }
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
        SetCameraControlScheme(&GameState->Camera, (camera_control_scheme) ((i32) GameState->Camera.ControlScheme + 1));
        GameInput->KeyWasDown[SDL_SCANCODE_F1] = true;
    }
    else if (!GameInput->CurrentKeyStates[SDL_SCANCODE_F1])
    {
        GameInput->KeyWasDown[SDL_SCANCODE_F1] = false;
    }

    //
    // NOTE: Game logic update
    //
    f32 CameraDeltaTheta = (f32) -GameInput->MouseDeltaX * 0.035f;
    f32 CameraDeltaPhi = (f32) -GameInput->MouseDeltaY * 0.035f;
    f32 CameraDeltaRadius = 0.0f;
    if (GameInput->CurrentKeyStates[SDL_SCANCODE_UP])
    {
        CameraDeltaRadius -= 1.0f;
    }
    if (GameInput->CurrentKeyStates[SDL_SCANCODE_DOWN])
    {
        CameraDeltaRadius += 1.0f;
    }
    CameraDeltaRadius *= 5.0f * GameInput->DeltaTime;
    UpdateCameraSphericalOrientation(&GameState->Camera, CameraDeltaRadius, CameraDeltaTheta, CameraDeltaPhi);

    world_object_instance *PlayerInstance = GameState->WorldObjectInstances + GameState->PlayerWorldInstanceID;
    world_object_blueprint *PlayerBlueprint = GameState->WorldObjectBlueprints + PlayerInstance->BlueprintID;
    
    PlayerInstance->Rotation *= Quat(Vec3(0,1,0), ToRadiansF(CameraDeltaTheta));
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
    vec3 PlayerDesiredPosition = PlayerInstance->Position + PlayerTranslation;
    if (GameInput->CurrentKeyStates[SDL_SCANCODE_LCTRL])
    {
        PlayerDesiredPosition.Y = 0.0f;
    }

    Assert(PlayerBlueprint->BoundingVolume);
    aabb *PlayerAABB = &PlayerBlueprint->BoundingVolume->AABB;
    vec3 AABoxAxes[] = { Vec3(1,0,0), Vec3(0,1,0), Vec3(0,0,1) };

    b32 WillCollide = false;
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

                            b32 IsPlayerAndTriSeparated =
                                IsThereASeparatingAxisTriBox(A, B, C,
                                                             PlayerDesiredPosition + PlayerAABB->Center,
                                                             PlayerAABB->Extents, AABoxAxes);

                            if (!IsPlayerAndTriSeparated)
                            {
                                DD_PushTriangle(&GameState->DebugDrawRenderUnit, A, B, C, Vec3(1,0,0));
                                ImmText_DrawQuickString(SimpleStringF("Colliding with Instance ID %d: triangle %d",
                                                                      InstanceIndex, TriangleIndex).D);
                            }

                            WillCollide |= !IsPlayerAndTriSeparated;
                        }
                    }
                } break;
                
                case COLLISION_TYPE_BV_AABB:
                {
                    Assert(Blueprint->BoundingVolume);
                    aabb *AABB = &Blueprint->BoundingVolume->AABB;
                        
                    b32 SeparatingAxisFound = false;
                    for (u32 AxisIndex = 0;
                         AxisIndex < 3;
                         ++AxisIndex)
                    {
                        f32 PlayerCenter = PlayerDesiredPosition.E[AxisIndex] + PlayerAABB->Center.E[AxisIndex];
                        f32 PlayerMin = PlayerCenter - PlayerAABB->Extents.E[AxisIndex];
                        f32 PlayerMax = PlayerCenter + PlayerAABB->Extents.E[AxisIndex];

                        f32 OtherCenter = Instance->Position.E[AxisIndex] + AABB->Center.E[AxisIndex];
                        f32 OtherMin = OtherCenter - AABB->Extents.E[AxisIndex];
                        f32 OtherMax = OtherCenter + AABB->Extents.E[AxisIndex];

                        if (PlayerMax < OtherMin || PlayerMin > OtherMax)
                        {
                            SeparatingAxisFound = true;
                            break;
                        }
                    }

                    WillCollide |= !SeparatingAxisFound;
                    b32 ThisOneCollides = !SeparatingAxisFound;

                    if (ThisOneCollides)
                    {
                        ImmText_DrawQuickString(SimpleStringF("Colliding with Instance ID %d: aabb",
                                                              InstanceIndex).D);
                    }
                
                    vec3 InstanceAABBPosition = Instance->Position + AABB->Center;
                    vec3 Color = ThisOneCollides ? Vec3(1.0f, 0.0f, 0.0f) : Vec3(1.0f, 1.0f, 0.0f);
                    DD_PushAABox(&GameState->DebugDrawRenderUnit, InstanceAABBPosition, AABB->Extents, Color);
                } break;

                default:
                {
                    InvalidCodePath;
                } break;
            }

            // if (Instance->BlueprintID == 2)
            // {
            // }
        }
    }

#if 0
    // vec3 TriA = Vec3(1,0,2);
    // vec3 TriB = Vec3(3,0,4);
    // vec3 TriC = Vec3(2,1.5f,3);

    vec3 TriA = Vec3(1,0,2);
    vec3 TriB = Vec3(3,0,2);
    vec3 TriC = Vec3(2,1.5f,2);

    // Do a generic Box-Triangle Separating Axis Test
    b32 IsPlayerAndTriSeparated = IsThereASeparatingAxisTriBox(TriA, TriB, TriC,
                                                               PlayerDesiredPosition + Vec3(0, AdamHalfHeight, 0),
                                                               AdamAABBExtents, AABoxAxes);
    
    vec3 TriColor = IsPlayerAndTriSeparated ? Vec3(1.0f, 1.0f, 0.0f) : Vec3(1.0f, 0.0f, 0.0f);
    DD_PushTriangle(&GameState->DebugDrawRenderUnit, TriA, TriB, TriC, TriColor);

    WillCollide |= !IsPlayerAndTriSeparated;
#endif

    if (!WillCollide || IgnoreCollisions)
    {
        PlayerInstance->Position = PlayerDesiredPosition;
    }
    SetThirdPersonCameraTarget(&GameState->Camera, PlayerInstance->Position + Vec3(0,GameState->PlayerCameraYOffset,0));
    
    vec3 PlayerAABBPosition = PlayerInstance->Position + PlayerAABB->Center;
    vec3 Color = WillCollide ? Vec3(1.0f, 0.0f, 0.0f) : Vec3(1.0f, 1.0f, 0.0f);
    DD_PushAABox(&GameState->DebugDrawRenderUnit, PlayerAABBPosition, PlayerAABB->Extents, Color);

    DD_PushCoordinateAxes(&GameState->DebugDrawRenderUnit,
                          Vec3(-7.5f, 0.0f, -7.5f),
                          Vec3(1.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f),
                          3.0f);
    
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

    GameState->WorldObjectInstances[9].Rotation *= Quat(Vec3(0.0f, 1.0f, 0.0f), ToRadiansF(30.0f) * GameInput->DeltaTime);
    
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
                        if (InstanceID > 0)
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
}

#include "opusone_camera.cpp"
#include "opusone_assimp.cpp"
#include "opusone_render.cpp"
#include "opusone_animation.cpp"
#include "opusone_immtext.cpp"
