#include "opusone_common.h"
#include "opusone_platform.h"
#include "opusone.h"
#include "opusone_opengl.h"
#include "opusone_math.h"
#include "opusone_linmath.h"
#include "opusone_vertspec.h"

#include "opusone_camera.cpp"
#include "opusone_assimp.cpp"

#include <cstdio>
#include <glad/glad.h>

inline mat4
GetTransformForWorldObjectInstance(world_object_instance *Instance)
{
    // TODO: This can probably be optimized a lot
    mat4 Translation = Mat4GetTranslation(Instance->Position);
    mat4 Rotation = Mat4GetRotationFromQuat(Instance->Rotation);
    mat4 Scale = Mat4GetScale(Instance->Scale);
    
    mat4 Result = Translation * Rotation * Scale;
    return Result;
}

void
GameUpdateAndRender(game_input *GameInput, game_memory *GameMemory, b32 *GameShouldQuit)
{
    game_state *GameState = (game_state *) GameMemory->Storage;
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

        GameState->StaticRenderUnit.ShaderID = 0;
            // OpenGL_BuildShaderProgram("resources/shaders/StaticMesh.vs", "resources/shaders/Basic.fs");
        OpenGL_SetUniformInt(GameState->StaticRenderUnit.ShaderID, "DiffuseMap", 0, true);
        OpenGL_SetUniformInt(GameState->StaticRenderUnit.ShaderID, "SpecularMap", 1, false);
        OpenGL_SetUniformInt(GameState->StaticRenderUnit.ShaderID, "EmissionMap", 2, false);
        OpenGL_SetUniformInt(GameState->StaticRenderUnit.ShaderID, "NormalMap", 3, false);

        GameState->SkinnedRenderUnit.ShaderID = 0;
            // OpenGL_BuildShaderProgram("resources/shaders/SkinnedMesh.vs", "resources/shaders/Basic.fs");
        OpenGL_SetUniformInt(GameState->SkinnedRenderUnit.ShaderID, "DiffuseMap", 0, true);
        OpenGL_SetUniformInt(GameState->SkinnedRenderUnit.ShaderID, "SpecularMap", 1, false);
        OpenGL_SetUniformInt(GameState->SkinnedRenderUnit.ShaderID, "EmissionMap", 2, false);
        OpenGL_SetUniformInt(GameState->SkinnedRenderUnit.ShaderID, "NormalMap", 3, false);

        InitializeCamera(GameState, vec3 { 0.0f, 1.7f, 5.0f }, 0.0f, 90.0f, 5.0f, CAMERA_CONTROL_MOUSE);

        //
        // NOTE: Load models using assimp
        //
        simple_string ModelPaths[] = {
            SimpleString("resources/models/box_room/BoxRoom.gltf"),
            SimpleString("resources/models/adam/adam.gltf")
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
            Blueprint->ImportedModel = Assimp_LoadModel(&GameState->AssetArena, ModelPaths[BlueprintIndex-1].D);
        }

        //
        // NOTE: Calculate how much space to allocate for render data
        //
        GameState->StaticRenderUnit.VertexCount = 0;
        GameState->StaticRenderUnit.MaxVertexCount = 0;
        GameState->StaticRenderUnit.IndexCount = 0;
        GameState->StaticRenderUnit.MaxIndexCount = 0;
        GameState->StaticRenderUnit.MaterialCount = 1; // Material0 is no material
        GameState->StaticRenderUnit.MaxMaterialCount = 1; // Material0 is no material
        GameState->StaticRenderUnit.MeshCount = 0;
        GameState->StaticRenderUnit.MaxMeshCount = 0;

        GameState->SkinnedRenderUnit.VertexCount = 0;
        GameState->SkinnedRenderUnit.MaxVertexCount = 0;
        GameState->SkinnedRenderUnit.IndexCount = 0;
        GameState->SkinnedRenderUnit.MaxIndexCount = 0;
        GameState->SkinnedRenderUnit.MaterialCount = 1; // Material0 is no material
        GameState->SkinnedRenderUnit.MaxMaterialCount = 1; // Material0 is no material
        GameState->SkinnedRenderUnit.MeshCount = 0;
        GameState->SkinnedRenderUnit.MaxMeshCount = 0;
        
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

            RenderUnit->MaxMaterialCount += ImportedModel->MaterialCount;
            RenderUnit->MaxMeshCount += ImportedModel->MeshCount;
            for (u32 MeshIndex = 0;
                 MeshIndex < ImportedModel->MeshCount;
                 ++MeshIndex)
            {
                imported_mesh *ImportedMesh = ImportedModel->Meshes + MeshIndex;

                RenderUnit->MaxVertexCount += ImportedMesh->VertexCount;
                RenderUnit->MaxIndexCount += ImportedMesh->IndexCount;
            }
        }

        //
        // NOTE: Allocate memory for render units
        //
        {
            render_unit *RenderUnit = &GameState->StaticRenderUnit;

            SetVertSpec(&RenderUnit->VertSpec, VERTS_STATIC_MESH);

            OpenGL_PrepareVertexData(RenderUnit, true);

            RenderUnit->Materials = MemoryArena_PushArray(&GameState->RenderArena,
                                                          RenderUnit->MaxMaterialCount,
                                                          render_data_material);
            RenderUnit->Meshes = MemoryArena_PushArray(&GameState->RenderArena,
                                                       RenderUnit->MaxMeshCount,
                                                       render_data_mesh);
        }
        {
            render_unit *RenderUnit = &GameState->SkinnedRenderUnit;

            SetVertSpec(&RenderUnit->VertSpec, VERTS_SKINNED_MESH);

            OpenGL_PrepareVertexData(RenderUnit, false);

            RenderUnit->Materials = MemoryArena_PushArray(&GameState->RenderArena,
                                                          RenderUnit->MaxMaterialCount,
                                                          render_data_material);
            RenderUnit->Meshes = MemoryArena_PushArray(&GameState->RenderArena,
                                                       RenderUnit->MaxMeshCount,
                                                       render_data_mesh);
        }
        //
        // NOTE: Process import data into the render units
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

            Assert(RenderUnit->MaterialCount + ImportedModel->MaterialCount <= RenderUnit->MaxMaterialCount);
            for (u32 MaterialIndex = 0;
                 MaterialIndex < ImportedModel->MaterialCount;
                 ++MaterialIndex)
            {
                imported_material *ImportedMaterial = ImportedModel->Materials + MaterialIndex;
                render_data_material *Material = RenderUnit->Materials + RenderUnit->MaterialCount + MaterialIndex;
                
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

            Assert(RenderUnit->MeshCount + ImportedModel->MeshCount <= RenderUnit->MaxMeshCount);
            for (u32 MeshIndex = 0;
                 MeshIndex < ImportedModel->MeshCount;
                 ++MeshIndex)
            {
                imported_mesh *ImportedMesh = ImportedModel->Meshes + MeshIndex;
                render_data_mesh *Mesh = RenderUnit->Meshes + RenderUnit->MeshCount + MeshIndex;

                Mesh->BaseVertexIndex = RenderUnit->VertexCount;
                Mesh->StartingIndex = RenderUnit->IndexCount;
                Mesh->IndexCount = ImportedMesh->IndexCount;
                Mesh->MaterialID = RenderUnit->MaterialCount + ImportedMesh->MaterialID;

                if (ImportedModel->Armature)
                {
                    void *AttribData[] = {
                        ImportedMesh->VertexPositions, ImportedMesh->VertexTangents, ImportedMesh->VertexBitangents,
                        ImportedMesh->VertexNormals, ImportedMesh->VertexColors, ImportedMesh->VertexUVs, 0, 0
                    };
                    OpenGL_SubVertexData(RenderUnit, AttribData, ImportedMesh->Indices, ImportedMesh->VertexCount, ImportedMesh->IndexCount);               }
                else
                {
                    void *AttribData[] = {
                        ImportedMesh->VertexPositions, ImportedMesh->VertexTangents, ImportedMesh->VertexBitangents,
                        ImportedMesh->VertexNormals, ImportedMesh->VertexColors, ImportedMesh->VertexUVs
                    };
                    OpenGL_SubVertexData(RenderUnit, AttribData, ImportedMesh->Indices, ImportedMesh->VertexCount, ImportedMesh->IndexCount);
                }
            }

            Blueprint->RenderUnit = &GameState->StaticRenderUnit;
            Blueprint->BaseMeshID = RenderUnit->MeshCount;
            Blueprint->MeshCount = ImportedModel->MeshCount;

            RenderUnit->MaterialCount += ImportedModel->MaterialCount;
            RenderUnit->MeshCount += ImportedModel->MeshCount;
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
            world_object_instance { 1, vec3 { 0.0f, 0.0f, 0.0f }, QuatGetNeutral(), vec3 { 1.0f, 1.0f, 1.0f } },
            world_object_instance { 2, vec3 { 0.0f, 0.0f, 0.0f }, QuatGetNeutral(), vec3 { 1.0f, 1.0f, 1.0f } },
#if 0
            world_object_instance { 1, vec3 { 20.0f, 0.0f, 0.0f }, QuatGetRotationAroundAxis(vec3 { 0.0f, 1.0f, 1.0f }, ToRadiansF(45.0f)), vec3 { 0.5f, 1.0f, 2.0f } },
            world_object_instance { 1, vec3 { 0.0f, 0.0f, -30.0f }, QuatGetRotationAroundAxis(vec3 { 1.0f, 1.0f, 1.0f }, ToRadiansF(160.0f)), vec3 { 1.0f, 1.0f, 5.0f } },
            world_object_instance { 2, vec3 { 0.0f, 0.0f, 1.0f }, QuatGetNeutral(), vec3 { 1.0f, 1.0f, 1.0f } },
            world_object_instance { 2, vec3 { 0.0f, 0.0f, 2.0f }, QuatGetNeutral(), vec3 { 1.0f, 1.0f, 1.0f } },
            world_object_instance { 2, vec3 { 0.0f, 0.0f, 3.0f }, QuatGetNeutral(), vec3 { 1.0f, 1.0f, 1.0f } },

            world_object_instance { 2, vec3 { 1.0f, 0.0f, 0.0f }, QuatGetNeutral(), vec3 { 1.0f, 1.0f, 1.0f } },
            world_object_instance { 2, vec3 { 1.0f, 0.0f, 1.0f }, QuatGetNeutral(), vec3 { 1.0f, 1.0f, 1.0f } },
            world_object_instance { 2, vec3 { 1.0f, 0.0f, 2.0f }, QuatGetNeutral(), vec3 { 1.0f, 1.0f, 1.0f } },
            world_object_instance { 2, vec3 { 1.0f, 0.0f, 3.0f }, QuatGetNeutral(), vec3 { 1.0f, 1.0f, 1.0f } },

            world_object_instance { 2, vec3 { 2.0f, 0.0f, 0.0f }, QuatGetNeutral(), vec3 { 1.0f, 1.0f, 1.0f } },
            world_object_instance { 2, vec3 { 2.0f, 0.0f, 1.0f }, QuatGetNeutral(), vec3 { 1.0f, 1.0f, 1.0f } },
            world_object_instance { 2, vec3 { 2.0f, 0.0f, 2.0f }, QuatGetNeutral(), vec3 { 1.0f, 1.0f, 1.0f } },
            world_object_instance { 2, vec3 { 2.0f, 0.0f, 3.0f }, QuatGetNeutral(), vec3 { 1.0f, 1.0f, 1.0f } },

            world_object_instance { 2, vec3 { 3.0f, 0.0f, 0.0f }, QuatGetNeutral(), vec3 { 1.0f, 1.0f, 1.0f } },
            world_object_instance { 2, vec3 { 3.0f, 0.0f, 1.0f }, QuatGetNeutral(), vec3 { 1.0f, 1.0f, 1.0f } },
            world_object_instance { 2, vec3 { 3.0f, 0.0f, 2.0f }, QuatGetNeutral(), vec3 { 1.0f, 1.0f, 1.0f } },
            world_object_instance { 2, vec3 { 3.0f, 0.0f, 3.0f }, QuatGetNeutral(), vec3 { 1.0f, 1.0f, 1.0f } }
#endif
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

            render_unit *RenderUnit = Blueprint->RenderUnit;

            for (u32 MeshIndex = 0;
                 MeshIndex < Blueprint->MeshCount;
                 ++MeshIndex)
            {
                render_data_mesh *Mesh = RenderUnit->Meshes + Blueprint->BaseMeshID + MeshIndex;

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

    if (GameInput->CurrentKeyStates[SDL_SCANCODE_ESCAPE])
    {
        *GameShouldQuit = true;
    }

    UpdateCameraForFrame(GameState, GameInput);
    
    glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    OpenGL_UseShader(GameState->StaticRenderUnit.ShaderID);

    mat4 ProjectionMat = Mat4GetPerspecitveProjection(70.0f, (f32) GameInput->ScreenWidth / (f32) GameInput->ScreenHeight, 0.1f, 1000.0f);
    OpenGL_SetUniformMat4F(GameState->StaticRenderUnit.ShaderID, "Projection", (f32 *) &ProjectionMat, false);
    mat4 ViewMat = GetCameraViewMat(&GameState->Camera);
    OpenGL_SetUniformMat4F(GameState->StaticRenderUnit.ShaderID, "View", (f32 *) &ViewMat, false);

    // Render each render unit
    {
        render_unit *RenderUnit = &GameState->StaticRenderUnit;

        glBindVertexArray(RenderUnit->VAO);

        u32 PreviousMaterialID = 0;
        for (u32 MeshIndex = 0;
             MeshIndex < RenderUnit->MeshCount;
             ++MeshIndex)
        {
            render_data_mesh *Mesh = RenderUnit->Meshes + MeshIndex;

            if (PreviousMaterialID == 0 || Mesh->MaterialID != PreviousMaterialID)
            {
                PreviousMaterialID = Mesh->MaterialID;
                if (Mesh->MaterialID > 0)
                {
                    render_data_material *Material = RenderUnit->Materials + Mesh->MaterialID;
                    OpenGL_BindTexturesForMaterial(Material);
                }
            }

            for (u32 InstanceSlotIndex = 0;
                 InstanceSlotIndex < MAX_INSTANCES_PER_MESH;
                 ++InstanceSlotIndex)
            {
                u32 InstanceID = Mesh->InstanceIDs[InstanceSlotIndex];
                if (InstanceID > 0)
                {
                    world_object_instance *Instance = GameState->WorldObjectInstances + InstanceID;
                    mat4 ModelTransform = GetTransformForWorldObjectInstance(Instance);
                    OpenGL_SetUniformMat4F(RenderUnit->ShaderID, "Model", (f32 *) &ModelTransform, false);
                    // TODO: Should I treat indices as unsigned everywhere?
                    // TODO: Instanced draw?
                    glDrawElementsBaseVertex(GL_TRIANGLES,
                                             Mesh->IndexCount,
                                             GL_UNSIGNED_INT,
                                             (void *) (Mesh->StartingIndex * sizeof(i32)),
                                             Mesh->BaseVertexIndex);
                }
            }
        }
    }
}
