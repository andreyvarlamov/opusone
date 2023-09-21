#ifndef OPUSONE_IMMTEXT_H
#define OPUSONE_IMMTEXT_H

#include "opusone_common.h"
#include "opusone_linmath.h"
#include "opusone_render.h"

struct glyph_info
{
    vec2 GlyphUVs[4];

    i32 MinX;
    i32 MaxX;
    i32 MinY;
    i32 MaxY;
    i32 Advance;
};

struct font_info
{
    u32 GlyphCount;
    glyph_info *GlyphInfos;

    u32 TextureID;
    u32 PointSize;
    u32 Height;
};

font_info *
ImmText_LoadFont(memory_arena *Arena, const char *Path, u32 PointSize);


void
ImmText_DrawString(const char *String, font_info *FontInfo, i32 X, i32 Y, u32 ScreenWidth, u32 ScreenHeight,
                   vec4 Color, b32 DrawBackground, vec3 BackgroundColor, render_unit *RenderUnit, memory_arena *Arena);

#endif
