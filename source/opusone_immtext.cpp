#include "opusone_immtext.h"

#include "opusone_common.h"
#include "opusone_platform.h"
#include "opusone_linmath.h"
#include "opusone_render.h"

b32
ValidateAllGlyphImagesFreed(platform_image *GlyphImages, u32 GlyphCount)
{
    for (u32 GlyphIndex = 0;
         GlyphIndex < GlyphCount;
         ++GlyphIndex)
    {
        if (GlyphImages[GlyphIndex].ImageData)
        {
            return false;
        }
    }

    return true;
}

font_info *
ImmText_LoadFont(memory_arena *Arena, const char *Path, u32 PointSize)
{
    //
    // NOTE: Allocate memory for the font info and glyph infos
    //
    font_info *FontInfo = MemoryArena_PushStruct(Arena, font_info);
    *FontInfo = {};

    FontInfo->GlyphCount = 128;
    FontInfo->GlyphInfos = MemoryArena_PushArray(Arena, FontInfo->GlyphCount, glyph_info);
    // NOTE: Allocate temp memory for the rendered glyphs (just the header, image bytes are handled by the platform)
    MemoryArena_Freeze(Arena);
    platform_image *GlyphImages = MemoryArena_PushArray(Arena, FontInfo->GlyphCount, platform_image);
    for (u32 GlyphIndex = 0;
         GlyphIndex < FontInfo->GlyphCount;
         ++GlyphIndex)
    {
        FontInfo->GlyphInfos[GlyphIndex] = {};
        GlyphImages[GlyphIndex] = {};
    }

    //
    // NOTE: Use platform layer to load font
    //
    platform_font Font = Platform_LoadFont(Path, PointSize);
    FontInfo->PointSize = Font.PointSize;
    FontInfo->Height = Font.Height;
    u32 BytesPerPixel = 4;

    //
    // NOTE: Render each glyph into an image, get glyph metrics and get max glyph
    // width to allocate byte for the     
    //
    u32 MaxGlyphWidth = 0;
    u32 MaxGlyphHeight = 0;
    for (u8 GlyphChar = 32;
         GlyphChar < FontInfo->GlyphCount;
         ++GlyphChar)
    {
        glyph_info *GlyphInfo = FontInfo->GlyphInfos + GlyphChar;

        Platform_GetGlyphMetrics(&Font, (char) GlyphChar,
                                 &GlyphInfo->MinX, &GlyphInfo->MaxX, &GlyphInfo->MinY, &GlyphInfo->MaxY, &GlyphInfo->Advance);

        platform_image GlyphImage = Platform_RenderGlyph(&Font, (char) GlyphChar);
        if (GlyphImage.Width > MaxGlyphWidth)
        {
            MaxGlyphWidth = GlyphImage.Width;
        }
        if (GlyphImage.Height > MaxGlyphHeight)
        {
            MaxGlyphHeight = GlyphImage.Height;
        }
        Assert(GlyphImage.BytesPerPixel == BytesPerPixel);

        GlyphImages[GlyphChar] = GlyphImage;
    }

    //
    // NOTE: Allocate memory for the font atlas
    //
    
    // NOTE: 12x8 = 96 -> for the 95 visible glyphs
    u32 AtlasColumns = 12;
    u32 AtlasRows = 8;
    u32 AtlasPxWidth = AtlasColumns * MaxGlyphWidth;
    u32 AtlasPxHeight = AtlasRows * MaxGlyphHeight;
    u32 AtlasPitch = BytesPerPixel * AtlasPxWidth;
    u8 *AtlasBytes = MemoryArena_PushArray(Arena, AtlasPitch * AtlasPxHeight, u8);
    
    //
    // NOTE: Blit each glyph surface to the atlas surface
    //
    u32 CurrentGlyphIndex = 0;
    for (u8 GlyphChar = 32;
         GlyphChar < FontInfo->GlyphCount;
         ++GlyphChar)
    {
        platform_image *GlyphImage = GlyphImages + GlyphChar;
        Assert(GlyphImage->Width > 0);
        Assert(GlyphImage->Width <= MaxGlyphWidth);
        Assert(GlyphImage->Height > 0);
        Assert(GlyphImage->Height <= MaxGlyphHeight);
        Assert(GlyphImage->BytesPerPixel == 4);
        Assert(GlyphImage->ImageData);
        
        glyph_info *GlyphInfo = FontInfo->GlyphInfos + GlyphChar;

        u32 CurrentAtlasCol = CurrentGlyphIndex % AtlasColumns;
        u32 CurrentAtlasRow = CurrentGlyphIndex / AtlasColumns;
        u32 AtlasPxX = CurrentAtlasCol * MaxGlyphWidth;
        u32 AtlasPxY = CurrentAtlasRow * FontInfo->Height;
        size_t AtlasByteOffset = (AtlasPxY * AtlasPxWidth + AtlasPxX) * BytesPerPixel;

        u8 *Dest = AtlasBytes + AtlasByteOffset;
        u8 *Source = GlyphImage->ImageData;
        for (u32 GlyphPxY = 0;
             GlyphPxY < GlyphImage->Height;
             ++GlyphPxY)
        {
            u8 *DestByte = (u8 *) Dest;
            u8 *SourceByte = (u8 *) Source;
            
            for (u32 GlyphPxX = 0;
                 GlyphPxX < GlyphImage->Width;
                 ++GlyphPxX)
            {
                for (u32 PixelByte = 0;
                     PixelByte < BytesPerPixel;
                     ++PixelByte)
                {
                    *DestByte++ = *SourceByte++;
                }
            }

            Dest += AtlasPitch;
            Source += GlyphImage->Pitch;
        }

        //
        // NOTE:Use the atlas position and width/height to calculate UVs for each glyph
        //
        // NOTE: It seems that SDL_ttf embeds MinX into the rendered glyph, but also it's ignored if it's less than 0
        // Need to shift where to place glyph if MinX is negative, but if not negative, it's already included
        // in the rendered glyph. This works but seems very finicky
        // TODO: Just use freetype directly or stb
        u32 GlyphTexWidth = ((GlyphInfo->MinX >= 0) ? (GlyphInfo->MaxX) : (GlyphInfo->MaxX - GlyphInfo->MinX));
        u32 GlyphTexHeight = MaxGlyphHeight;
        
        f32 OneOverAtlasPxWidth = 1.0f / (f32) AtlasPxWidth;
        f32 OneOverAtlasPxHeight = 1.0f / (f32) AtlasPxHeight;
        f32 UVLeft = (f32) AtlasPxX * OneOverAtlasPxWidth;
        f32 UVTop = (f32) AtlasPxY * OneOverAtlasPxHeight;
        f32 UVRight = (f32) (AtlasPxX + GlyphTexWidth) * OneOverAtlasPxWidth;
        f32 UVBottom = (f32) (AtlasPxY + GlyphTexHeight) * OneOverAtlasPxHeight;
        GlyphInfo->GlyphUVs[0] = Vec2(UVLeft, UVTop);
        GlyphInfo->GlyphUVs[1] = Vec2(UVLeft, UVBottom);
        GlyphInfo->GlyphUVs[2] = Vec2(UVRight, UVBottom);
        GlyphInfo->GlyphUVs[3] = Vec2(UVRight, UVTop);

        Platform_FreeImage(GlyphImage);

        CurrentGlyphIndex++;
    }

    Assert(ValidateAllGlyphImagesFreed(GlyphImages, FontInfo->GlyphCount));

    FontInfo->TextureID = OpenGL_LoadFontAtlasTexture(AtlasBytes, AtlasPxWidth, AtlasPxHeight, AtlasPitch, BytesPerPixel);

    platform_image AtlasPlatformImage = {};
    AtlasPlatformImage.Width = AtlasPxWidth;
    AtlasPlatformImage.Height = AtlasPxHeight;
    AtlasPlatformImage.Pitch = AtlasPitch;
    AtlasPlatformImage.BytesPerPixel = BytesPerPixel;
    AtlasPlatformImage.ImageData = AtlasBytes;
    simple_string TempFileName = GetFilenameFromPath(Path, false);
    simple_string TempFile = CatStrings(TempFileName.D, ".bmp");
    simple_string SaveToPath = CatStrings("temp/", TempFile.D);
    Platform_SaveImageToDisk(SaveToPath.D, &AtlasPlatformImage,
                             0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);

    MemoryArena_Unfreeze(Arena);
    Platform_CloseFont(&Font);
    
    return FontInfo;
}

internal inline f32
PixelsToNDCRel(f32 Pixels, f32 OneOverHalfScreenDim)
{
    f32 Result = Pixels * OneOverHalfScreenDim;
    return Result;
}

internal inline f32
PixelsToNDCAbs(f32 Pixels, f32 OneOverHalfScreenDim)
{
    f32 Result = PixelsToNDCRel(Pixels, OneOverHalfScreenDim) - 1.0f;
    return Result;
}

internal inline vec2
PixelsToNDCRel(vec2 Pixels, f32 OneOverHalfScreenWidth, f32 OneOverHalfScreenHeight)
{
    f32 X = PixelsToNDCRel(Pixels.X, OneOverHalfScreenWidth);
    f32 Y = -PixelsToNDCRel(Pixels.Y, OneOverHalfScreenHeight);
    return Vec2(X, Y);
}

internal inline vec2
PixelsToNDCAbs(vec2 Pixels, f32 OneOverHalfScreenWidth, f32 OneOverHalfScreenHeight)
{
    f32 X = PixelsToNDCAbs(Pixels.X, OneOverHalfScreenWidth);
    f32 Y = -PixelsToNDCAbs(Pixels.Y, OneOverHalfScreenHeight);
    return Vec2(X, Y);
}

void
ImmText_DrawString(const char *String, font_info *FontInfo, i32 X, i32 Y, u32 ScreenWidth, u32 ScreenHeight,
                   vec4 Color, b32 DrawBackground, vec3 BackgroundColor, render_unit *RenderUnit, memory_arena *Arena)
{
    Assert(FontInfo);
    Assert(FontInfo->TextureID > 0);

    f32 OneOverHalfScreenWidth = 1.0f / ((f32) ScreenWidth * 0.5f);
    f32 OneOverHalfScreenHeight = 1.0f / ((f32) ScreenHeight * 0.5f);

    i32 CurrentX = X;
    i32 MaxX = X;
    i32 CurrentY = Y;

    u32 StringVisibleCount = 0;
    u32 StringCount;
    for (StringCount = 0;
         String[StringCount] != '\0';
         ++StringCount)
    {
        if (String[StringCount] != '\n')
        {
            StringVisibleCount++;
        }
    }

    u32 OffsetForBgVertices = 0;
    u32 OffsetForBgIndices = 0;
    if (DrawBackground)
    {
        OffsetForBgVertices = 4;
        OffsetForBgIndices = 6;
    }

    MemoryArena_Freeze(Arena);
    u32 VertexCount = OffsetForBgVertices + StringVisibleCount * 4;
    u32 IndexCount = OffsetForBgIndices + StringVisibleCount * 6;
    vec2 *Vertices = MemoryArena_PushArray(Arena, VertexCount, vec2);
    vec4 *Colors = MemoryArena_PushArray(Arena, VertexCount, vec4);
    // NOTE: Quick hack: use third param of uv, to add to texture's alpha, to be able to do just filled quads
    // TODO: Do this better
    vec3 *UVs = MemoryArena_PushArray(Arena, VertexCount, vec3); 
    i32 *Indices = MemoryArena_PushArray(Arena, IndexCount, i32);

    u32 CurrentVertexIndex = OffsetForBgVertices;
    u32 CurrentUVIndex = OffsetForBgVertices;
    u32 CurrentColorIndex = OffsetForBgVertices;
    u32 CurrentIndexIndex = OffsetForBgIndices;
    for (u32 StringIndex = 0;
         StringIndex < StringCount;
         ++StringIndex)
    {
        u32 Glyph = String[StringIndex];

        if (Glyph == '\n')
        {
            if (CurrentX > MaxX)
            {
                MaxX = CurrentX;
            }
            CurrentX = X;
            CurrentY += FontInfo->Height;
            continue;
        }

        Assert(Glyph < FontInfo->GlyphCount);
        glyph_info *GlyphInfo = FontInfo->GlyphInfos + Glyph;

        i32 PxX = ((GlyphInfo->MinX >= 0) ? CurrentX : (CurrentX + GlyphInfo->MinX));
        i32 PxY = CurrentY;
        i32 PxWidth = ((GlyphInfo->MinX >= 0) ? GlyphInfo->MaxX : (GlyphInfo->MaxX - GlyphInfo->MinX));
        i32 PxHeight = FontInfo->Height;

        vec2 MinNDC = PixelsToNDCAbs(Vec2((f32) PxX, (f32) PxY), OneOverHalfScreenWidth, OneOverHalfScreenHeight);
        vec2 MaxNDC = PixelsToNDCAbs(Vec2((f32) (PxX + PxWidth), (f32) (PxY + PxHeight)), OneOverHalfScreenWidth, OneOverHalfScreenHeight);

        u32 BaseVertexIndex = CurrentVertexIndex;
        Vertices[CurrentVertexIndex++] =      MinNDC;
        Vertices[CurrentVertexIndex++] = Vec2(MinNDC.X, MaxNDC.Y);
        Vertices[CurrentVertexIndex++] =      MaxNDC;
        Vertices[CurrentVertexIndex++] = Vec2(MaxNDC.X, MinNDC.Y);

        Colors[CurrentColorIndex++] = Color;
        Colors[CurrentColorIndex++] = Color;
        Colors[CurrentColorIndex++] = Color;
        Colors[CurrentColorIndex++] = Color;

        for (u32 GlyphUVIndex = 0;
             GlyphUVIndex < 4;
             ++GlyphUVIndex)
        {
            UVs[CurrentUVIndex++] = Vec3(GlyphInfo->GlyphUVs[GlyphUVIndex], 0);
        }

        i32 IndicesToCopy[] = {
            0, 1, 3,  3, 1, 2
        };

        for (u32 IndexToCopyIndex = 0;
             IndexToCopyIndex < ArrayCount(IndicesToCopy);
             ++IndexToCopyIndex)
        {
            Indices[CurrentIndexIndex++] = BaseVertexIndex + IndicesToCopy[IndexToCopyIndex];
        }

        CurrentX += GlyphInfo->Advance;
    }
    Assert(CurrentVertexIndex == VertexCount);
    Assert(CurrentColorIndex == VertexCount);
    Assert(CurrentUVIndex == VertexCount);
    Assert(CurrentIndexIndex == IndexCount);

    if (DrawBackground)
    {
        i32 Pad = 2;
        i32 PxLeft = Max(0, X - Pad);
        i32 PxTop = Max(0, Y - Pad);
        MaxX = Max(CurrentX, MaxX);
        i32 PxRight = MaxX + Pad;
        i32 PxBottom = CurrentY + FontInfo->Height + Pad;

        vec2 MinNDC = PixelsToNDCAbs(Vec2((f32) PxLeft, (f32) PxTop), OneOverHalfScreenWidth, OneOverHalfScreenHeight);
        vec2 MaxNDC = PixelsToNDCAbs(Vec2((f32) PxRight, (f32) PxBottom), OneOverHalfScreenWidth, OneOverHalfScreenHeight);

        Vertices[0] =      MinNDC;
        Vertices[1] = Vec2(MinNDC.X, MaxNDC.Y);
        Vertices[2] =      MaxNDC;
        Vertices[3] = Vec2(MaxNDC.X, MinNDC.Y);

        vec4 CalcBGColor = Vec4(BackgroundColor, Color.A);
        Colors[0] = CalcBGColor;
        Colors[1] = CalcBGColor;
        Colors[2] = CalcBGColor;
        Colors[3] = CalcBGColor;

        for (u32 GlyphUVIndex = 0;
             GlyphUVIndex < 4;
             ++GlyphUVIndex)
        {
            UVs[GlyphUVIndex] = Vec3(0, 0, 1);
        }

        i32 IndicesToCopy[] = {
            0, 1, 3,  3, 1, 2
        };

        for (u32 IndexToCopyIndex = 0;
             IndexToCopyIndex < ArrayCount(IndicesToCopy);
             ++IndexToCopyIndex)
        {
            Indices[IndexToCopyIndex] = IndicesToCopy[IndexToCopyIndex];
        }
    }

    render_marker *Marker = RenderUnit->Markers + (RenderUnit->MarkerCount++);
    *Marker = {};
    Marker->StateT = RENDER_STATE_IMM_TEXT;
    Marker->BaseVertexIndex = RenderUnit->VertexCount;
    Marker->StartingIndex = RenderUnit->IndexCount;
    Marker->IndexCount = IndexCount;
    Marker->StateD.ImmText.AtlasTextureID = FontInfo->TextureID;

    void *AttribData[16] = {};
    u32 AttribCount = 0;
    AttribData[AttribCount++] = Vertices;
    AttribData[AttribCount++] = Colors;
    AttribData[AttribCount++] = UVs;
    Assert(AttribCount <= ArrayCount(AttribData));

    SubVertexDataForRenderUnit(RenderUnit, AttribData, AttribCount, Indices, VertexCount, IndexCount);

    MemoryArena_Unfreeze(Arena);
}

global_variable font_info *_ImmTextQuick_Font;
global_variable i32 _ImmTextQuick_StartingX;
global_variable i32 _ImmTextQuick_StartingY;
global_variable i32 _ImmTextQuick_CurrentX;
global_variable i32 _ImmTextQuick_CurrentY;
global_variable i32 _ImmTextQuick_ScreenWidth;
global_variable i32 _ImmTextQuick_ScreenHeight;
global_variable vec3 _ImmTextQuick_Color;
global_variable vec3 _ImmTextQuick_BgColor;
global_variable render_unit *_ImmTextQuick_RenderUnit;
global_variable memory_arena *_ImmTextQuick_Arena;

void
ImmText_InitializeQuickDraw(font_info *Font,
                            i32 X, i32 Y, i32 ScreenWidth, i32 ScreenHeight,
                            vec3 Color, vec3 BgColor,
                            render_unit *RenderUnit, memory_arena *Arena)
{
    _ImmTextQuick_Font = Font;
    _ImmTextQuick_CurrentX = X;
    _ImmTextQuick_CurrentY = Y;
    _ImmTextQuick_StartingX = X;
    _ImmTextQuick_StartingY = Y;
    _ImmTextQuick_ScreenWidth = ScreenWidth;
    _ImmTextQuick_ScreenHeight = ScreenHeight;
    _ImmTextQuick_Color = Color;
    _ImmTextQuick_BgColor = BgColor;
    _ImmTextQuick_RenderUnit = RenderUnit;
    _ImmTextQuick_Arena = Arena;
}

void
ImmText_DrawQuickString(const char *String)
{
    ImmText_DrawString(String, _ImmTextQuick_Font,
                       _ImmTextQuick_CurrentX, _ImmTextQuick_CurrentY, _ImmTextQuick_ScreenWidth, _ImmTextQuick_ScreenHeight,
                       Vec4(_ImmTextQuick_Color, 1), true, _ImmTextQuick_BgColor, _ImmTextQuick_RenderUnit, _ImmTextQuick_Arena);

    _ImmTextQuick_CurrentY += _ImmTextQuick_Font->Height;
}

void
ImmText_ResetQuickDraw()
{
    _ImmTextQuick_CurrentX = _ImmTextQuick_StartingX;
    _ImmTextQuick_CurrentY = _ImmTextQuick_StartingY;
}
