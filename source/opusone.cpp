#include "opusone.h"

#include "opusone_opengl.h"

#include <cstdio>
#include <glad/glad.h>

void
GameUpdateAndRender(game_input *GameInput, game_memory *GameMemory, b32 *GameShouldQuit)
{
    game_state *GameState = (game_state *) GameMemory->Storage;
    if (!GameMemory->IsInitialized)
    {
        GameState->Shader = OpenGL_BuildShaderProgram("resources/shaders/Basic.vs", "resources/shaders/Basic.fs");

        size_t AttribStrides[] = { 3 * sizeof(f32), 3 * sizeof(f32) };
        u32 AttribComponentCounts[] = { 3, 3 };
        OpenGL_PrepareVertexData(3, AttribStrides, AttribComponentCounts, ArrayCount(AttribStrides),
                                 3, sizeof(i32), true,
                                 &GameState->VAO, &GameState->VBO, &GameState->EBO);

        GameState->IndexCount = 3;
        
        GameMemory->IsInitialized = true;
    }

    if (GameInput->CurrentKeyStates[SDL_SCANCODE_ESCAPE])
    {
        *GameShouldQuit = true;
    }

    glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    size_t AttribMaxBytes[] = { 3 * 3 * sizeof(f32), 3 * 3 * sizeof(f32) };
    size_t AttribUsedBytes[] = { 3 * 3 * sizeof(f32), 3 * 3 * sizeof(f32) };
    f32 Positions[] = {
        -1.0f, -1.0f, 0.0f,
        1.0f, -1.0f, 0.0f,
        0.0f, 1.0f, 0.0f
    };
    f32 Colors[] = {
        1.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 1.0f
    };
    void *AttribData[] = { Positions, Colors };
    size_t IndicesUsedBytes = 3 * sizeof(i32);
    i32 IndicesData[] = { 0, 1, 2 };
    OpenGL_SubVertexData(GameState->VBO, AttribMaxBytes, AttribUsedBytes, AttribData, ArrayCount(AttribData),
                         GameState->EBO, IndicesUsedBytes, IndicesData);

    OpenGL_UseShader(GameState->Shader);
    glBindVertexArray(GameState->VAO);
    glDrawElements(GL_TRIANGLES, GameState->IndexCount, GL_UNSIGNED_INT, 0);
}
