#ifndef OPUSONE_PLATFORM_H
#define OPUSONE_PLATFORM_H

#include "opusone_common.h"

#include <sdl2/SDL_scancode.h>

enum mouse_button_type
{
    MouseButton_Left   = 0,
    MouseButton_Middle = 1,
    MouseButton_Right  = 2,
    MouseButton_Count,
};

struct game_input
{
    u8 CurrentKeyStates_[SDL_NUM_SCANCODES];
    u8 PreviousKeyStates_[SDL_NUM_SCANCODES];
    u8 CurrentMouseButtonStates_[MouseButton_Count];
    u8 PreviousMouseButtonStates_[MouseButton_Count];

    i32 MouseX;
    i32 MouseY;
    i32 MouseDeltaX;
    i32 MouseDeltaY;

    f32 DeltaTime;
    i32 ScreenWidth;
    i32 ScreenHeight;
    i32 OriginalScreenWidth;
    i32 OriginalScreenHeight;
};

struct game_memory
{
    b32 IsInitialized;
    
    size_t StorageSize;
    void *Storage;
};

struct platform_image
{
    u32 Width;
    u32 Height;
    u32 Pitch;
    u32 BytesPerPixel;
    u8 *ImageData;
    void *PointerToFree_;
};

struct platform_font
{
    void *Font_;
    u32 Height;
    u32 PointSize;
};

void
GameUpdateAndRender(game_input *GameInput, game_memory *GameMemory, b32 *GameShouldQuit);

inline b32
Platform_KeyIsDown(game_input *GameInput, u32 KeyScancode)
{
    b32 Result = GameInput->CurrentKeyStates_[KeyScancode];
    return Result;
}

inline b32
Platform_KeyJustPressed(game_input *GameInput, u32 KeyScancode)
{
    b32 Result = (GameInput->CurrentKeyStates_[KeyScancode] && !GameInput->PreviousKeyStates_[KeyScancode]);
    return Result;
}

inline b32
Platform_KeyJustReleased(game_input *GameInput, u32 KeyScancode)
{
    b32 Result = (!GameInput->CurrentKeyStates_[KeyScancode] && GameInput->PreviousKeyStates_[KeyScancode]);
    return Result;
}

inline b32
Platform_MouseButtonIsDown(game_input *GameInput, mouse_button_type MouseButton)
{
    b32 Result = GameInput->CurrentMouseButtonStates_[MouseButton];
    return Result;
}

inline b32
Platform_MouseButtonJustPressed(game_input *GameInput, mouse_button_type MouseButton)
{
    b32 Result = (GameInput->CurrentMouseButtonStates_[MouseButton] && !GameInput->PreviousMouseButtonStates_[MouseButton]);
    return Result;
}

inline b32
Platform_MouseButtonJustReleased(game_input *GameInput, mouse_button_type MouseButton)
{
    b32 Result = (!GameInput->CurrentMouseButtonStates_[MouseButton] && GameInput->PreviousMouseButtonStates_[MouseButton]);
    return Result;
}

void
Platform_SetRelativeMouse(b32 Enabled);

char *
Platform_ReadFile(const char *FilePath);

void
Platform_Free(void *Memory);

platform_image
Platform_LoadImage(const char *ImagePath);

void
Platform_FreeImage(platform_image *PlatformImage);

platform_font
Platform_LoadFont(const char *FontPath, u32 PointSize);

void
Platform_CloseFont(platform_font *PlatformFont);

void
Platform_GetGlyphMetrics(platform_font *PlatformFont, char Glyph,
                        i32 *Out_MinX, i32 *Out_MaxX, i32 *Out_MinY, i32 *Out_MaxY, i32 *Out_Advance);

platform_image
Platform_RenderGlyph(platform_font *PlatformFont, char Glyph);

void
Platform_SaveImageToDisk(const char *Path, platform_image *PlatformImage, u32 RMask, u32 GMask, u32 BMask, u32 AMask);

#endif
