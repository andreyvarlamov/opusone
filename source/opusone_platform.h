#ifndef OPUSONE_PLATFORM_H
#define OPUSONE_PLATFORM_H

#include "opusone_common.h"

#include <sdl2/SDL_scancode.h>

struct game_input
{
    const u8 *CurrentKeyStates;
    u8 KeyWasDown[SDL_NUM_SCANCODES];

    i32 MouseX;
    i32 MouseY;
    i32 MouseDeltaX;
    i32 MouseDeltaY;
    u8 MouseButtonState[3];
    u8 MouseWasDown[3];

    f32 DeltaTime;
    i32 ScreenWidth;
    i32 ScreenHeight;
};

struct game_memory
{
    b32 IsInitialized;
    
    size_t StorageSize;
    void *Storage;
};

struct platform_load_image_result
{
    u32 Width;
    u32 Height;
    u32 Pitch;
    u32 BytesPerPixel;
    u8 *ImageData;
    void *PointerToFree_;
};

void
GameUpdateAndRender(game_input *GameInput, game_memory *GameMemory, b32 *GameShouldQuit);

void
PlatformSetRelativeMouse(b32 Enabled);

char *
PlatformReadFile(const char *FilePath);

void
PlatformFree(void *Memory);

platform_load_image_result
PlatformLoadImage(const char *ImagePath);

void
PlatformFreeImage(platform_load_image_result *PlatformLoadImageResult);

#endif
