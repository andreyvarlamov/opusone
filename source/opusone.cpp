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

        GameState->ShaderID = OpenGL_BuildShaderProgram("resources/shaders/Basic.vs", "resources/shaders/Basic.fs");
        OpenGL_SetUniformInt(GameState->ShaderID, "DiffuseMap", 0, true);
        OpenGL_SetUniformInt(GameState->ShaderID, "SpecularMap", 1, false);
        OpenGL_SetUniformInt(GameState->ShaderID, "EmissionMap", 2, false);
        OpenGL_SetUniformInt(GameState->ShaderID, "NormalMap", 3, false);

        InitializeCamera(&GameState->Camera, vec3 { 0.0f, 1.7f, 5.0f }, 0.0f, 90.0f, 5.0f, CAMERA_CONTROL_MOUSE);

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
                                 &GameState->RenderArena);
        }

        if (VerticesForSkinnedCount > 0)
        {
            InitializeRenderUnit(&GameState->SkinnedRenderUnit, VERT_SPEC_SKINNED_MESH,
                                 MaterialsForSkinnedCount, MeshesForSkinnedCount,
                                 VerticesForSkinnedCount, IndicesForSkinnedCount,
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

                render_data_material ZeroMaterial {};
                *Material = ZeroMaterial;
                
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

            for (u32 MeshPerModelIndex = 0;
                 MeshPerModelIndex < ImportedModel->MeshCount;
                 ++MeshPerModelIndex)
            {
                imported_mesh *ImportedMesh = ImportedModel->Meshes + MeshPerModelIndex;
                render_data_mesh *Mesh = RenderUnit->Meshes + RenderUnit->MeshCount + MeshPerModelIndex;

                render_data_mesh ZeroMesh {};
                *Mesh = ZeroMesh;
                
                Mesh->BaseVertexIndex = RenderUnit->VertexCount;
                Mesh->StartingIndex = RenderUnit->IndexCount;
                Mesh->IndexCount = ImportedMesh->IndexCount;
                // MaterialID per model -> MaterialID per render unit
                Mesh->MaterialID = ((ImportedMesh->MaterialID == 0) ? 0 :
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
            Blueprint->BaseMeshID = RenderUnit->MeshCount;
            Blueprint->MeshCount = ImportedModel->MeshCount;

            RenderUnit->MaterialCount += ImportedModel->MaterialCount - ((ImportedModel->MaterialCount == 0) ? 0 : 1);
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
            world_object_instance { 1, vec3 { 20.0f, 0.0f, 0.0f }, QuatGetRotationAroundAxis(vec3 { 0.0f, 1.0f, 1.0f }, ToRadiansF(45.0f)), vec3 { 0.5f, 1.0f, 2.0f } },
            world_object_instance { 1, vec3 { 0.0f, 0.0f, -30.0f }, QuatGetRotationAroundAxis(vec3 { 1.0f, 1.0f, 1.0f }, ToRadiansF(160.0f)), vec3 { 1.0f, 1.0f, 5.0f } },
            world_object_instance { 2, vec3 { 0.0f, 0.0f, 0.0f }, QuatGetNeutral(), vec3 { 1.0f, 1.0f, 1.0f } },
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

    OpenGL_UseShader(GameState->ShaderID);

    mat4 ProjectionMat = Mat4GetPerspecitveProjection(70.0f, (f32) GameInput->ScreenWidth / (f32) GameInput->ScreenHeight, 0.1f, 1000.0f);
    OpenGL_SetUniformMat4F(GameState->ShaderID, "Projection", (f32 *) &ProjectionMat, false);
    mat4 ViewMat = GetCameraViewMat(&GameState->Camera);
    OpenGL_SetUniformMat4F(GameState->ShaderID, "View", (f32 *) &ViewMat, false);

    //
    // NOTE: Render each render unit
    //
    render_unit *RenderUnits[] = { &GameState->StaticRenderUnit, &GameState->SkinnedRenderUnit };
    for (u32 RenderUnitIndex = 0;
         RenderUnitIndex < ArrayCount(RenderUnits);
         ++RenderUnitIndex)
    {
        render_unit *RenderUnit = RenderUnits[RenderUnitIndex];

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
                    world_object_instance *Instance = GameState->WorldObjectInstances + InstanceID;
                    mat4 ModelTransform = GetTransformForWorldObjectInstance(Instance);
                    OpenGL_SetUniformMat4F(GameState->ShaderID, "Model", (f32 *) &ModelTransform, false);
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

#include "opusone_camera.cpp"
#include "opusone_assimp.cpp"
#include "opusone_render.cpp"
