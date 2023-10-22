#include "opusone_common.h"
#include "opusone_platform.h"
#include "opusone.h"
#include "opusone_math.h"
#include "opusone_linmath.h"
#include "opusone_assimp.h"
#include "opusone_render.h"
#include "opusone_animation.h"
#include "opusone_immtext.h"
#include "opusone_collision.h"
#include "opusone_entity.h"

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

        GameState->Camera.Position = Vec3(0.0f, 0.1f, 5.0f);
        GameState->Camera.Yaw = 270.0f;
        GameState->Camera.Pitch = -30.0f;
        GameState->Camera.ThirdPersonRadius = 5.0f;
        GameState->Camera.IsThirdPerson = true;

        GameState->EntityTypeSpecs = MemoryArena_PushArray(&GameState->WorldArena, EntityType_Count, entity_type_spec);
        for (u32 EntityType = EntityType_None + 1;
             EntityType < EntityType_Count;
             ++EntityType)
        {
            entity_type_spec *Spec = GameState->EntityTypeSpecs + EntityType;

            switch (EntityType)
            {
                case EntityType_BoxRoom:
                {
                    Spec->ImportedModel = Assimp_LoadModel(&GameState->AssetArena, "resources/models/box_room_separate/BoxRoomSeparate.gltf");

                    // TODO: Pull full initialization of a collision geometry based on type and imported model into a function
                    {
                        Spec->CollisionType = COLLISION_TYPE_POLYHEDRON_SET;
                        Spec->CollisionGeometry = MemoryArena_PushStruct(&GameState->WorldArena, collision_geometry);
                        polyhedron_set *PolyhedronSet = &Spec->CollisionGeometry->PolyhedronSet;
                        PolyhedronSet->PolyhedronCount = Spec->ImportedModel->MeshCount;
                        PolyhedronSet->Polyhedra = MemoryArena_PushArray(&GameState->WorldArena, PolyhedronSet->PolyhedronCount, polyhedron);
                    
                        imported_mesh *ImportedMeshCursor = Spec->ImportedModel->Meshes;
                        polyhedron *PolyhedronCursor = PolyhedronSet->Polyhedra;
                        for (u32 MeshIndex = 0;
                             MeshIndex < Spec->ImportedModel->MeshCount;
                             ++MeshIndex, ++ImportedMeshCursor, ++PolyhedronCursor)
                        {
                            ComputePolyhedronFromVertices(&GameState->WorldArena, &GameState->TransientArena,
                                                          ImportedMeshCursor->VertexPositions, ImportedMeshCursor->VertexCount,
                                                          ImportedMeshCursor->Indices, ImportedMeshCursor->IndexCount,
                                                          PolyhedronCursor);
                        }
                    }
                } break;

                case EntityType_Player:
                case EntityType_Enemy:
                {
                    // TODO: I think it's asset importer's responsibility to be a little smarter, and not reload same models.
                    // Will address redundant (e.g. player and enemy has same model, but different in other parts of the spec) loads later.
                    Spec->ImportedModel = Assimp_LoadModel(&GameState->AssetArena, "resources/models/adam/adam_new.gltf");

                    // TODO: This info should come from the importer later
                    f32 AdamHeight = 1.8412f;
                    f32 AdamHalfHeight = AdamHeight * 0.5f;

                    {
                        Spec->CollisionType = COLLISION_TYPE_AABB;
                        Spec->CollisionGeometry = MemoryArena_PushStruct(&GameState->WorldArena, collision_geometry);
                        Spec->CollisionGeometry->AABB = {};
                        Spec->CollisionGeometry->AABB.Center = Vec3(0, AdamHalfHeight, 0);
                        Spec->CollisionGeometry->AABB.Extents = Vec3 (0.3f, AdamHalfHeight, 0.3f);
                    }
                } break;

                case EntityType_Thing:
                {
                    Spec->ImportedModel = Assimp_LoadModel(&GameState->AssetArena, "resources/models/complex_animation_keys/AnimationStudy2c.gltf");

                    {
                        Spec->CollisionType = COLLISION_TYPE_NONE;
                        Spec->CollisionGeometry = 0;
                    }
                } break;

                case EntityType_Container:
                {
                    Spec->ImportedModel = Assimp_LoadModel(&GameState->AssetArena, "resources/models/container/Container.gltf");

                    {
                        Spec->CollisionType = COLLISION_TYPE_POLYHEDRON_SET;
                        Spec->CollisionGeometry = MemoryArena_PushStruct(&GameState->WorldArena, collision_geometry);
                        polyhedron_set *PolyhedronSet = &Spec->CollisionGeometry->PolyhedronSet;
                        PolyhedronSet->PolyhedronCount = Spec->ImportedModel->MeshCount;
                        PolyhedronSet->Polyhedra = MemoryArena_PushArray(&GameState->WorldArena, PolyhedronSet->PolyhedronCount, polyhedron);
                    
                        imported_mesh *ImportedMeshCursor = Spec->ImportedModel->Meshes;
                        polyhedron *PolyhedronCursor = PolyhedronSet->Polyhedra;
                        for (u32 MeshIndex = 0;
                             MeshIndex < Spec->ImportedModel->MeshCount;
                             ++MeshIndex, ++ImportedMeshCursor, ++PolyhedronCursor)
                        {
                            ComputePolyhedronFromVertices(&GameState->WorldArena, &GameState->TransientArena,
                                                          ImportedMeshCursor->VertexPositions, ImportedMeshCursor->VertexCount,
                                                          ImportedMeshCursor->Indices, ImportedMeshCursor->IndexCount,
                                                          PolyhedronCursor);
                        }
                    }
                } break;

                case EntityType_Snowman:
                {
                    Spec->ImportedModel = Assimp_LoadModel(&GameState->AssetArena, "resources/models/snowman/Snowman.gltf");

                    {
                        Spec->CollisionType = COLLISION_TYPE_POLYHEDRON_SET;
                        Spec->CollisionGeometry = MemoryArena_PushStruct(&GameState->WorldArena, collision_geometry);
                        polyhedron_set *PolyhedronSet = &Spec->CollisionGeometry->PolyhedronSet;
                        PolyhedronSet->PolyhedronCount = Spec->ImportedModel->MeshCount;
                        PolyhedronSet->Polyhedra = MemoryArena_PushArray(&GameState->WorldArena, PolyhedronSet->PolyhedronCount, polyhedron);
                    
                        imported_mesh *ImportedMeshCursor = Spec->ImportedModel->Meshes;
                        polyhedron *PolyhedronCursor = PolyhedronSet->Polyhedra;
                        for (u32 MeshIndex = 0;
                             MeshIndex < Spec->ImportedModel->MeshCount;
                             ++MeshIndex, ++ImportedMeshCursor, ++PolyhedronCursor)
                        {
                            ComputePolyhedronFromVertices(&GameState->WorldArena, &GameState->TransientArena,
                                                          ImportedMeshCursor->VertexPositions, ImportedMeshCursor->VertexCount,
                                                          ImportedMeshCursor->Indices, ImportedMeshCursor->IndexCount,
                                                          PolyhedronCursor);
                        }
                    }
                } break;

                case EntityType_ObstacleCourse:
                {
                    Spec->ImportedModel = Assimp_LoadModel(&GameState->AssetArena, "resources/models/collisions/Collisions.gltf");

                    {
                        Spec->CollisionType = COLLISION_TYPE_POLYHEDRON_SET;
                        Spec->CollisionGeometry = MemoryArena_PushStruct(&GameState->WorldArena, collision_geometry);
                        polyhedron_set *PolyhedronSet = &Spec->CollisionGeometry->PolyhedronSet;
                        PolyhedronSet->PolyhedronCount = Spec->ImportedModel->MeshCount;
                        PolyhedronSet->Polyhedra = MemoryArena_PushArray(&GameState->WorldArena, PolyhedronSet->PolyhedronCount, polyhedron);
                    
                        imported_mesh *ImportedMeshCursor = Spec->ImportedModel->Meshes;
                        polyhedron *PolyhedronCursor = PolyhedronSet->Polyhedra;
                        for (u32 MeshIndex = 0;
                             MeshIndex < Spec->ImportedModel->MeshCount;
                             ++MeshIndex, ++ImportedMeshCursor, ++PolyhedronCursor)
                        {
                            ComputePolyhedronFromVertices(&GameState->WorldArena, &GameState->TransientArena,
                                                          ImportedMeshCursor->VertexPositions, ImportedMeshCursor->VertexCount,
                                                          ImportedMeshCursor->Indices, ImportedMeshCursor->IndexCount,
                                                          PolyhedronCursor);
                        }
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
        // NOTE: Skinned and static mesh render units
        u32 MaterialsForStaticCount = 0;
        u32 MeshesForStaticCount = 0;
        u32 VerticesForStaticCount = 0;
        u32 IndicesForStaticCount = 0;
        u32 MaterialsForSkinnedCount = 0;
        u32 MeshesForSkinnedCount = 0;
        u32 VerticesForSkinnedCount = 0;
        u32 IndicesForSkinnedCount = 0;
        
        for (u32 EntityType = EntityType_None + 1;
             EntityType < EntityType_Count;
             ++EntityType)
        {
            entity_type_spec *Spec = GameState->EntityTypeSpecs + EntityType;
            imported_model *ImportedModel = Spec->ImportedModel;

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

        // NOTE: Debug draw render unit
        {
            u32 MaxMarkers = 1024 * 3;
            u32 MaxVertices = 16384 * 3;
            u32 MaxIndices = 32768 * 3;
            
            InitializeRenderUnit(&GameState->DebugDrawRenderUnit, VERT_SPEC_DEBUG_DRAW,
                                 0, MaxMarkers, MaxVertices, MaxIndices, true, false, DebugDrawShader,
                                 &GameState->RenderArena);
        }

        // NOTE: ImmText render unit
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
        for (u32 EntityType = EntityType_None + 1;
             EntityType < EntityType_Count;
             ++EntityType)
        {
            entity_type_spec *Spec = GameState->EntityTypeSpecs + EntityType;
            imported_model *ImportedModel = Spec->ImportedModel;

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

            Spec->RenderUnit = RenderUnit;
            Spec->BaseMeshID = RenderUnit->MarkerCount;
            Spec->MeshCount = ImportedModel->MeshCount;

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
        // NOTE: Add entities
        //
        GameState->EntityCount = 0;

        AddEntity(GameState, EntityType_BoxRoom, Vec3(0), Quat(), Vec3(1));
        AddEntity(GameState, EntityType_BoxRoom, Vec3(20,0,0), Quat(Vec3(0,1,1), ToRadiansF(45)), Vec3(0.5f,1,2));
        AddEntity(GameState, EntityType_BoxRoom, Vec3(0,0,-30), Quat(Vec3(1), ToRadiansF(160)), Vec3(1,1,5));
        AddEntity(GameState, EntityType_Enemy, Vec3(-1,0,0), Quat(), Vec3(1));
        AddEntity(GameState, EntityType_Enemy, Vec3(0), Quat(), Vec3(1));
        AddEntity(GameState, EntityType_Enemy, Vec3(1,0,0), Quat(), Vec3(1));
        AddEntity(GameState, EntityType_Enemy, Vec3(2,0,0), Quat(), Vec3(1));
        AddEntity(GameState, EntityType_Thing, Vec3(0,0,-3), Quat(), Vec3(0.3f));
        AddEntity(GameState, EntityType_Container, Vec3(-3,-0.2f,3), Quat(Vec3(0,0,1), ToRadiansF(5)), Vec3(1));
        AddEntity(GameState, EntityType_Container, Vec3(-3,0,-3), Quat(Vec3(0,1,0), ToRadiansF(45)), Vec3(1));
        AddEntity(GameState, EntityType_Container, Vec3(3,0,-3), Quat(Vec3(1,1,0), ToRadiansF(60)), Vec3(1));
        AddEntity(GameState, EntityType_Snowman, Vec3(5,0,5), Quat(), Vec3(3));
        // GameState->PlayerEntity = AddEntity(GameState, EntityType_Player, Vec3(0,2,5), Quat(), Vec3(1));
        // GameState->PlayerEntity = AddEntity(GameState, EntityType_Player, Vec3(-10.9f,-13,0.11f), Quat(), Vec3(1));
        GameState->PlayerEntity = AddEntity(GameState, EntityType_Player, Vec3(0,1,100), Quat(), Vec3(1));
        AddEntity(GameState, EntityType_BoxRoom, Vec3(-40,-30,0), Quat(Vec3(0,0,1), ToRadiansF(30)), Vec3(5,1,1));
        AddEntity(GameState, EntityType_ObstacleCourse, Vec3(0,0,100), Quat(), Vec3(1));

        // NOTE: Misc data that probably should be set somewhere else
        GameState->PlayerEyeHeight = 1.7f;
        GameState->PlayerSpecJumpVelocity = 100.0f;
        GameState->PlayerSpecAccelerationValue = 300.0f;
        GameState->PlayerSpecGravityValue = -2.0f;
        GameState->PlayerSpecDragValue = 50.0f;
        GameState->GroundContact = CollisionContact(); GameState->GroundContact.Normal = Vec3(0,1,0);
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
        GameState->Camera.IsThirdPerson = !GameState->Camera.IsThirdPerson;
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
    RequestedControls->PlayerJump = Platform_KeyJustPressed(GameInput, SDL_SCANCODE_SPACE) || Platform_KeyJustPressed(GameInput, SDL_SCANCODE_BACKSPACE);
    
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
    f32 CameraDeltaYaw = 0.0f;
    f32 CameraDeltaPitch = 0.0f;
    f32 CameraDeltaRadius = 0.0f;
    if (RequestedControls->CameraLeft) CameraDeltaYaw += 1.0f;
    if (RequestedControls->CameraRight) CameraDeltaYaw -= 1.0f;
    if (RequestedControls->CameraUp) CameraDeltaPitch += 1.0f;
    if (RequestedControls->CameraDown) CameraDeltaPitch -= 1.0f;
    if (RequestedControls->CameraForward) CameraDeltaRadius -= 1.0f;
    if (RequestedControls->CameraBackward) CameraDeltaRadius += 1.0f;
    CameraDeltaYaw *= KeyboardLookSensitivity * GameInput->DeltaTime;
    CameraDeltaPitch *= KeyboardLookSensitivity * GameInput->DeltaTime;
    CameraDeltaRadius *= KeyboardDollySensitivity * GameInput->DeltaTime;
    if (GameState->MouseControlledTemp)
    {
        CameraDeltaYaw += MouseLookSensitivity * (f32) (-GameInput->MouseDeltaX);
        CameraDeltaPitch += MouseLookSensitivity * (f32) (-GameInput->MouseDeltaY);
    }
    CameraSetDeltaOrientation(&GameState->Camera, CameraDeltaYaw, CameraDeltaPitch, CameraDeltaRadius);
    
    // NOTE: Updates requested by controls (player)
    entity *Player = GameState->PlayerEntity;
    entity_type_spec *PlayerSpec = GameState->EntityTypeSpecs + EntityType_Player;

    if (RequestedControls->CameraIsIndependent)
    {
        if (!GameState->CameraWillReset)
        {
            GameState->CameraYawToResetTo = GameState->Camera.Yaw;
            GameState->CameraPitchToResetTo = GameState->Camera.Pitch;
            GameState->CameraWillReset = true;
        }
    }
    else
    {
        if (GameState->CameraWillReset)
        {
            CameraSetOrientation(&GameState->Camera, GameState->CameraYawToResetTo, GameState->CameraPitchToResetTo);
            GameState->CameraWillReset = false;
        }
        
        Player->WorldPosition.R = CameraGetYawQuat(&GameState->Camera);
    }

    vec3 PlayerAcceleration = {};
    if (RequestedControls->PlayerForward) PlayerAcceleration.Z -= 1.0f;
    if (RequestedControls->PlayerBackward) PlayerAcceleration.Z += 1.0f;
    if (RequestedControls->PlayerLeft) PlayerAcceleration.X -= 1.0f;
    if (RequestedControls->PlayerRight) PlayerAcceleration.X += 1.0f;
    if (GameState->GravityDisabledTemp)
    {
        if (RequestedControls->PlayerDown) PlayerAcceleration.Y -= 1.0f;
        if (RequestedControls->PlayerUp) PlayerAcceleration.Y += 1.0f;
    }
    // if (!IsZeroVector(PlayerAcceleration))
    // {
    //     PlayerAcceleration = GameState->PlayerSpecAccelerationValue * RotateVecByQuatSlow(VecNormalize(PlayerAcceleration), Player->WorldPosition.R);
    // }

    if (!GameState->GravityDisabledTemp)
    {
        // NOTE: Gravity
        PlayerAcceleration.Y = GameState->PlayerSpecGravityValue;
        
        // if (RequestedControls->PlayerJump && GameState->PlayerOnGround)
        // {
        //     GameState->PlayerVelocity.Y += GameState->PlayerSpecJumpVelocity;
        // }
    }

    if (Platform_KeyJustPressed(GameInput, SDL_SCANCODE_B))
    {
        Breakpoint;
    }
        
    {
        vec3 GroundNormal = GameState->GroundContact.Normal;
        DD_DrawQuickVector(Player->WorldPosition.P, Player->WorldPosition.P + VecNormalize(GroundNormal), Vec3(0,1,0));
        vec3 PlayerRight = RotateVecByQuatSlow(Vec3(-1,0,0), Player->WorldPosition.R);
        vec3 GroundForward = VecNormalize(VecCross(GroundNormal, PlayerRight));
        // DD_DrawQuickVector(Player->WorldPosition.P, Player->WorldPosition.P + GroundForward, Vec3(0,0,1));
        vec3 GroundRight = VecCross(GroundForward, GroundNormal);
        // DD_DrawQuickVector(Player->WorldPosition.P, Player->WorldPosition.P + GroundRight, Vec3(1,0,0));
        // mat3 GroundTransform = Mat3(GroundRight, GroundNormal, GroundForward);

        vec3 ForwardComponent = PlayerAcceleration.Z * GroundForward;
        // DD_DrawQuickVector(Player->WorldPosition.P, Player->WorldPosition.P + ForwardComponent, Vec3(0,0,1));
        vec3 RightComponent = PlayerAcceleration.X * GroundRight;
        // DD_DrawQuickVector(Player->WorldPosition.P, Player->WorldPosition.P + RightComponent, Vec3(1,0,0));
        vec3 UpComponent = PlayerAcceleration.Y * GroundNormal;
        // DD_DrawQuickVector(Player->WorldPosition.P, Player->WorldPosition.P + UpComponent, Vec3(0,1,0));
        
        PlayerAcceleration = -ForwardComponent + RightComponent + UpComponent;
    }

    PlayerAcceleration *= GameState->PlayerSpecAccelerationValue;

    // NOTE: Integration for player movement
    PlayerAcceleration -= GameState->PlayerSpecDragValue * GameState->PlayerVelocity; // NOTE: Drag
    
    vec3 PlayerTranslation = (0.5f * PlayerAcceleration * GameInput->DeltaTime * GameInput->DeltaTime +
                              GameState->PlayerVelocity * GameInput->DeltaTime);

     DD_DrawQuickAABox(Player->WorldPosition.P + PlayerTranslation + PlayerSpec->CollisionGeometry->AABB.Center,
                      PlayerSpec->CollisionGeometry->AABB.Extents,
                      Vec3(1));
    
    GameState->PlayerVelocity += PlayerAcceleration * GameInput->DeltaTime;

    ImmText_DrawQuickString(SimpleStringF("Player velocity: {%0.3f,%0.3f,%0.3f}",
                                          GameState->PlayerVelocity.X, GameState->PlayerVelocity.Y, GameState->PlayerVelocity.Z).D);
    ImmText_DrawQuickString(SimpleStringF("Player position: {%0.3f,%0.3f,%0.3f}",
                                          Player->WorldPosition.P.X, Player->WorldPosition.P.Y, Player->WorldPosition.P.Z).D);

    // NOTE: Entity AI updates
    // ....

    // NOTE: Process entity movement
    u32 CollisionIterations = COLLISION_ITERATIONS + 1; // NOTE: Last iteration is only to check if can move after prev iteration
    b32 TranslationIsSafe = false;
    b32 FoundGroundThisFrame = false;
    collision_contact GroundContactThisFrame = CollisionContact();
    GroundContactThisFrame.Normal = Vec3(0,1,0);

    u32 IterationIndex;
    for (IterationIndex = 0;
         IterationIndex < CollisionIterations;
         ++IterationIndex)
    {
        // TODO: Make this proper
        if (IgnoreCollisions)
        {
            TranslationIsSafe = true;
            break;
        }

        // if (Platform_KeyJustPressed(GameInput, SDL_SCANCODE_B) && IterationIndex == 0)
        // {
        //     Breakpoint;
        // }

        collision_contact ClosestContacts[COLLISION_CONTACTS_PER_ITERATION] = {};
        CheckCollisionsForEntity(GameState, Player, PlayerTranslation, ArrayCount(ClosestContacts), ClosestContacts);

        collision_contact ContactToResolve = CollisionContact();
        b32 IsContactToResolveGround = false;
        {
            b32 FoundSameGroundContact = false;
            for (u32 ContactIndex = 0;
                 ContactIndex < ArrayCount(ClosestContacts);
                 ++ContactIndex)
            { 
                collision_contact *Contact = ClosestContacts + ContactIndex;
                if (IsGroundContact(Contact) && GameState->PlayerOnGround && AreContactsSame(Contact, &GameState->GroundContact))
                {
                    ContactToResolve = *Contact;
                    FoundSameGroundContact = true;
                    IsContactToResolveGround = true;
                    break;
                }
            }

            if (!FoundSameGroundContact)
            {
                for (u32 ContactIndex = 0;
                     ContactIndex < ArrayCount(ClosestContacts);
                     ++ContactIndex)
                {
                    collision_contact *Contact = ClosestContacts + ContactIndex;
                    if (Contact->Entity)
                        // VecDot(Contact->Normal, PlayerTranslation) < -FLT_EPSILON)
                    {
                        ContactToResolve = *Contact;
                        IsContactToResolveGround = IsGroundContact(Contact);
                        break;
                    }
                }
            }
        }

        // NOTE: if no valid closest contact, safe to move; if there is a valid contact, try to resolve it and continue with the next iteration
        if (!ContactToResolve.Entity)
        {
            TranslationIsSafe = true;
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

            vec3 CollidingTranslationComponent;
            vec3 CollidingVelocityComponent;
            if (IsContactToResolveGround)
            {
                FoundGroundThisFrame = true;
                GroundContactThisFrame = ContactToResolve;

                CollidingTranslationComponent = AbsF(VecDot(ContactToResolve.Normal, PlayerTranslation)) * ContactToResolve.Normal;
                CollidingVelocityComponent = AbsF(VecDot(ContactToResolve.Normal, GameState->PlayerVelocity)) * ContactToResolve.Normal;
            }
            else
            {
                if (GameState->PlayerOnGround)
                {
                    ContactToResolve.Normal.Y = 0;
                    CollidingTranslationComponent = AbsF(VecDot(ContactToResolve.Normal, PlayerTranslation)) * ContactToResolve.Normal;
                    CollidingTranslationComponent.Y = -PlayerTranslation.Y;
                    CollidingVelocityComponent = AbsF(VecDot(ContactToResolve.Normal, GameState->PlayerVelocity)) * ContactToResolve.Normal;
                    CollidingVelocityComponent.Y = -GameState->PlayerVelocity.Y;
                }
                else
                {
                    CollidingTranslationComponent = AbsF(VecDot(ContactToResolve.Normal, PlayerTranslation)) * ContactToResolve.Normal;
                    CollidingVelocityComponent = AbsF(VecDot(ContactToResolve.Normal, GameState->PlayerVelocity)) * ContactToResolve.Normal;
                }
            }
            
            PlayerTranslation += CollidingTranslationComponent;
            GameState->PlayerVelocity += CollidingVelocityComponent;

            // DD_DrawQuickVector(Player->WorldPosition.P, Player->WorldPosition.P + ClosestContact.Normal,
            //                    Vec3(IterationIndex == 0, IterationIndex == 1, IterationIndex == 2));

        }
    }

    f32 ActualTranslationLengthThisFrame = 0;
    if (TranslationIsSafe)
    {
        GameState->PlayerOnGround = FoundGroundThisFrame;
        GameState->GroundContact = GroundContactThisFrame;
        Player->WorldPosition.P += PlayerTranslation;
        ActualTranslationLengthThisFrame = VecLength(PlayerTranslation);
    }

    CameraSetWorldPosition(&GameState->Camera, Player->WorldPosition.P + Vec3(0, GameState->PlayerEyeHeight,0));

    DD_DrawQuickVector(Player->WorldPosition.P, Player->WorldPosition.P + VecNormalize(PlayerTranslation), Vec3(1));

    ImmText_DrawQuickString(SimpleStringF("Actual player translation per frame: %0.16f[Safe=%d;Iter=%d;Ground=%d]",
                                          ActualTranslationLengthThisFrame, TranslationIsSafe, IterationIndex, GameState->PlayerOnGround).D);


    DD_DrawQuickAABox(Player->WorldPosition.P + PlayerSpec->CollisionGeometry->AABB.Center,
                      PlayerSpec->CollisionGeometry->AABB.Extents,
                      GameState->GravityDisabledTemp ? Vec3(0,0,1) : Vec3(0,1,0));

    // NOTE: Mouse picking
    vec3 CameraFront = CameraGetFront(&GameState->Camera);
    
    vec3 RayPosition = Player->WorldPosition.P + Vec3(0,GameState->PlayerEyeHeight,0);
    vec3 RayDirection = CameraFront;
    f32 RayTMin = FLT_MAX;
    u32 ClosestEntityIndex = 0;
    
    for (u32 EntityIndex = 0;
         EntityIndex < GameState->EntityCount;
         ++EntityIndex)
    {
        entity *Entity = GameState->Entities + EntityIndex;
        if (Entity != Player)
        {
            entity_type_spec *Spec = GameState->EntityTypeSpecs + Entity->Type;

            switch (Spec->CollisionType)
            {
                case COLLISION_TYPE_AABB:
                {
                    Assert(Spec->CollisionGeometry);
                    aabb *AABB = &Spec->CollisionGeometry->AABB;

                    f32 ThisTMin;
                    b32 IsRayIntersecting =
                        IntersectRayAABB(RayPosition, RayDirection, AABB->Center + Player->WorldPosition.P, AABB->Extents, &ThisTMin, 0);

                    if (IsRayIntersecting && ThisTMin < RayTMin)
                    {
                        RayTMin = ThisTMin;
                        ClosestEntityIndex = EntityIndex;
                    }
                } break;

                default:
                {
                    continue;
                } break;
            }
        }
    }

    if (ClosestEntityIndex > 0)
    {
        ImmText_DrawQuickString(SimpleStringF("Looking at entity#%d", ClosestEntityIndex).D);
    }

    if (!GameState->Camera.IsThirdPerson)
    {
        DD_DrawPoint(&GameState->DebugDrawRenderUnit, GameState->Camera.Position + CameraGetFront(&GameState->Camera), Vec3(1), 4);
    }
    
    for (u32 EntityIndex = 0;
         EntityIndex < GameState->EntityCount;
         ++EntityIndex)
    {
        entity *Entity = GameState->Entities + EntityIndex;

        if (Entity->AnimationState)
        {
            f32 DeltaTicks = (f32) (GameInput->DeltaTime * (Entity->AnimationState->Animation->TicksPerSecond));
            Entity->AnimationState->CurrentTicks += DeltaTicks;
            if (Entity->AnimationState->CurrentTicks >= Entity->AnimationState->Animation->TicksDuration)
            {
                Entity->AnimationState->CurrentTicks -= Entity->AnimationState->Animation->TicksDuration;
            }
        }
    }

    vec3 ViewPosition = CameraGetTruePosition(&GameState->Camera);
    OpenGL_SetUniformVec3F(GameState->StaticRenderUnit.ShaderID, "ViewPosition", (f32 *) &ViewPosition, true);
    OpenGL_SetUniformVec3F(GameState->SkinnedRenderUnit.ShaderID, "ViewPosition", (f32 *) &ViewPosition, true);

    // NOTE: Debug collision polyhedra
#if 0
    for (u32 EntityIndex = 0;
         EntityIndex < GameState->EntityCount;
         ++EntityIndex)
    {
        entity *Entity = GameState->Entities + EntityIndex;
        entity_type_spec *Spec = GameState->EntityTypeSpecs + Entity->Type;

        if (Spec->CollisionType == COLLISION_TYPE_POLYHEDRON_SET)
        {
            Assert(Spec->CollisionGeometry);
            
            polyhedron_set *PolyhedronSet = CopyAndTransformPolyhedronSet(&GameState->TransientArena,
                                                                          &Spec->CollisionGeometry->PolyhedronSet,
                                                                          Entity->WorldPosition.P,
                                                                          Entity->WorldPosition.R,
                                                                          Entity->WorldPosition.S);

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
            
            mat4 ViewMat = CameraGetViewMat(&GameState->Camera);
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
                        entity *Entity = Mesh->EntityInstances[InstanceSlotIndex];
                        if (Entity && (Entity != Player || GameState->Camera.IsThirdPerson))
                        {
                            if (Entity->IsInvisible)
                            {
                                continue;
                            }

                            mat4 ModelTransform = WorldPositionTransform(&Entity->WorldPosition);

                            if (Entity->AnimationState)
                            {
                                imported_armature *Armature = Entity->AnimationState->Armature;
                                u32 BoneCount = Entity->AnimationState->Armature->BoneCount;
                        
                                MemoryArena_Freeze(&GameState->RenderArena);
                                mat4 *BoneTransforms = MemoryArena_PushArray(&GameState->RenderArena,
                                                                             BoneCount,
                                                                             mat4);
                                                                     
                                ComputeTransformsForAnimation(Entity->AnimationState, BoneTransforms, BoneCount);
                        
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
#include "opusone_entity.cpp"
