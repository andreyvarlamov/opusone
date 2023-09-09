#include "opusone.h"

#include "opusone_opengl.h"
#include "opusone_math.h"
#include "opusone_linmath.h"

#include <cstdio>
#include <glad/glad.h>

void
GameUpdateAndRender(game_input *GameInput, game_memory *GameMemory, b32 *GameShouldQuit)
{
    game_state *GameState = (game_state *) GameMemory->Storage;
    if (!GameMemory->IsInitialized)
    {
        GameState->Shader = OpenGL_BuildShaderProgram("resources/shaders/Basic.vs", "resources/shaders/Basic.fs");

        size_t AttribStrides[] = { sizeof(vec3), sizeof(vec3) };
        u32 AttribComponentCounts[] = { 3, 3 };
        OpenGL_PrepareVertexData(4, AttribStrides, AttribComponentCounts, ArrayCount(AttribStrides),
                                 6, sizeof(i32), true,
                                 &GameState->VAO, &GameState->VBO, &GameState->EBO);

        GameState->IndexCount = 6;

        GameState->Angle = 0.0f;
        
        GameMemory->IsInitialized = true;
    }

    if (GameInput->CurrentKeyStates[SDL_SCANCODE_ESCAPE])
    {
        *GameShouldQuit = true;
    }

    glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    GameState->Angle += 100.0f * GameInput->DeltaTime;
    if (GameState->Angle > 360.0f)
    {
        GameState->Angle -= 360.0f;
    }

    mat3 Rotation = Mat3GetRotationAroundAxis(vec3 { 0.0f, 1.0f, 0.0f }, ToRadiansF(GameState->Angle));
    
    vec3 Positions[] = {
        Rotation * vec3 { -1.0f, -1.0f, 0.0f } + vec3 { 0.0f, 0.0f, -5.0f },
        Rotation * vec3 {  1.0f, -1.0f, 0.0f } + vec3 { 0.0f, 0.0f, -5.0f },
        Rotation * vec3 {  1.0f,  1.0f, 0.0f } + vec3 { 0.0f, 0.0f, -5.0f },
        Rotation * vec3 { -1.0f,  1.0f, 0.0f } + vec3 { 0.0f, 0.0f, -5.0f },
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

    OpenGL_UseShader(GameState->Shader);

    mat4 ProjectionMat = GetPerspecitveProjectionMat(70.0f, (f32) GameInput->ScreenWidth / (f32) GameInput->ScreenHeight, 0.1f, 1000.0f);
    // mat4 ProjectionMat = Mat4Identity();
    OpenGL_SetUniformMat4F(GameState->Shader, "Projection", (f32 *) &ProjectionMat, false);
    
    glBindVertexArray(GameState->VAO);
    glDrawElements(GL_TRIANGLES, GameState->IndexCount, GL_UNSIGNED_INT, 0);
}
