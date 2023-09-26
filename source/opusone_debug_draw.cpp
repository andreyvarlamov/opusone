#include "opusone_common.h"
#include "opusone_linmath.h"
#include "opusone_render.h"

void
DD_PushAABox(render_unit *RenderUnit, vec3 Position, vec3 Extents, vec3 Color)
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
DD_PushVector(render_unit *RenderUnit, vec3 A, vec3 B, vec3 AColor, vec3 BColor, f32 LineWidth)
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
    Marker->StateD.Debug.IsOverlay = true;

    void *AttribData[16] = {};
    u32 AttribCount = 0;
    AttribData[AttribCount++] = Vertices;
    AttribData[AttribCount++] = Normals;
    AttribData[AttribCount++] = Colors;
    Assert(AttribCount <= ArrayCount(AttribData));

    SubVertexDataForRenderUnit(RenderUnit, AttribData, AttribCount, Indices, VertexCount, IndexCount);
}

void
DD_PushSimpleVector(render_unit *RenderUnit, vec3 A, vec3 B, vec3 Color)
{
    DD_PushVector(RenderUnit, A, B, Color, Color, 1.0f);
}

void
DD_PushPoint(render_unit *RenderUnit, vec3 Point, vec3 Color, f32 PointSize)
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
    Marker->StateD.Debug.IsOverlay = true;
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
DD_PushCoordinateAxes(render_unit *RenderUnit, vec3 Position, vec3 X, vec3 Y, vec3 Z, f32 Scale)
{
    DD_PushVector(RenderUnit,
                  Position, Position + X * Scale,
                  Vec3(0.8f, 0.8f, 0.8f), Vec3(1.0f, 0.0f, 0.0f), 3.0f);
    DD_PushVector(RenderUnit,
                  Position, Position + Y * Scale,
                  Vec3(0.8f, 0.8f, 0.8f), Vec3(0.0f, 1.0f, 0.0f), 3.0f);
    DD_PushVector(RenderUnit,
                  Position, Position + Z * Scale,
                  Vec3(0.8f, 0.8f, 0.8f), Vec3(0.0f, 0.0f, 1.0f), 3.0f);
}

void
DD_PushTriangle(render_unit *RenderUnit, vec3 A, vec3 B, vec3 C, vec3 Color)
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
