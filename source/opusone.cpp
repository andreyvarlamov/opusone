#include "opusone.h"

#include <cstdio>
#include <glad/glad.h>

void
GameUpdateAndRender(game_input *GameInput, game_memory *GameMemory, b32 *GameShouldQuit)
{
    game_state *GameState = (game_state *) GameMemory->Storage;
    if (!GameMemory->IsInitialized)
    {
        char *FileContent = PlatformReadFile("resources/shaders/Basic.vs");
        printf("%s\n", FileContent);
        PlatformFree(FileContent);
        
        GameMemory->IsInitialized = true;
    }

    if (GameInput->CurrentKeyStates[SDL_SCANCODE_ESCAPE])
    {
        *GameShouldQuit = true;
    }

    glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}
