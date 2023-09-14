#include "opusone_common.h"
#include "opusone_platform.h"
#include "opusone.h"
#include "opusone_opengl.h"
#include "opusone_math.h"
#include "opusone_linmath.h"

#include "opusone_camera.cpp"
#include "opusone_assimp.cpp"

#include <cstdio>
#include <glad/glad.h>

inline render_unit *
GetSufficientRenderUnit(game_state *GameState, memory_arena *Arena, u32 VertexCount, u32 IndexCount)
{
    Assert(VertexCount < VERTICES_PER_RENDER_UNIT);
    Assert(IndexCount < INDICES_PER_RENDER_UNIT);
    
    render_unit *RenderUnit = GameState->FirstRenderUnit;
    render_unit *PrevRenderUnit = 0;

    while (RenderUnit)
    {
        if (((RenderUnit->VertexCount + VertexCount) < RenderUnit->MaxVertexCount) &&
            ((RenderUnit->IndexCount + IndexCount) < RenderUnit->MaxIndexCount))
        {
            break;
        }

        PrevRenderUnit = RenderUnit;
        RenderUnit = RenderUnit->Next;
    }

    if (!RenderUnit)
    {
        // TODO: Allocate render unit
        RenderUnit = MemoryArena_PushStruct(Arena, render_unit);
        RenderUnit->VAO = 0;
        RenderUnit->VBO = 0;
        RenderUnit->EBO = 0;
        RenderUnit->VertexCount = 0;
        RenderUnit->MaxVertexCount = VERTICES_PER_RENDER_UNIT;
        RenderUnit->IndexCount = 0;
        RenderUnit->MaxIndexCount = INDICES_PER_RENDER_UNIT;
        if (PrevRenderUnit)
        {
            PrevRenderUnit->Next = RenderUnit;
        }

        vertex_attrib_spec AttribSpecs[] = {
            vertex_attrib_spec { sizeof(vec3), 3 }, // Position
            vertex_attrib_spec { sizeof(vec3), 3 }, // Tangent
            vertex_attrib_spec { sizeof(vec3), 3 }, // Bitangent
            vertex_attrib_spec { sizeof(vec3), 3 }, // Normal
            vertex_attrib_spec { sizeof(vec4), 4 }, // Color
            vertex_attrib_spec { sizeof(vec2), 2 }  // UV
        };
        OpenGL_PrepareVertexData(RenderUnit, AttribSpecs, ArrayCount(AttribSpecs), false);
    }

    return RenderUnit;
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

        InitializeCamera(GameState, vec3 { 0.0f, 0.0f, 5.0f }, 0.0f, 90.0f, 5.0f, CAMERA_CONTROL_MOUSE);

        //
        // NOTE: Load models using assimp
        //
        simple_string ModelPaths[] = {
            SimpleString("resources/models/box_room/BoxRoom.gltf")
        };

        GameState->ImportedModelCount = ArrayCount(ModelPaths);
        GameState->ImportedModels = MemoryArena_PushArray(&GameState->AssetArena, GameState->ImportedModelCount, imported_model *);
        for (u32 ModelIndex = 0;
             ModelIndex < GameState->ImportedModelCount;
             ++ModelIndex)
        {
            GameState->ImportedModels[ModelIndex] = Assimp_LoadModel(&GameState->AssetArena, ModelPaths[ModelIndex].D);
        }

        //
        // NOTE: Calculate how much space to allocate for render data
        //
        GameState->MaterialCount = 0;
        GameState->MeshCount = 0;
        for (u32 ModelIndex = 0;
             ModelIndex < GameState->ImportedModelCount;
             ++ModelIndex)
        {
            imported_model *ImportedModel = GameState->ImportedModels[ModelIndex];
            GameState->MaterialCount += ImportedModel->MaterialCount;
            GameState->MeshCount += ImportedModel->MeshCount;
        }
        GameState->Materials = MemoryArena_PushArray(&GameState->RenderArena, GameState->MaterialCount, render_data_material);
        GameState->Meshes = MemoryArena_PushArray(&GameState->RenderArena, GameState->MeshCount, render_data_mesh);

        //
        // NOTE: Load materials and vertex data to GPU
        //
        for (u32 ModelIndex = 0;
             ModelIndex < GameState->ImportedModelCount;
             ++ModelIndex)
        {
            imported_model *ImportedModel = GameState->ImportedModels[ModelIndex];

            for (u32 MaterialIndex = 0;
                 MaterialIndex < ImportedModel->MaterialCount;
                 ++MaterialIndex)
            {
                imported_material *ImportedMaterial = ImportedModel->Materials + MaterialIndex;
                render_data_material *Material = GameState->Materials + MaterialIndex;
                
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

                render_unit *RenderUnit =
                    GetSufficientRenderUnit(GameState, &GameState->RenderArena, ImportedMesh->VertexCount, ImportedMesh->IndexCount);

                // TODO: Sub data at the sufficient render unit

                /*
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
                */
            }
        }

        #if 0
        {
            imported_mesh *Mesh = GameState->Room.ImportedModel->Meshes;
            size_t AttribStrides[] = { sizeof(vec3), sizeof(vec3), sizeof(vec3), sizeof(vec3), sizeof(vec4), sizeof(vec2) };
            u32 AttribComponentCounts[] = { 3, 3, 3, 3, 3, 2 };
            OpenGL_PrepareVertexData(Mesh->VertexCount, AttribStrides, AttribComponentCounts, ArrayCount(AttribStrides),
                                     Mesh->IndexCount, sizeof(i32), false,
                                     &GameState->Room.VAO, &GameState->Room.VBO, &GameState->Room.EBO);

            size_t AttribMaxBytes[6] = {};
            size_t AttribUsedBytes[6] = {};
            for (u32 AttribIndex = 0;
                 AttribIndex < 6;
                 ++AttribIndex)
            {
                AttribUsedBytes[AttribIndex] = AttribStrides[AttribIndex] * Mesh->VertexCount;
                AttribMaxBytes[AttribIndex] = AttribUsedBytes[AttribIndex];
            }
            void *AttribData[] = {
                Mesh->VertexPositions, Mesh->VertexTangents, Mesh->VertexBitangents,
                Mesh->VertexNormals, Mesh->VertexColors, Mesh->VertexUVs
            };
            
            size_t IndicesUsedBytes = Mesh->IndexCount * sizeof(i32);

            OpenGL_SubVertexData(GameState->Room.VBO, AttribMaxBytes, AttribUsedBytes, AttribData, ArrayCount(AttribData),
                                 GameState->Room.EBO, IndicesUsedBytes, Mesh->Indices);
            
            GameState->Room.IndexCount = Mesh->IndexCount;
        }
        
        for (u32 MaterialIndex = 1;
             MaterialIndex < GameState->Room.ImportedModel->MaterialCount;
             ++MaterialIndex)
        {
            imported_material *Material = GameState->Room.ImportedModel->Materials + MaterialIndex;

            for (u32 TexturePathIndex = 0;
                 TexturePathIndex < TEXTURE_TYPE_COUNT;
                 ++TexturePathIndex)
            {
                simple_string Path = Material->TexturePaths[TexturePathIndex];
                if (Path.Length > 0)
                {
                    platform_load_image_result LoadImageResult = PlatformLoadImage(Path.D);

                    GameState->Room.TextureIDs[TexturePathIndex] =
                        OpenGL_LoadTexture(LoadImageResult.ImageData,
                                           LoadImageResult.Width, LoadImageResult.Height,
                                           LoadImageResult.Pitch, LoadImageResult.BytesPerPixel);

                    PlatformFreeImage(&LoadImageResult);
                }
            }
        }
        #endif

        //
        // NOTE: OpenGL init state
        //
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    if (GameInput->CurrentKeyStates[SDL_SCANCODE_ESCAPE])
    {
        *GameShouldQuit = true;
    }

    UpdateCameraForFrame(GameState, GameInput);
    
    glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    #if 0
    OpenGL_UseShader(GameState->Shader);

    mat4 ProjectionMat = GetPerspecitveProjectionMat(70.0f, (f32) GameInput->ScreenWidth / (f32) GameInput->ScreenHeight, 0.1f, 1000.0f);
    OpenGL_SetUniformMat4F(GameState->Shader, "Projection", (f32 *) &ProjectionMat, false);
    mat4 ViewMat = GetCameraViewMat(&GameState->Camera);
    OpenGL_SetUniformMat4F(GameState->Shader, "View", (f32 *) &ViewMat, false);

    {
        for (u32 TextureIndex = 0;
             TextureIndex < 4;
             ++TextureIndex)
        {
            glActiveTexture(GL_TEXTURE0 + TextureIndex);
            glBindTexture(GL_TEXTURE_2D, GameState->Room.TextureIDs[TextureIndex]);
        }

        glBindVertexArray(GameState->Room.VAO);
        glDrawElements(GL_TRIANGLES, GameState->Room.IndexCount, GL_UNSIGNED_INT, 0);
    }
    #endif
}
