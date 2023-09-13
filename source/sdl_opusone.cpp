#include <cstdlib>
#include <cstdio>

#include <glad/glad.h>
#include <sdl2/SDL.h>
#include <sdl2/SDL_image.h>

#include "opusone_common.h"
#include "opusone_platform.h"

int
main(int Argc, char *Argv[])
{
    Assert(SDL_Init(SDL_INIT_VIDEO) >= 0);
    SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_Window *Window = SDL_CreateWindow("Opus One",
                                          SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                          1920, 1080,
                                          SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    Assert(Window);
    
    SDL_GLContext MainContext = SDL_GL_CreateContext(Window);
    Assert(MainContext);

    gladLoadGLLoader(SDL_GL_GetProcAddress);
    printf("OpenGL loaded\n");
    printf("Vendor: %s\n", glGetString(GL_VENDOR));
    printf("Renderer: %s\n", glGetString(GL_RENDERER));
    printf("Version: %s\n", glGetString(GL_VERSION));

    i32 SDLImageFlags = IMG_INIT_JPG | IMG_INIT_PNG;
    Assert((IMG_Init(SDLImageFlags) & SDLImageFlags));

    game_input GameInput = {};
    game_memory GameMemory = {};
    GameMemory.StorageSize = Megabytes(64);
    GameMemory.Storage = calloc(1, GameMemory.StorageSize);
    Assert(GameMemory.Storage);

    SDL_GetWindowSize(Window, &GameInput.ScreenWidth, &GameInput.ScreenHeight);
    glViewport(0, 0, GameInput.ScreenWidth, GameInput.ScreenHeight);

    f32 MonitorRefreshRate = 164.386f; // Hardcode from my display settings for now
    GameInput.DeltaTime = 1.0f / MonitorRefreshRate;

    b32 ShouldQuit = false;
    while (!ShouldQuit)
    {
        SDL_Event SDLEvent;
        while (SDL_PollEvent(&SDLEvent))
        {
            switch (SDLEvent.type)
            {
                case SDL_QUIT:
                {
                    ShouldQuit = true;
                } break;
                case SDL_WINDOWEVENT:
                {
                    if (SDLEvent.window.event == SDL_WINDOWEVENT_RESIZED)
                    {
                        GameInput.ScreenWidth = SDLEvent.window.data1;
                        GameInput.ScreenHeight = SDLEvent.window.data2;
                        glViewport(0, 0, GameInput.ScreenWidth, GameInput.ScreenHeight);
                    }
                } break;
            }
        }

        GameInput.CurrentKeyStates = SDL_GetKeyboardState(0);
        SDL_GetMouseState(&GameInput.MouseX, &GameInput.MouseY);
        u32 SDLMouseButtonState = SDL_GetRelativeMouseState(&GameInput.MouseDeltaX, &GameInput.MouseDeltaY);
        GameInput.MouseButtonState[0] = (SDL_BUTTON(1) & SDLMouseButtonState); // Left mouse button
        GameInput.MouseButtonState[1] = (SDL_BUTTON(2) & SDLMouseButtonState); // Middle mouse button
        GameInput.MouseButtonState[2] = (SDL_BUTTON(3) & SDLMouseButtonState); // Right mouse button

        GameUpdateAndRender(&GameInput, &GameMemory, &ShouldQuit);

        SDL_GL_SwapWindow(Window);
    }

    SDL_Quit();
    return 0;
}

void
PlatformSetRelativeMouse(b32 Enabled)
{
    Assert(SDL_SetRelativeMouseMode((SDL_bool) Enabled) == 0);
}

char *
PlatformReadFile(const char *FilePath)
{
    FILE *File;
    fopen_s(&File, FilePath, "rb");
    // TODO: Handle errors opening files properly
    Assert(File);
    
    fseek(File, 0, SEEK_END);
    size_t FileSize = ftell(File);
    Assert(FileSize > 0);
    fseek(File, 0, SEEK_SET);

    char *Result = (char *) malloc(FileSize + 1);
    Assert(Result);
    size_t ElementsRead = fread(Result, FileSize, 1, File);
    Assert(ElementsRead == 1);
    Result[FileSize] = '\0';

    fclose(File);

    return Result;
}

void
PlatformFree(void *Memory)
{
    free(Memory);
}

platform_load_image_result
PlatformLoadImage(const char *ImagePath)
{
    // TODO: JPG and PNG only for now, BMPs have to be handled separately
    SDL_Surface *ImageSurface = IMG_Load(ImagePath);
    // TODO: Handle errors opening images properly
    Assert(ImageSurface);

    platform_load_image_result Result {};

    Result.Width = (u32) ImageSurface->w;
    Result.Height = (u32) ImageSurface->h;
    Result.Pitch = (u32) ImageSurface->pitch;
    Result.BytesPerPixel = (u32) ImageSurface->format->BytesPerPixel;
    Result.ImageData = (u8 *) ImageSurface->pixels;
    Result.PointerToFree_ = (void *) ImageSurface;

    return Result;
}

void
PlatformFreeImage(platform_load_image_result *PlatformLoadImageResult)
{
    SDL_FreeSurface((SDL_Surface *) PlatformLoadImageResult->PointerToFree_);
    PlatformLoadImageResult->ImageData = 0;
    PlatformLoadImageResult->PointerToFree_ = 0;
}
