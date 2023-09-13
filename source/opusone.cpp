#include "opusone_common.h"
#include "opusone_platform.h"
#include "opusone.h"
#include "opusone_opengl.h"
#include "opusone_math.h"
#include "opusone_linmath.h"
#include "opusone_camera.cpp"
#include "opusone_assimp.h"

#include <cstdio>
#include <glad/glad.h>

void
GameUpdateAndRender(game_input *GameInput, game_memory *GameMemory, b32 *GameShouldQuit)
{
    game_state *GameState = (game_state *) GameMemory->Storage;
    if (!GameMemory->IsInitialized)
    {
        GameMemory->IsInitialized = true;

        GameState->Shader = OpenGL_BuildShaderProgram("resources/shaders/Basic.vs", "resources/shaders/Basic.fs");
        OpenGL_SetUniformInt(GameState->Shader, "DiffuseMap", 0, true);
        OpenGL_SetUniformInt(GameState->Shader, "SpecularMap", 1, false);
        OpenGL_SetUniformInt(GameState->Shader, "EmissionMap", 2, false);
        OpenGL_SetUniformInt(GameState->Shader, "NormalMap", 3, false);

        {
            size_t AttribStrides[] = { sizeof(vec3), sizeof(vec3) };
            u32 AttribComponentCounts[] = { 3, 3 };
            OpenGL_PrepareVertexData(4, AttribStrides, AttribComponentCounts, ArrayCount(AttribStrides),
                                     6, sizeof(i32), true,
                                     &GameState->VAO, &GameState->VBO, &GameState->EBO);
        }

        GameState->IndexCount = 6;

        GameState->Angle = 0.0f;

        InitializeCamera(GameState, vec3 { 0.0f, 0.0f, 5.0f }, 0.0f, 90.0f, 5.0f, CAMERA_CONTROL_MOUSE);

        u8 *AssetArenaBase = (u8 *) GameMemory->Storage + Megabytes(4);
        size_t AssetArenaSize = Megabytes(4);
        GameState->AssetArena = MemoryArena(AssetArenaBase, AssetArenaSize);

        imported_model *Model = Assimp_LoadModel(&GameState->AssetArena, "resources/models/box_room/BoxRoom.gltf");

        {
            imported_mesh *Mesh = Model->Meshes;
            size_t AttribStrides[] = { sizeof(vec3), sizeof(vec3), sizeof(vec3), sizeof(vec3), sizeof(vec4), sizeof(vec2) };
            u32 AttribComponentCounts[] = { 3, 3, 3, 3, 3, 2 };
            OpenGL_PrepareVertexData(Mesh->VertexCount, AttribStrides, AttribComponentCounts, ArrayCount(AttribStrides),
                                     Mesh->IndexCount, sizeof(i32), false,
                                     &GameState->Model.VAO, &GameState->Model.VBO, &GameState->Model.EBO);

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

            OpenGL_SubVertexData(GameState->Model.VBO, AttribMaxBytes, AttribUsedBytes, AttribData, ArrayCount(AttribData),
                                 GameState->Model.EBO, IndicesUsedBytes, Mesh->Indices);
            
            GameState->Model.IndexCount = Mesh->IndexCount;

            glEnable(GL_DEPTH_TEST);
            glEnable(GL_CULL_FACE);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }
        
        for (u32 MaterialIndex = 1;
             MaterialIndex < Model->MaterialCount;
             ++MaterialIndex)
        {
            imported_material *Material = Model->Materials + MaterialIndex;

            for (u32 TexturePathIndex = 0;
                 TexturePathIndex < TEXTURE_TYPE_COUNT;
                 ++TexturePathIndex)
            {
                simple_string Path = Material->TexturePaths[TexturePathIndex];
                if (Path.Length > 0)
                {
                    platform_load_image_result LoadImageResult = PlatformLoadImage(Path.D);

                    GameState->Model.TextureIDs[TexturePathIndex] =
                        OpenGL_LoadTexture(LoadImageResult.ImageData,
                                           LoadImageResult.Width, LoadImageResult.Height,
                                           LoadImageResult.Pitch, LoadImageResult.BytesPerPixel);

                    PlatformFreeImage(&LoadImageResult);
                }
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

    GameState->Angle += 100.0f * GameInput->DeltaTime;
    if (GameState->Angle > 360.0f)
    {
        GameState->Angle -= 360.0f;
    }

    #if 0
    mat3 Rotation = Mat3GetRotationAroundAxis(VecNormalize(vec3 { 0.0f, 1.0f, 1.0f }), ToRadiansF(GameState->Angle));
    
    vec3 Positions[] = {
        Rotation * vec3 { -1.0f, -1.0f, 0.0f },
        Rotation * vec3 {  1.0f, -1.0f, 0.0f },
        Rotation * vec3 {  1.0f,  1.0f, 0.0f },
        Rotation * vec3 { -1.0f,  1.0f, 0.0f }
    };
    vec3 Colors[] = {
        vec3 { 1.0f, 0.0f, 0.0f },
        vec3 { 0.0f, 1.0f, 0.0f },
        vec3 { 0.0f, 0.0f, 1.0f },
        vec3 { 1.0f, 1.0f, 1.0f }
    };

    size_t AttribMaxBytes[] = { sizeof(Positions), sizeof(Colors) };
    size_t AttribUsedBytes[] = { sizeof(Positions), sizeof(Colors) };

    void *AttribData[] = { Positions, Colors };
    i32 IndicesData[] = { 0, 1, 3,  3, 1, 2 };
    size_t IndicesUsedBytes = sizeof(IndicesData);

    OpenGL_SubVertexData(GameState->VBO, AttribMaxBytes, AttribUsedBytes, AttribData, ArrayCount(AttribData),
                         GameState->EBO, IndicesUsedBytes, IndicesData);
    #endif
    
    OpenGL_UseShader(GameState->Shader);

    mat4 ProjectionMat = GetPerspecitveProjectionMat(70.0f, (f32) GameInput->ScreenWidth / (f32) GameInput->ScreenHeight, 0.1f, 1000.0f);
    OpenGL_SetUniformMat4F(GameState->Shader, "Projection", (f32 *) &ProjectionMat, false);
    mat4 ViewMat = GetCameraViewMat(&GameState->Camera);
    OpenGL_SetUniformMat4F(GameState->Shader, "View", (f32 *) &ViewMat, false);

    for (u32 TextureIndex = 0;
         TextureIndex < 4;
         ++TextureIndex)
    {
        glActiveTexture(GL_TEXTURE0 + TextureIndex);
        glBindTexture(GL_TEXTURE_2D, GameState->Model.TextureIDs[TextureIndex]);
    }

    glBindVertexArray(GameState->Model.VAO);
    glDrawElements(GL_TRIANGLES, GameState->Model.IndexCount, GL_UNSIGNED_INT, 0);
     
    // glBindVertexArray(GameState->VAO);
    // glDrawElements(GL_TRIANGLES, GameState->IndexCount, GL_UNSIGNED_INT, 0);
}
