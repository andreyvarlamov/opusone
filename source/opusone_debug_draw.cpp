#ifndef OPUSONE_DEBUG_DRAW_CPP
#define OPUSONE_DEBUG_DRAW_CPP

#include "opusone_common.h"
#include "opusone_linmath.h"
#include "opusone_render.h"

void
DD_DrawAABox(render_unit *RenderUnit, vec3 Position, vec3 Extents, vec3 Color)
{
    Assert(RenderUnit);
    
    vec3 Vertices[] = {
        Position + Vec3(-Extents.X,  Extents.Y,  Extents.Z),
        Position + Vec3(-Extents.X, -Extents.Y,  Extents.Z),
        Position + Vec3( Extents.X, -Extents.Y,  Extents.Z),
        Position + Vec3( Extents.X,  Extents.Y,  Extents.Z),
        Position + Vec3( Extents.X,  Extents.Y, -Extents.Z),
        Position + Vec3( Extents.X, -Extents.Y, -Extents.Z),
        Position + Vec3(-Extents.X, -Extents.Y, -Extents.Z),
        Position + Vec3(-Extents.X,  Extents.Y, -Extents.Z)
    };
    u32 VertexCount = ArrayCount(Vertices);

    vec3 Normals[] = {
        VecNormalize(Vec3(-1.0f,  1.0f,  1.0f)),
        VecNormalize(Vec3(-1.0f, -1.0f,  1.0f)),
        VecNormalize(Vec3( 1.0f, -1.0f,  1.0f)),
        VecNormalize(Vec3( 1.0f,  1.0f,  1.0f)),
        VecNormalize(Vec3( 1.0f,  1.0f, -1.0f)),
        VecNormalize(Vec3( 1.0f, -1.0f, -1.0f)),
        VecNormalize(Vec3(-1.0f, -1.0f, -1.0f)),
        VecNormalize(Vec3(-1.0f,  1.0f, -1.0f))
    };

    vec4 Colors[8] = {};
    for (u32 I = 0; I < 8; ++I)
    {
        Colors[I] = Vec4(Color, 1.0f);
    }

#if 0
    i32 Indices[] = {
        0, 1, 3,  3, 1, 2, // front
        4, 5, 7,  7, 5, 6, // back
        7, 0, 4,  4, 0, 3, // top
        1, 6, 2,  2, 6, 5, // bottom
        7, 6, 0,  0, 6, 1, // left
        3, 2, 4,  4, 2, 5  // right
    };
#else
    i32 Indices[] = {
        0, 1,  1, 2,  2, 3,  3, 0,
        4, 5,  5, 6,  6, 7,  7, 4,
        0, 7,  1, 6,  2, 5,  3, 4
    };
#endif
    u32 IndexCount = ArrayCount(Indices);

    render_marker *Marker = RenderUnit->Markers + (RenderUnit->MarkerCount++);
    Assert(RenderUnit->MarkerCount <= RenderUnit->MaxMarkerCount);
    *Marker = {};
    Marker->StateT = RENDER_STATE_DEBUG;
    Marker->BaseVertexIndex = RenderUnit->VertexCount;
    Marker->StartingIndex = RenderUnit->IndexCount;
    Marker->IndexCount = IndexCount;

    void *AttribData[16] = {};
    u32 AttribCount = 0;
    AttribData[AttribCount++] = Vertices;
    AttribData[AttribCount++] = Normals;
    AttribData[AttribCount++] = Colors;
    Assert(AttribCount <= ArrayCount(AttribData));

    SubVertexDataForRenderUnit(RenderUnit, AttribData, AttribCount, Indices, VertexCount, IndexCount);
}

void
DD_DrawEllipsoid(render_unit *RenderUnit, vec3 Position, f32 XRadius, f32 YRadius, u32 RingCount, u32 SectorCount, vec3 Color, memory_arena *TransientArena)
{
    Assert(RingCount > 2);
    Assert(SectorCount > 2);

    u32 VertexCount = 2 + (RingCount - 2) * SectorCount;
    u32 IndexCount = (RingCount - 2) * SectorCount * 6;
    
    vec3 *Vertices = MemoryArena_PushArray(TransientArena, VertexCount, vec3);
    vec3 *Normals = MemoryArena_PushArray(TransientArena, VertexCount, vec3);
    vec4 *Colors = MemoryArena_PushArray(TransientArena, VertexCount, vec4);

    vec4 ColorV4 = Vec4(Color, 1.0f);

    u32 VertexIndex = 0;

    for (u32 RingIndex = 0; RingIndex < RingCount; ++RingIndex)
    {
        if (RingIndex == 0)
        {
            // At RingIndex = 0 -> VerticalAngle = PI/2 -> cos = 0 -> 0 radius ring -> north pole
            Vertices[VertexIndex] = vec3 { 0.0f, YRadius, 0.0f } + Position;
            Normals[VertexIndex] = vec3 { 0.0f, 1.0f, 0.0f };
            Colors[VertexIndex] = ColorV4;
            VertexIndex++;
        }
        else if (RingIndex == (RingCount - 1))
        {
            // At RingIndex = RingCount - 1 -> VerticalAngle = -PI/2 -> cos = 0; 0 radius ring -> south pole
            Vertices[VertexIndex] = vec3 { 0.0f, -YRadius, 0.0f } + Position;
            Normals[VertexIndex] = vec3 { 0.0f, -1.0f, 0.0f };
            Colors[VertexIndex] = ColorV4;
            VertexIndex++;
        }
        else
        {
            f32 RingRatio = ((f32) RingIndex / (f32) (RingCount - 1));

            f32 VerticalAngle = (PI32 / 2.0f - (PI32 * RingRatio)); // PI/2 -> -PI/2 Range
            f32 RingRadius = XRadius * CosF(VerticalAngle);
            f32 Y = YRadius * SinF(VerticalAngle); // Radius -> -Radius range

            for (u32 SectorIndex = 0; SectorIndex < SectorCount; ++SectorIndex)
            {
                f32 SectorRatio = (f32) SectorIndex / (f32) SectorCount;
                f32 Theta = 2.0f * PI32 * SectorRatio;
                f32 X = RingRadius * SinF(Theta);
                f32 Z = RingRadius * CosF(Theta);

                vec3 VertPosition = vec3 { X, Y, Z };
                Vertices[VertexIndex] = VertPosition + Position;  // Add the position of the sphere origin
                Normals[VertexIndex] = VecNormalize(VertPosition);
                Colors[VertexIndex] = ColorV4;
                VertexIndex++;
            }
        }
    }
    Assert(VertexIndex == VertexCount);

    i32 *Indices = MemoryArena_PushArray(TransientArena, IndexCount, i32);

    u32 IndexIndex = 0;

    for (u32 RingIndex = 0; RingIndex < RingCount - 1; ++RingIndex)
    {
        for (u32 SectorIndex = 0; SectorIndex < SectorCount; ++SectorIndex)
        {
            i32 NextSectorIndex = ((SectorIndex == (SectorCount - 1)) ?
                                   (0) :
                                   ((i32) SectorIndex + 1));

            if (RingIndex == 0)
            {
                // Sectors are triangles, add 3 indices

                i32 CurrentRingSector0 = 0;
                i32 NextRingSector0 = 1;

                Indices[IndexIndex++] = CurrentRingSector0;
                Indices[IndexIndex++] = NextRingSector0 + (i32) SectorIndex;
                Indices[IndexIndex++] = NextRingSector0 + NextSectorIndex;
            }
            else
            {
                i32 CurrentRingSector0 = 1 + ((i32) RingIndex - 1) * (i32) SectorCount;
                i32 NextRingSector0 = 1 + (i32) RingIndex * (i32) SectorCount;

                if (RingIndex == (RingCount - 2))
                {
                    // Sectors are triangles, add 3 indices

                    Indices[IndexIndex++] = CurrentRingSector0 + (i32) SectorIndex;
                    Indices[IndexIndex++] = NextRingSector0;
                    Indices[IndexIndex++] = CurrentRingSector0 + NextSectorIndex;
                }
                else
                {
                    // Sectors are quads, add 6 indices

                    Indices[IndexIndex++] = CurrentRingSector0 + (i32) SectorIndex;
                    Indices[IndexIndex++] = NextRingSector0 + (i32) SectorIndex;
                    Indices[IndexIndex++] = NextRingSector0 + NextSectorIndex;

                    Indices[IndexIndex++] = CurrentRingSector0 + (i32) SectorIndex;
                    Indices[IndexIndex++] = NextRingSector0 + NextSectorIndex;
                    Indices[IndexIndex++] = CurrentRingSector0 + NextSectorIndex;
                }
            }
        }
    }
    Assert(IndexIndex == IndexCount);

    render_marker *Marker = RenderUnit->Markers + (RenderUnit->MarkerCount++);
    Assert(RenderUnit->MarkerCount <= RenderUnit->MaxMarkerCount);
    *Marker = {};
    Marker->StateT = RENDER_STATE_DEBUG;
    Marker->BaseVertexIndex = RenderUnit->VertexCount;
    Marker->StartingIndex = RenderUnit->IndexCount;
    Marker->IndexCount = IndexCount;
    Marker->StateD.Debug.IsPointMode = true;
    Marker->StateD.Debug.PointSize = 3.0f;

    void *AttribData[16] = {};
    u32 AttribCount = 0;
    AttribData[AttribCount++] = Vertices;
    AttribData[AttribCount++] = Normals;
    AttribData[AttribCount++] = Colors;
    Assert(AttribCount <= ArrayCount(AttribData));

    SubVertexDataForRenderUnit(RenderUnit, AttribData, AttribCount, Indices, VertexCount, IndexCount);
}


void
DD_DrawVector(render_unit *RenderUnit, vec3 A, vec3 B, vec3 AColor, vec3 BColor, f32 LineWidth)
{
    vec3 Vertices[] = { A, B };
    u32 VertexCount = ArrayCount(Vertices);

    vec3 Normals[] = {
        Vec3(0.0f, 1.0f, 0.0f),
        Vec3(0.0f, 1.0f, 0.0f) // ????
    };

    vec4 Colors[] = { Vec4(AColor, 1.0f), Vec4(BColor, 1.0f) };

    i32 Indices[] = { 0, 1 };
    u32 IndexCount = ArrayCount(Indices);

    render_marker *Marker = RenderUnit->Markers + (RenderUnit->MarkerCount++);
    Assert(RenderUnit->MarkerCount <= RenderUnit->MaxMarkerCount);
    *Marker = {};
    Marker->StateT = RENDER_STATE_DEBUG;
    Marker->BaseVertexIndex = RenderUnit->VertexCount;
    Marker->StartingIndex = RenderUnit->IndexCount;
    Marker->IndexCount = IndexCount;
    Marker->StateD.Debug.LineWidth = LineWidth;
    // Marker->StateD.Debug.IsOverlay = true;

    void *AttribData[16] = {};
    u32 AttribCount = 0;
    AttribData[AttribCount++] = Vertices;
    AttribData[AttribCount++] = Normals;
    AttribData[AttribCount++] = Colors;
    Assert(AttribCount <= ArrayCount(AttribData));

    SubVertexDataForRenderUnit(RenderUnit, AttribData, AttribCount, Indices, VertexCount, IndexCount);
}

void
DD_DrawPoint(render_unit *RenderUnit, vec3 Point, vec3 Color, f32 PointSize)
{
    vec3 Vertices[] = { Point };
    u32 VertexCount = ArrayCount(Vertices);

    vec3 Normals[] = {
        Vec3(0.0f, 1.0f, 0.0f) // ????
    };

    vec4 Colors[] = { Vec4(Color, 1.0f) };

    i32 Indices[] = { 0 };
    u32 IndexCount = ArrayCount(Indices);

    render_marker *Marker = RenderUnit->Markers + (RenderUnit->MarkerCount++);
    Assert(RenderUnit->MarkerCount <= RenderUnit->MaxMarkerCount);
    *Marker = {};
    Marker->StateT = RENDER_STATE_DEBUG;
    Marker->BaseVertexIndex = RenderUnit->VertexCount;
    Marker->StartingIndex = RenderUnit->IndexCount;
    Marker->IndexCount = IndexCount;
    Marker->StateD.Debug.PointSize = PointSize;
    // Marker->StateD.Debug.IsOverlay = true;
    Marker->StateD.Debug.IsPointMode = true;

    void *AttribData[16] = {};
    u32 AttribCount = 0;
    AttribData[AttribCount++] = Vertices;
    AttribData[AttribCount++] = Normals;
    AttribData[AttribCount++] = Colors;
    Assert(AttribCount <= ArrayCount(AttribData));

    SubVertexDataForRenderUnit(RenderUnit, AttribData, AttribCount, Indices, VertexCount, IndexCount);
}

void
DD_DrawAxes(render_unit *RenderUnit, vec3 Position, vec3 X, vec3 Y, vec3 Z, f32 Scale)
{
    DD_DrawVector(RenderUnit,
                  Position, Position + X * Scale,
                  Vec3(0.8f, 0.8f, 0.8f), Vec3(1.0f, 0.0f, 0.0f), 2.0f);
    DD_DrawVector(RenderUnit,
                  Position, Position + Y * Scale,
                  Vec3(0.8f, 0.8f, 0.8f), Vec3(0.0f, 1.0f, 0.0f), 2.0f);
    DD_DrawVector(RenderUnit,
                  Position, Position + Z * Scale,
                  Vec3(0.8f, 0.8f, 0.8f), Vec3(0.0f, 0.0f, 1.0f), 2.0f);
}

void
DD_DrawTriangle(render_unit *RenderUnit, vec3 A, vec3 B, vec3 C, vec3 Color)
{
    vec3 Vertices[] = { A, B, C };
    u32 VertexCount = ArrayCount(Vertices);

    vec3 Normals[] = {
        Vec3(0,1,0),
        Vec3(0,1,0),
        Vec3(0,1,0)
    };

    vec4 Colors[] = { Vec4(Color, 1.0f), Vec4(Color, 1.0f), Vec4(Color, 1.0f) };

    i32 Indices[] = { 0, 1, 1, 2, 2, 0 };
    u32 IndexCount = ArrayCount(Indices);

    render_marker *Marker = RenderUnit->Markers + (RenderUnit->MarkerCount++);
    Assert(RenderUnit->MarkerCount <= RenderUnit->MaxMarkerCount);
    *Marker = {};
    Marker->StateT = RENDER_STATE_DEBUG;
    Marker->BaseVertexIndex = RenderUnit->VertexCount;
    Marker->StartingIndex = RenderUnit->IndexCount;
    Marker->IndexCount = IndexCount;

    void *AttribData[16] = {};
    u32 AttribCount = 0;
    AttribData[AttribCount++] = Vertices;
    AttribData[AttribCount++] = Normals;
    AttribData[AttribCount++] = Colors;
    Assert(AttribCount <= ArrayCount(AttribData));

    SubVertexDataForRenderUnit(RenderUnit, AttribData, AttribCount, Indices, VertexCount, IndexCount);
}

global_variable render_unit *_DDQuick_RenderUnit;

void
DD_InitializeQuickDraw(render_unit *RenderUnit)
{
    _DDQuick_RenderUnit = RenderUnit;
}

void
DD_DrawQuickPoint(vec3 Point, vec3 Color)
{
    DD_DrawPoint(_DDQuick_RenderUnit, Point, Color, 5.0f);
}

void
DD_DrawQuickVector(vec3 A, vec3 B, vec3 Color)
{
    DD_DrawVector(_DDQuick_RenderUnit, A, B, Color, Color, 1.0f);
}

void
DD_DrawQuickAABox(vec3 Position, vec3 Extents, vec3 Color)
{
    DD_DrawAABox(_DDQuick_RenderUnit, Position, Extents, Color);
}

void
DD_DrawQuickTriangle(vec3 A, vec3 B, vec3 C, vec3 Color)
{
    DD_DrawTriangle(_DDQuick_RenderUnit, A, B, C, Color);
}

#endif
