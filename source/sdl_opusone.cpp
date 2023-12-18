#include <cstdlib>
#include <cstdio>

#include <glad/glad.h>
#include <sdl2/SDL.h>
#include <sdl2/SDL_image.h>
#include <sdl2/SDL_ttf.h>

#include "opusone_common.h"
#include "opusone_platform.h"

internal void
UpdateInput(game_input *GameInput);

int
main(int Argc, char *Argv[])
{
    i32 SDLInitResult = SDL_Init(SDL_INIT_VIDEO);
    Assert(SDLInitResult >= 0);
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
    printf("PLATFORM: OpenGL loaded\n");
    printf("PLATFORM: Vendor: %s\n", glGetString(GL_VENDOR));
    printf("PLATFORM: Renderer: %s\n", glGetString(GL_RENDERER));
    printf("PLATFORM: Version: %s\n", glGetString(GL_VERSION));

    i32 SDLImageFlags = IMG_INIT_JPG | IMG_INIT_PNG;
    b32 IMGInitResult = IMG_Init(SDLImageFlags);
    Assert(IMGInitResult & SDLImageFlags);

    i32 TTFInitResult = TTF_Init();
    Assert(TTFInitResult != -1);

    game_memory GameMemory = {};
    GameMemory.StorageSize = Megabytes(64);
    GameMemory.Storage = calloc(1, GameMemory.StorageSize);
    Assert(GameMemory.Storage);

    game_input *GameInput = (game_input *) calloc(1, sizeof(game_input));
    Assert(GameInput);

    SDL_GetWindowSize(Window, &GameInput->ScreenWidth, &GameInput->ScreenHeight);
    GameInput->OriginalScreenWidth = GameInput->ScreenWidth;
    GameInput->OriginalScreenHeight = GameInput->ScreenHeight;
    glViewport(0, 0, GameInput->ScreenWidth, GameInput->ScreenHeight);
    printf("PLATFORM: Original Screen Resolution: %d x %d\n", GameInput->OriginalScreenWidth, GameInput->OriginalScreenHeight);

    f32 ExpectedRefreshRates[] = { 60.0f, 164.386f, 30.0f }; // Hardcode from my display settings for now
    u32 ExpectedRefreshRateCount = ArrayCount(ExpectedRefreshRates);
    
    u32 CurrentExpectedRefreshRate = 1;
    GameInput->DeltaTime = 1.0f / ExpectedRefreshRates[CurrentExpectedRefreshRate];
    printf("PLATFORM: Expected refresh rate: %0.3f fps [DT=%0.6f]\n",
           ExpectedRefreshRates[CurrentExpectedRefreshRate], GameInput->DeltaTime);

    u64 PerfCounterFrequency = SDL_GetPerformanceFrequency();
    u64 LastCounter = SDL_GetPerformanceCounter();
    f64 PrevFrameDeltaTimeSec = 0.0f;
    f64 FPS = 0.0f;

#if 0
    i32 OldSwapInterval = SDL_GL_GetSwapInterval();
    i32 SetSwapResult = SDL_GL_SetSwapInterval(0);
    i32 NewSwapInterval = SDL_GL_GetSwapInterval();
    printf("PLATFORM: OldSwapInterval: %d, SetSwapResult: %d, NewSwapInterval: %d\n",
           OldSwapInterval, SetSwapResult, NewSwapInterval);
#endif

    b32 ShouldQuit = false;
    while (!ShouldQuit)
    {
        glFinish(); // TODO: Still need more research and experimentation to decide if I want to do this
        
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
                        GameInput->ScreenWidth = SDLEvent.window.data1;
                        GameInput->ScreenHeight = SDLEvent.window.data2;
                        glViewport(0, 0, GameInput->ScreenWidth, GameInput->ScreenHeight);
                        printf("PLATFORM: Window resized. New resolution: %d x %d\n", GameInput->ScreenWidth, GameInput->ScreenHeight);
                    }
                } break;
            }
        }

        UpdateInput(GameInput);

        if (Platform_KeyJustPressed(GameInput, SDL_SCANCODE_F11))
        {
            CurrentExpectedRefreshRate = (CurrentExpectedRefreshRate + 1) % ExpectedRefreshRateCount;
            GameInput->DeltaTime = 1.0f / ExpectedRefreshRates[CurrentExpectedRefreshRate];
            printf("PLATFORM: Expected refresh rate: %0.3f fps [DT=%0.6f]\n",
                   ExpectedRefreshRates[CurrentExpectedRefreshRate], GameInput->DeltaTime);
        }

        GameUpdateAndRender(GameInput, &GameMemory, &ShouldQuit);

        SDL_GL_SwapWindow(Window);

        u64 CurrentCounter = SDL_GetPerformanceCounter();
        u64 CounterElapsed = CurrentCounter - LastCounter;
        LastCounter = CurrentCounter;
        PrevFrameDeltaTimeSec = (f64) CounterElapsed / (f64) PerfCounterFrequency;
        FPS = 1.0 / PrevFrameDeltaTimeSec;

        char Title[256];
        sprintf_s(Title, "Opus One [%0.3fFPS|%0.3fms]", FPS, PrevFrameDeltaTimeSec * 1000.0f);
        SDL_SetWindowTitle(Window, Title);
    }

    SDL_Quit();
    return 0;
}

internal void
UpdateInput(game_input *GameInput)
{
    const u8 *SDLKeyboardState = SDL_GetKeyboardState(0);
    for (u32 ScancodeIndex = 0;
         ScancodeIndex < SDL_NUM_SCANCODES;
         ++ScancodeIndex)
    {
        GameInput->PreviousKeyStates_[ScancodeIndex] = GameInput->CurrentKeyStates_[ScancodeIndex];
        GameInput->CurrentKeyStates_[ScancodeIndex] = (SDLKeyboardState[ScancodeIndex] != 0);
    }
    
    u32 SDLMouseButtonState = SDL_GetMouseState(&GameInput->MouseX, &GameInput->MouseY);
    for (u32 MouseButtonIndex = 0;
         MouseButtonIndex < MouseButton_Count;
         ++MouseButtonIndex)
    {
        GameInput->PreviousMouseButtonStates_[MouseButtonIndex] = GameInput->CurrentMouseButtonStates_[MouseButtonIndex];
        GameInput->CurrentMouseButtonStates_[MouseButtonIndex] = (SDLMouseButtonState & SDL_BUTTON(MouseButtonIndex + 1));
    }

    SDL_GetRelativeMouseState(&GameInput->MouseDeltaX, &GameInput->MouseDeltaY);
}

void
Platform_SetRelativeMouse(b32 Enabled)
{
    i32 SDLResult = SDL_SetRelativeMouseMode((SDL_bool) Enabled);
    Assert(SDLResult == 0);
}

char *
Platform_ReadFile(const char *FilePath)
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
Platform_Free(void *Memory)
{
    free(Memory);
}

platform_image
Platform_LoadImage(const char *ImagePath)
{
    // TODO: JPG and PNG only for now, BMPs have to be handled separately
    SDL_Surface *ImageSurface = IMG_Load(ImagePath);
    // TODO: Handle errors opening images properly
    Assert(ImageSurface);

    platform_image Result = {};

    Result.Width = (u32) ImageSurface->w;
    Result.Height = (u32) ImageSurface->h;
    Result.Pitch = (u32) ImageSurface->pitch;
    Result.BytesPerPixel = (u32) ImageSurface->format->BytesPerPixel;
    Result.ImageData = (u8 *) ImageSurface->pixels;
    Result.PointerToFree_ = (void *) ImageSurface;

    return Result;
}

void
Platform_FreeImage(platform_image *PlatformImage)
{
    SDL_FreeSurface((SDL_Surface *) PlatformImage->PointerToFree_);
    PlatformImage->ImageData = 0;
    PlatformImage->PointerToFree_ = 0;
}

platform_font
Platform_LoadFont(const char *FontPath, u32 PointSize)
{
    platform_font Result = {};
    TTF_Font *Font = TTF_OpenFont(FontPath, PointSize);
    Assert(Font);

    Result.Font_ = (void *) Font;
    Result.Height = TTF_FontHeight(Font);
    Result.PointSize = PointSize;
    
    return Result;
}

void
Platform_CloseFont(platform_font *PlatformFont)
{
    TTF_CloseFont((TTF_Font  *) PlatformFont->Font_);
    PlatformFont->Font_ = 0;
}

void
Platform_GetGlyphMetrics(platform_font *PlatformFont, char Glyph,
                        i32 *Out_MinX, i32 *Out_MaxX, i32 *Out_MinY, i32 *Out_MaxY, i32 *Out_Advance)
{
    Assert(PlatformFont);
    Assert(PlatformFont->Font_);
    Assert(Out_MinX);
    Assert(Out_MaxX);
    Assert(Out_MinY);
    Assert(Out_MaxY);
    Assert(Out_Advance);
   
    TTF_GlyphMetrics((TTF_Font *) PlatformFont->Font_, Glyph, Out_MinX, Out_MaxX, Out_MinY, Out_MaxY, Out_Advance);
}

platform_image
Platform_RenderGlyph(platform_font *PlatformFont, char Glyph)
{
    Assert(PlatformFont);
    Assert(PlatformFont->Font_);
    TTF_Font *TTFFont = (TTF_Font *) PlatformFont->Font_;
    
    SDL_Surface *GlyphSurface = TTF_RenderGlyph_Blended(TTFFont, Glyph, SDL_Color { 255, 255, 255, 255 });

    // TODO: Handle errors opening images properly
    Assert(GlyphSurface);

    platform_image Result {};

    Result.Width = (u32) GlyphSurface->w;
    Result.Height = (u32) GlyphSurface->h;
    Result.Pitch = (u32) GlyphSurface->pitch;
    Result.BytesPerPixel = (u32) GlyphSurface->format->BytesPerPixel;
    Result.ImageData = (u8 *) GlyphSurface->pixels;
    Result.PointerToFree_ = (void *) GlyphSurface;

    return Result;
}

void
Platform_SaveImageToDisk(const char *Path, platform_image *PlatformImage, u32 RMask, u32 GMask, u32 BMask, u32 AMask)
{
    SDL_Surface *Surface = SDL_CreateRGBSurfaceFrom((void *) PlatformImage->ImageData,
                                                    PlatformImage->Width,
                                                    PlatformImage->Height,
                                                    PlatformImage->BytesPerPixel * 8,
                                                    PlatformImage->Pitch,
                                                    RMask, GMask, BMask, AMask);

    i32 Result = SDL_SaveBMP(Surface, Path);
    Assert(Result == 0);
    
    SDL_FreeSurface(Surface);
}
