#include "opusone_common.h"
#include "opusone_platform.h"
#include "opusone.h"
#include "opusone_math.h"
#include "opusone_linmath.h"
#include "opusone_camera.h"
#include "opusone_assimp.h"
#include "opusone_render.h"

#include <cstdio>
#include <glad/glad.h>

#include "opusone_debug_draw.cpp"

void
ComputeTransformsForAnimation(animation_state *AnimationState, mat4 *BoneTransforms, u32 BoneTransformCount)
{
    imported_armature *Armature = AnimationState->Armature;
    imported_animation *Animation = AnimationState->Animation;
    Assert(Armature);
    Assert(Animation);
    Assert(Armature->BoneCount == BoneTransformCount);
    
    for (u32 BoneIndex = 1;
         BoneIndex < Armature->BoneCount;
         ++BoneIndex)
    {
        imported_bone *Bone = Armature->Bones + BoneIndex;

        mat4 *Transform = BoneTransforms + BoneIndex;
                            
        imported_animation_channel *Channel = Animation->Channels + BoneIndex;
        Assert(Channel->BoneID == BoneIndex);

        // NOTE: Can't just use a "no change" key, because animation is supposed
        // to include bone's transform to parent. If there are such cases, will
        // need to handle them specially
        Assert(Channel->PositionKeyCount != 0);
        Assert(Channel->RotationKeyCount != 0);
        Assert(Channel->ScaleKeyCount != 0);

        vec3 Position = {};

        if (Channel->PositionKeyCount == 1)
        {
            Position = Channel->PositionKeys[0];
        }
        else if (Channel->PositionKeyCount > 1)
        {
            u32 PrevTimeIndex = 0;
            u32 TimeIndex = 0;
            for (;
                 TimeIndex < Channel->PositionKeyCount;
                 ++TimeIndex)
            {
                if (AnimationState->CurrentTicks <= Channel->PositionKeyTimes[TimeIndex])
                {
                    break;
                }
                PrevTimeIndex = TimeIndex;
            }
            Assert(TimeIndex < Channel->PositionKeyCount);
            // NOTE: If the current animation time is at exactly 0.0, both keys will be the same,
            // lerp will have no effect. No need to branch, because that should happen only rarely anyways.
                                
            vec3 PositionA = Channel->PositionKeys[PrevTimeIndex];
            vec3 PositionB = Channel->PositionKeys[TimeIndex];

            f32 T = (f32) ((AnimationState->CurrentTicks - Channel->PositionKeyTimes[PrevTimeIndex]) /
                           (Channel->PositionKeyTimes[TimeIndex] - Channel->PositionKeyTimes[PrevTimeIndex]));

            Position = Vec3Lerp(PositionA, PositionB, T);

#if 0
            if (ShouldLog && PrevTimeIndex == 0)
            {
                printf("[%s]-P-(%d)<%0.3f,%0.3f,%0.3f>:(%d)<%0.3f,%0.3f,%0.3f>[%0.3f]<%0.3f,%0.3f,%0.3f>\n",
                       Bone->BoneName.D,
                       PrevTimeIndex, PositionA.X, PositionA.Y, PositionA.Z,
                       TimeIndex, PositionB.X, PositionB.Y, PositionB.Z,
                       T, Position.X, Position.Y, Position.Z);
            }
#endif
        }
                            
        quat Rotation = Quat();

        if (Channel->RotationKeyCount == 1)
        {
            Rotation = Channel->RotationKeys[0];
        }
        else if (Channel->PositionKeyCount > 1)
        {
            u32 PrevTimeIndex = 0;
            u32 TimeIndex = 0;
            for (;
                 TimeIndex < Channel->PositionKeyCount;
                 ++TimeIndex)
            {
                if (AnimationState->CurrentTicks <= Channel->RotationKeyTimes[TimeIndex])
                {
                    break;
                }
                PrevTimeIndex = TimeIndex;
            }
            Assert(TimeIndex < Channel->RotationKeyCount);

            quat RotationA = Channel->RotationKeys[PrevTimeIndex];
            quat RotationB = Channel->RotationKeys[TimeIndex];

            f32 T = (f32) ((AnimationState->CurrentTicks - Channel->RotationKeyTimes[PrevTimeIndex]) /
                           (Channel->RotationKeyTimes[TimeIndex] - Channel->RotationKeyTimes[PrevTimeIndex]));

            Rotation = QuatSphericalLerp(RotationA, RotationB, T);

#if 0
            if (ShouldLog && PrevTimeIndex == 0)
            {
                printf("[%s]-R-(%d)<%0.3f,%0.3f,%0.3f,%0.3f>:(%d)<%0.3f,%0.3f,%0.3f,%0.3f>[%0.3f]<%0.3f,%0.3f,%0.3f,%0.3f>\n",
                       Bone->BoneName.D,
                       PrevTimeIndex, RotationA.W, RotationA.X, RotationA.Y, RotationA.Z,
                       TimeIndex, RotationB.W, RotationB.X, RotationB.Y, RotationB.Z,
                       T, Rotation.W, Rotation.X, Rotation.Y, Rotation.Z);
            }
#endif
        }
                            
        vec3 Scale = Vec3(1.0f, 1.0f, 1.0f);

        if (Channel->ScaleKeyCount == 1)
        {
            Scale = Channel->ScaleKeys[0];
        }
        else if (Channel->ScaleKeyCount > 1)
        {
            u32 PrevTimeIndex = 0;
            u32 TimeIndex = 0;
            for (;
                 TimeIndex < Channel->ScaleKeyCount;
                 ++TimeIndex)
            {
                if (AnimationState->CurrentTicks <= Channel->ScaleKeyTimes[TimeIndex])
                {
                    break;
                }
                PrevTimeIndex = TimeIndex;
            }
            Assert(TimeIndex < Channel->RotationKeyCount);

            vec3 ScaleA = Channel->ScaleKeys[PrevTimeIndex];
            vec3 ScaleB = Channel->ScaleKeys[TimeIndex];

            f32 T = (f32) ((AnimationState->CurrentTicks - Channel->ScaleKeyTimes[PrevTimeIndex]) /
                           (Channel->ScaleKeyTimes[TimeIndex] - Channel->ScaleKeyTimes[PrevTimeIndex]));

            Scale = Vec3Lerp(ScaleA, ScaleB, T);

#if 0
            if (ShouldLog && PrevTimeIndex == 0)
            {
                printf("[%s]-S-(%d)<%0.3f,%0.3f,%0.3f>:(%d)<%0.3f,%0.3f,%0.3f>[%0.3f]<%0.3f,%0.3f,%0.3f>\n",
                       Bone->BoneName.D,
                       PrevTimeIndex, ScaleA.X, ScaleA.Y, ScaleA.Z,
                       TimeIndex, ScaleB.X, ScaleB.Y, ScaleB.Z,
                       T, Scale.X, Scale.Y, Scale.Z);
            }
#endif
        }

        mat4 AnimationTransform = Mat4GetFullTransform(Position, Rotation, Scale);

        if (Bone->ParentID == 0)
        {
            *Transform = AnimationTransform;
        }
        else
        {
            Assert (Bone->ParentID < BoneIndex);
            *Transform = BoneTransforms[Bone->ParentID] * AnimationTransform;
        }
    }
}

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
    for (u32 GlyphIndex = 0;
         GlyphIndex < FontInfo->GlyphCount;
         ++GlyphIndex)
    {
        FontInfo->GlyphInfos[GlyphIndex] = {};
    }

    //
    // NOTE: Allocate temp memory for the pointers to rendered glyphs
    //
    MemoryArena_Freeze(Arena);
    platform_image *GlyphImages = MemoryArena_PushArray(Arena, FontInfo->GlyphCount, platform_image);

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
        Assert(GlyphImage.Height <= FontInfo->Height);
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
    u32 AtlasPxHeight = AtlasRows * Font.Height;
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
        Assert(GlyphImage->Height <= FontInfo->Height);
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
        u32 GlyphTexHeight = Font.Height;
        
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
    Platform_SaveImageToDisk("temp/font_atlas_test.bmp", &AtlasPlatformImage,
                             0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);

    MemoryArena_Unfreeze(Arena);
    Platform_CloseFont(&Font);
    
    return FontInfo;
}

void
ImmText_DrawString(const char *String, font_info *FontInfo, i32 X, i32 Y, u32 ScreenWidth, u32 ScreenHeight,
                   vec4 Color, render_unit *RenderUnit, memory_arena *Arena)
{
    Assert(FontInfo);
    Assert(FontInfo->TextureID > 0);

    f32 HalfScreenWidth = (f32) ScreenWidth * 0.5f;
    f32 HalfScreenHeight = (f32) ScreenHeight * 0.5f;

    i32 CurrentX = X;
    i32 CurrentY = Y;

    u32 StringCount = 0;
    while(String[StringCount] != '\0')
    {
        StringCount++;
    }

    MemoryArena_Freeze(Arena);
    u32 VertexCount = StringCount * 4;
    u32 IndexCount = StringCount * 6;
    vec2 *Vertices = MemoryArena_PushArray(Arena, VertexCount, vec2);
    vec2 *UVs = MemoryArena_PushArray(Arena, VertexCount, vec2);
    i32 *Indices = MemoryArena_PushArray(Arena, IndexCount, i32);

    u32 CurrentVertexIndex = 0;
    u32 CurrentUVIndex = 0;
    u32 CurrentIndexIndex = 0;
    for (u32 StringIndex = 0;
         StringIndex < StringCount;
         ++StringIndex)
    {
        u32 Glyph = String[StringIndex];

        if (Glyph == '\n')
        {
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
        f32 NDCLeft = ((f32) PxX / HalfScreenWidth) - 1.0f;
        f32 NDCTop = -(((f32) PxY / HalfScreenHeight) - 1.0f); // Inverted
        f32 NDCRight = ((f32) (PxX + PxWidth) / HalfScreenWidth) - 1.0f;
        f32 NDCBottom = -(((f32) (PxY + PxHeight) / HalfScreenHeight) - 1.0f); // Inverted

        u32 BaseVertexIndex = CurrentVertexIndex;
        
        Vertices[CurrentVertexIndex++] = Vec2(NDCLeft, NDCTop);
        Vertices[CurrentVertexIndex++] = Vec2(NDCLeft, NDCBottom);
        Vertices[CurrentVertexIndex++] = Vec2(NDCRight, NDCBottom);
        Vertices[CurrentVertexIndex++] = Vec2(NDCRight, NDCTop);

        for (u32 GlyphUVIndex = 0;
             GlyphUVIndex < 4;
             ++GlyphUVIndex)
        {
            UVs[CurrentUVIndex++] = GlyphInfo->GlyphUVs[GlyphUVIndex];
        }

        i32 IndicesToCopy[] = {
            0, 1, 3,  3, 1, 2
        };

        for (u32 IndexToCopyIndex = 0;
             IndexToCopyIndex < 6;
             ++IndexToCopyIndex)
        {
            Indices[CurrentIndexIndex++] = BaseVertexIndex + IndicesToCopy[IndexToCopyIndex];
        }

        CurrentX += GlyphInfo->Advance;
    }

    render_marker *Marker = RenderUnit->Markers + (RenderUnit->MarkerCount++);
    *Marker = {};
    Marker->StateT = RENDER_STATE_IMM_TEXT;
    Marker->BaseVertexIndex = RenderUnit->VertexCount;
    Marker->StartingIndex = RenderUnit->IndexCount;
    Marker->IndexCount = IndexCount;
    Marker->StateD.ImmText.AtlasTextureID = FontInfo->TextureID;
    Marker->StateD.ImmText.Color = Color;
    Marker->StateD.ImmText.IsOverlay = true;

    void *AttribData[16] = {};
    u32 AttribCount = 0;
    AttribData[AttribCount++] = Vertices;
    AttribData[AttribCount++] = UVs;
    Assert(AttribCount <= ArrayCount(AttribData));

    SubVertexDataForRenderUnit(RenderUnit, AttribData, AttribCount, Indices, VertexCount, IndexCount);

    MemoryArena_Unfreeze(Arena);
}

void
GameUpdateAndRender(game_input *GameInput, game_memory *GameMemory, b32 *GameShouldQuit)
{
    game_state *GameState = (game_state *) GameMemory->Storage;

    //
    // NOTE: First frame/initialization
    //
    if (!GameMemory->IsInitialized)
    {
        GameMemory->IsInitialized = true;

        // TODO: Allocate more than game_state size? Better if I do hot-reloading in the future
        // NOTE: Not sure if nested arenas are a good idea, but I will go with it for now
        u8 *RootArenaBase = (u8 *) GameMemory->Storage + sizeof(game_state);
        size_t RootArenaSize = GameMemory->StorageSize - sizeof(game_state);
        GameState->RootArena = MemoryArena(RootArenaBase, RootArenaSize);

        GameState->WorldArena = MemoryArenaNested(&GameState->RootArena, Megabytes(4));
        GameState->RenderArena = MemoryArenaNested(&GameState->RootArena, Megabytes(4));
        GameState->AssetArena = MemoryArenaNested(&GameState->RootArena, Megabytes(16));

        GameState->ContrailOne24 = ImmText_LoadFont(&GameState->AssetArena, "resources/fonts/ContrailOne-Regular.ttf", 24);

        u32 StaticShader = OpenGL_BuildShaderProgram("resources/shaders/StaticMesh.vs", "resources/shaders/Basic.fs");
        OpenGL_SetUniformInt(StaticShader, "DiffuseMap", 0, true);
        OpenGL_SetUniformInt(StaticShader, "SpecularMap", 1, false);
        OpenGL_SetUniformInt(StaticShader, "EmissionMap", 2, false);
        OpenGL_SetUniformInt(StaticShader, "NormalMap", 3, false);
        u32 SkinnedShader = OpenGL_BuildShaderProgram("resources/shaders/SkinnedMesh.vs", "resources/shaders/Basic.fs");
        OpenGL_SetUniformInt(SkinnedShader, "DiffuseMap", 0, true);
        OpenGL_SetUniformInt(SkinnedShader, "SpecularMap", 1, false);
        OpenGL_SetUniformInt(SkinnedShader, "EmissionMap", 2, false);
        OpenGL_SetUniformInt(SkinnedShader, "NormalMap", 3, false);
        u32 DebugDrawShader = OpenGL_BuildShaderProgram("resources/shaders/DebugDraw.vs", "resources/shaders/DebugDraw.fs");
        u32 ImmTextShader = OpenGL_BuildShaderProgram("resources/shaders/ImmText.vs", "resources/shaders/ImmText.fs");
        OpenGL_SetUniformInt(SkinnedShader, "FontAtlas", 0, true);

        InitializeCamera(&GameState->Camera, vec3 { 0.0f, 1.7f, 5.0f }, 0.0f, 90.0f, 5.0f, CAMERA_CONTROL_MOUSE);

        //
        // NOTE: Load models using assimp
        //
        simple_string ModelPaths[] = {
            SimpleString("resources/models/box_room/BoxRoom.gltf"),
            SimpleString("resources/models/adam/adam_new.gltf"),
            SimpleString("resources/models/complex_animation_keys/AnimationStudy2c.gltf")
        };

        GameState->WorldObjectBlueprintCount = ArrayCount(ModelPaths) + 1;
        GameState->WorldObjectBlueprints = MemoryArena_PushArray(&GameState->WorldArena,
                                                                 GameState->WorldObjectBlueprintCount,
                                                                 world_object_blueprint);
        
        for (u32 BlueprintIndex = 1;
             BlueprintIndex < GameState->WorldObjectBlueprintCount;
             ++BlueprintIndex)
        {
            world_object_blueprint *Blueprint = GameState->WorldObjectBlueprints + BlueprintIndex;
            *Blueprint = {};
            
            Blueprint->ImportedModel = Assimp_LoadModel(&GameState->AssetArena, ModelPaths[BlueprintIndex-1].D);
        }

        //
        // NOTE: Initialize render units
        //
        u32 MaterialsForStaticCount = 0;
        u32 MeshesForStaticCount = 0;
        u32 VerticesForStaticCount = 0;
        u32 IndicesForStaticCount = 0;
        u32 MaterialsForSkinnedCount = 0;
        u32 MeshesForSkinnedCount = 0;
        u32 VerticesForSkinnedCount = 0;
        u32 IndicesForSkinnedCount = 0;
        
        for (u32 BlueprintIndex = 1;
             BlueprintIndex < GameState->WorldObjectBlueprintCount;
             ++BlueprintIndex)
        {
            world_object_blueprint *Blueprint = GameState->WorldObjectBlueprints + BlueprintIndex;
            imported_model *ImportedModel = Blueprint->ImportedModel;

            u32 *MaterialCount;
            u32 *MeshCount;
            u32 *VertexCount;
            u32 *IndexCount;
            
            if (ImportedModel->Armature)
            {
                MaterialCount = &MaterialsForSkinnedCount;
                MeshCount = &MeshesForSkinnedCount;
                VertexCount = &VerticesForSkinnedCount;
                IndexCount = &IndicesForSkinnedCount;
            }
            else
            {
                MaterialCount = &MaterialsForStaticCount;
                MeshCount = &MeshesForStaticCount;
                VertexCount = &VerticesForStaticCount;
                IndexCount = &IndicesForStaticCount;
            }
            
            *MaterialCount += ImportedModel->MaterialCount - ((ImportedModel->MaterialCount == 0) ? 0 : 1);
            *MeshCount += ImportedModel->MeshCount;

            for (u32 MeshIndex = 0;
                 MeshIndex < ImportedModel->MeshCount;
                 ++MeshIndex)
            {
                imported_mesh *ImportedMesh = ImportedModel->Meshes + MeshIndex;

                *VertexCount += ImportedMesh->VertexCount;
                *IndexCount += ImportedMesh->IndexCount;
            }
        }

        if (VerticesForStaticCount > 0)
        {
            InitializeRenderUnit(&GameState->StaticRenderUnit, VERT_SPEC_STATIC_MESH,
                                 MaterialsForStaticCount, MeshesForStaticCount,
                                 VerticesForStaticCount, IndicesForStaticCount,
                                 false, StaticShader, &GameState->RenderArena);
        }

        if (VerticesForSkinnedCount > 0)
        {
            InitializeRenderUnit(&GameState->SkinnedRenderUnit, VERT_SPEC_SKINNED_MESH,
                                 MaterialsForSkinnedCount, MeshesForSkinnedCount,
                                 VerticesForSkinnedCount, IndicesForSkinnedCount,
                                 false, SkinnedShader, &GameState->RenderArena);
        }

        // Debug draw render unit
        {
            u32 MaxMarkers = 1024;
            u32 MaxVertices = 16384;
            u32 MaxIndices = 32768;
            
            InitializeRenderUnit(&GameState->DebugDrawRenderUnit, VERT_SPEC_DEBUG_DRAW,
                                 0, MaxMarkers, MaxVertices, MaxIndices, true, DebugDrawShader,
                                 &GameState->RenderArena);
        }

        // ImmText render unit
        {
            u32 MaxMarkers = 1024;
            u32 MaxVertices = 16384;
            u32 MaxIndices = 32768;
            
            InitializeRenderUnit(&GameState->ImmTextRenderUnit, VERT_SPEC_IMM_TEXT,
                                 0, MaxMarkers, MaxVertices, MaxIndices, true, ImmTextShader,
                                 &GameState->RenderArena);
        }

        //
        // NOTE: Imported data -> Rendering data
        //
        for (u32 BlueprintIndex = 1;
             BlueprintIndex < GameState->WorldObjectBlueprintCount;
             ++BlueprintIndex)
        {
            world_object_blueprint *Blueprint = GameState->WorldObjectBlueprints + BlueprintIndex;
            imported_model *ImportedModel = Blueprint->ImportedModel;

            render_unit *RenderUnit;

            if (ImportedModel->Armature)
            {
                RenderUnit = &GameState->SkinnedRenderUnit;
            }
            else
            {
                RenderUnit = &GameState->StaticRenderUnit;
            }

            for (u32 MaterialPerModelIndex = 1;
                 MaterialPerModelIndex < ImportedModel->MaterialCount;
                 ++MaterialPerModelIndex)
            {
                imported_material *ImportedMaterial = ImportedModel->Materials + MaterialPerModelIndex;
                render_data_material *Material = RenderUnit->Materials + RenderUnit->MaterialCount + (MaterialPerModelIndex - 1);
                *Material = {};
                
                for (u32 TexturePathIndex = 0;
                     TexturePathIndex < TEXTURE_TYPE_COUNT;
                     ++TexturePathIndex)
                {
                    simple_string Path = ImportedMaterial->TexturePaths[TexturePathIndex];
                    if (Path.Length > 0)
                    {
                        platform_image LoadImageResult = Platform_LoadImage(Path.D);

                        Material->TextureIDs[TexturePathIndex] =
                            OpenGL_LoadTexture(LoadImageResult.ImageData,
                                               LoadImageResult.Width, LoadImageResult.Height,
                                               LoadImageResult.Pitch, LoadImageResult.BytesPerPixel);

                        Platform_FreeImage(&LoadImageResult);
                    }
                }
            }

            for (u32 MeshIndex = 0;
                 MeshIndex < ImportedModel->MeshCount;
                 ++MeshIndex)
            {
                imported_mesh *ImportedMesh = ImportedModel->Meshes + MeshIndex;
                render_marker *Marker = RenderUnit->Markers + RenderUnit->MarkerCount + MeshIndex;
                *Marker = {};

                Marker->StateT = RENDER_STATE_MESH;
                Marker->BaseVertexIndex = RenderUnit->VertexCount;
                Marker->StartingIndex = RenderUnit->IndexCount;
                Marker->IndexCount = ImportedMesh->IndexCount;
                // MaterialID per model -> MaterialID per render unit
                Marker->StateD.Mesh.MaterialID = ((ImportedMesh->MaterialID == 0) ? 0 :
                                    (RenderUnit->MaterialCount + (ImportedMesh->MaterialID - 1)));

                void *AttribData[16] = {};
                u32 AttribCount = 0;
                
                switch (RenderUnit->VertSpecType)
                {
                    case VERT_SPEC_STATIC_MESH:
                    {
                        AttribData[AttribCount++] = ImportedMesh->VertexPositions;
                        AttribData[AttribCount++] = ImportedMesh->VertexTangents;
                        AttribData[AttribCount++] = ImportedMesh->VertexBitangents;
                        AttribData[AttribCount++] = ImportedMesh->VertexNormals;
                        AttribData[AttribCount++] = ImportedMesh->VertexColors;
                        AttribData[AttribCount++] = ImportedMesh->VertexUVs;
                    } break;

                    case VERT_SPEC_SKINNED_MESH:
                    {
                        AttribData[AttribCount++] = ImportedMesh->VertexPositions;
                        AttribData[AttribCount++] = ImportedMesh->VertexTangents;
                        AttribData[AttribCount++] = ImportedMesh->VertexBitangents;
                        AttribData[AttribCount++] = ImportedMesh->VertexNormals;
                        AttribData[AttribCount++] = ImportedMesh->VertexColors;
                        AttribData[AttribCount++] = ImportedMesh->VertexUVs;
                        AttribData[AttribCount++] = ImportedMesh->VertexBoneIDs;
                        AttribData[AttribCount++] = ImportedMesh->VertexBoneWeights;
                    } break;

                    default:
                    {
                        InvalidCodePath;
                    } break;
                }

                Assert(AttribCount <= ArrayCount(AttribData));
                SubVertexDataForRenderUnit(RenderUnit, AttribData, AttribCount, ImportedMesh->Indices,
                                           ImportedMesh->VertexCount, ImportedMesh->IndexCount);
            }

            Blueprint->RenderUnit = RenderUnit;
            Blueprint->BaseMeshID = RenderUnit->MarkerCount;
            Blueprint->MeshCount = ImportedModel->MeshCount;

            RenderUnit->MaterialCount += ImportedModel->MaterialCount - ((ImportedModel->MaterialCount == 0) ? 0 : 1);
            RenderUnit->MarkerCount += ImportedModel->MeshCount;
        }

        //
        // NOTE: OpenGL init state
        //
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glEnable(GL_LINE_SMOOTH);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        //
        // NOTE: Add instances of world object blueprints
        //
        world_object_instance Instances[] = {
            world_object_instance { 1, Vec3(0.0f, 0.0f, 0.0f), Quat(), Vec3(1.0f, 1.0f, 1.0f), 0 },
            world_object_instance { 1, Vec3(20.0f,0.0f, 0.0f), Quat(Vec3(0.0f, 1.0f, 1.0f), ToRadiansF(45.0f)), Vec3(0.5f, 1.0f, 2.0f), 0 },
            world_object_instance { 1, Vec3(0.0f,0.0f,-30.0f), Quat(Vec3(1.0f, 1.0f,1.0f), ToRadiansF(160.0f)), Vec3(1.0f, 1.0f, 5.0f), 0 },
            world_object_instance { 2, Vec3(0.0f, 0.0f, 0.0f), Quat(), Vec3(1.0f, 1.0f, 1.0f), 0 },
            world_object_instance { 2, Vec3(-1.0f, 0.0f, 0.0f), Quat(), Vec3(1.0f, 1.0f, 1.0f), 0 },
            world_object_instance { 2, Vec3(1.0f, 0.0f, 0.0f), Quat(), Vec3(1.0f, 1.0f, 1.0f), 0 },
            world_object_instance { 2, Vec3(2.0f, 0.0f, 0.0f), Quat(), Vec3(1.0f, 1.0f, 1.0f), 0 },
            world_object_instance { 3, Vec3(0.0f, 0.0f, -3.0f), Quat(), Vec3(0.3f, 0.3f, 0.3f), 0 },
        };

        GameState->WorldObjectInstanceCount = ArrayCount(Instances) + 1;
        GameState->WorldObjectInstances = MemoryArena_PushArray(&GameState->WorldArena,
                                                                GameState->WorldObjectInstanceCount,
                                                                world_object_instance);
        
        for (u32 InstanceIndex = 1;
             InstanceIndex < GameState->WorldObjectInstanceCount;
             ++InstanceIndex)
        {
            world_object_instance *Instance = GameState->WorldObjectInstances + InstanceIndex;
            *Instance = Instances[InstanceIndex-1];

            Assert(Instance->BlueprintID > 0);
            
            world_object_blueprint *Blueprint = GameState->WorldObjectBlueprints + Instance->BlueprintID;

            if (Blueprint->ImportedModel->Armature)
            {
                Assert(Blueprint->ImportedModel->Animations);
                Assert(Blueprint->ImportedModel->AnimationCount > 0);

                Instance->AnimationState = MemoryArena_PushStruct(&GameState->WorldArena, animation_state);

                Instance->AnimationState->Armature = Blueprint->ImportedModel->Armature;
                
                if (Instance->BlueprintID == 2)
                {
                    // NOTE: Adam: default animation - running
                    Instance->AnimationState->Animation = Blueprint->ImportedModel->Animations + 2;
                }
                else if (Instance->BlueprintID == 3)
                {
                    Instance->AnimationState->Animation = Blueprint->ImportedModel->Animations;
                }
                else
                {
                    // NOTE: All models with armature should have animation in animation state set
                    InvalidCodePath;
                }
                Instance->AnimationState->CurrentTicks = 0.0f;
            }

            render_unit *RenderUnit = Blueprint->RenderUnit;

            for (u32 MeshIndex = 0;
                 MeshIndex < Blueprint->MeshCount;
                 ++MeshIndex)
            {
                render_marker *Marker = RenderUnit->Markers + Blueprint->BaseMeshID + MeshIndex;
                Assert(Marker->StateT == RENDER_STATE_MESH);
                render_state_mesh *Mesh = &Marker->StateD.Mesh;

                b32 SlotFound = false;
                for (u32 SlotIndex = 0;
                     SlotIndex < MAX_INSTANCES_PER_MESH;
                     ++SlotIndex)
                {
                    if (Mesh->InstanceIDs[SlotIndex] == 0)
                    {
                        Mesh->InstanceIDs[SlotIndex] = InstanceIndex;
                        SlotFound = true;
                        break;
                    }
                }
                Assert(SlotFound);
            }
        }
    }

    //
    // NOTE: Misc controls
    //
    if (GameInput->CurrentKeyStates[SDL_SCANCODE_ESCAPE])
    {
        *GameShouldQuit = true;
    }

    //
    // NOTE: Camera
    //
    UpdateCameraForFrame(GameState, GameInput);

    //
    // NOTE: Game logic update
    //
    for (u32 InstanceIndex = 0;
         InstanceIndex < GameState->WorldObjectInstanceCount;
         ++InstanceIndex)
    {
        world_object_instance *Instance = GameState->WorldObjectInstances + InstanceIndex;

        if (Instance->AnimationState)
        {
            f32 DeltaTicks = (f32) (GameInput->DeltaTime * (Instance->AnimationState->Animation->TicksPerSecond));
            Instance->AnimationState->CurrentTicks += DeltaTicks;
            if (Instance->AnimationState->CurrentTicks >= Instance->AnimationState->Animation->TicksDuration)
            {
                Instance->AnimationState->CurrentTicks -= Instance->AnimationState->Animation->TicksDuration;
            }
        }
    }

    vec3 BoxPosition = Vec3(-5.0f, 1.0f, 0.0f);
    vec3 BoxExtents = Vec3(1.0f, 1.0f, 1.0f);
    
    for (u32 BoxIndex = 1;
         BoxIndex <= 5;
         ++BoxIndex)
    {
        DD_PushAABox(&GameState->DebugDrawRenderUnit, BoxPosition, BoxExtents, Vec3(1.0f, 1.0f, 0.0f));
        BoxPosition.X += 2.5f;
    }

    DD_PushCoordinateAxes(&GameState->DebugDrawRenderUnit,
                          Vec3(),
                          Vec3(1.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f), Vec3(0.0f, 0.0f, 1.0f),
                          3.0f);
        
    DD_PushPoint(&GameState->DebugDrawRenderUnit, Vec3 (5.0f, 0.0f, 5.0f), Vec3(1.0f, 0.0f, 0.5f), 3.0f);
    DD_PushPoint(&GameState->DebugDrawRenderUnit, Vec3 (4.0f, 0.0f, 5.0f), Vec3(1.0f, 0.0f, 0.5f), 1.0f);
    DD_PushPoint(&GameState->DebugDrawRenderUnit, Vec3 (6.0f, 0.0f, 5.0f), Vec3(1.0f, 0.0f, 0.5f), 50.0f);
    DD_PushPoint(&GameState->DebugDrawRenderUnit, Vec3 (5.0f, 0.0f, 4.0f), Vec3(1.0f, 0.0f, 0.5f), 10.0f);

    ImmText_DrawString("Hello, world!", GameState->ContrailOne24,
                       5, 5, GameInput->OriginalScreenWidth, GameInput->OriginalScreenHeight,
                       Vec4(1.0f, 1.0f, 1.0f, 1.0f), &GameState->ImmTextRenderUnit, &GameState->RenderArena);
    
    //
    // NOTE: Render
    //
    glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    render_unit *RenderUnits[] = {
        &GameState->StaticRenderUnit,
        &GameState->SkinnedRenderUnit,
        &GameState->DebugDrawRenderUnit,
        &GameState->ImmTextRenderUnit
    };
    
    u32 PreviousShaderID = 0;
    
    for (u32 RenderUnitIndex = 0;
         RenderUnitIndex < ArrayCount(RenderUnits);
         ++RenderUnitIndex)
    {
        render_unit *RenderUnit = RenderUnits[RenderUnitIndex];

        if (PreviousShaderID == 0 || RenderUnit->ShaderID != PreviousShaderID)
        {
            Assert(RenderUnit->ShaderID > 0);
            
            OpenGL_UseShader(RenderUnit->ShaderID);

            mat4 ProjectionMat = Mat4GetPerspecitveProjection(70.0f,
                                                              (f32) GameInput->ScreenWidth / (f32) GameInput->ScreenHeight,
                                                              0.1f, 1000.0f);
            OpenGL_SetUniformMat4F(RenderUnit->ShaderID, "Projection", (f32 *) &ProjectionMat, false);
            
            mat4 ViewMat = GetCameraViewMat(&GameState->Camera);
            OpenGL_SetUniformMat4F(RenderUnit->ShaderID, "View", (f32 *) &ViewMat, false);

            PreviousShaderID = RenderUnit->ShaderID;
        }
        
        glBindVertexArray(RenderUnit->VAO);

        u32 TestCounter = 0;

        u32 PreviousMaterialID = 0;
        for (u32 MarkerIndex = 0;
             MarkerIndex < RenderUnit->MarkerCount;
             ++MarkerIndex)
        {
            render_marker *Marker = RenderUnit->Markers + MarkerIndex;

            switch (Marker->StateT)
            {
                case RENDER_STATE_MESH:
                {
                    render_state_mesh *Mesh = &Marker->StateD.Mesh;
                    
                    if (PreviousMaterialID == 0 || Mesh->MaterialID != PreviousMaterialID)
                    {
                        PreviousMaterialID = Mesh->MaterialID;
                        if (Mesh->MaterialID > 0)
                        {
                            render_data_material *Material = RenderUnit->Materials + Mesh->MaterialID;
                            BindTexturesForMaterial(Material);
                        }
                    }

                    for (u32 InstanceSlotIndex = 0;
                         InstanceSlotIndex < MAX_INSTANCES_PER_MESH;
                         ++InstanceSlotIndex)
                    {
                        u32 InstanceID = Mesh->InstanceIDs[InstanceSlotIndex];
                        if (InstanceID > 0)
                        {
                            // b32 ShouldLog = (InstanceID == 5);
                            world_object_instance *Instance = GameState->WorldObjectInstances + InstanceID;

                            mat4 ModelTransform = Mat4GetFullTransform(Instance->Position, Instance->Rotation, Instance->Scale);

                            if (Instance->AnimationState)
                            {
                                imported_armature *Armature = Instance->AnimationState->Armature;
                                u32 BoneCount = Instance->AnimationState->Armature->BoneCount;
                        
                                MemoryArena_Freeze(&GameState->RenderArena);
                                mat4 *BoneTransforms = MemoryArena_PushArray(&GameState->RenderArena,
                                                                             BoneCount,
                                                                             mat4);
                                                                     
                                ComputeTransformsForAnimation(Instance->AnimationState, BoneTransforms, BoneCount);
                        
                                for (u32 BoneIndex = 0;
                                     BoneIndex < BoneCount;
                                     ++BoneIndex)
                                {
                                    mat4 BoneTransform;
                            
                                    if (BoneIndex == 0)
                                    {
                                        BoneTransform = Mat4(1.0f);
                                    }
                                    else
                                    {
                                        BoneTransform = BoneTransforms[BoneIndex] * Armature->Bones[BoneIndex].InverseBindTransform;
                                    }

                                    simple_string UniformName;
                                    sprintf_s(UniformName.D, "BoneTransforms[%d]", BoneIndex);
                                    OpenGL_SetUniformMat4F(RenderUnit->ShaderID, UniformName.D, (f32 *) &BoneTransform, false);
                                }
                        
                                MemoryArena_Unfreeze(&GameState->RenderArena);
                            }
                            
                            OpenGL_SetUniformMat4F(RenderUnit->ShaderID, "Model", (f32 *) &ModelTransform, false);
                            // TODO: Should I treat indices as unsigned everywhere?
                            // TODO: Instanced draw?
                            glDrawElementsBaseVertex(GL_TRIANGLES,
                                                     Marker->IndexCount,
                                                     GL_UNSIGNED_INT,
                                                     (void *) (Marker->StartingIndex * sizeof(i32)),
                                                     Marker->BaseVertexIndex);
                        }
                    }
                } break; // case RENDER_STATE_MESH

                case RENDER_STATE_DEBUG:
                {
                    render_state_debug *DebugMarker = &Marker->StateD.Debug;

                    if (DebugMarker->LineWidth > 1.0f)
                    {
                        glLineWidth(DebugMarker->LineWidth);
                    }

                    if (DebugMarker->PointSize > 1.0f)
                    {
                        glPointSize(DebugMarker->PointSize);
                    }

                    if (DebugMarker->IsOverlay)
                    {
                        glDisable(GL_DEPTH_TEST);
                    }
                   
                    
                    glDrawElementsBaseVertex(DebugMarker->IsPointMode ? GL_POINTS : GL_LINES,
                                             Marker->IndexCount,
                                             GL_UNSIGNED_INT,
                                             (void *) (Marker->StartingIndex * sizeof(i32)),
                                             Marker->BaseVertexIndex);

                    if (DebugMarker->IsOverlay)
                    {
                        glEnable(GL_DEPTH_TEST);
                    }
                    
                    if (DebugMarker->LineWidth > 1.0f)
                    {
                        glPointSize(1.0f);
                    }
                    
                    if (DebugMarker->PointSize > 1.0f)
                    {
                        glLineWidth(1.0f);
                    }
                    
                } break; // case RENDER_STATE_DEBUG

                case RENDER_STATE_IMM_TEXT:
                {
                    render_state_imm_text *ImmText = &Marker->StateD.ImmText;
                    
                    OpenGL_SetUniformVec4F(RenderUnit->ShaderID, "Color", (f32 *) &ImmText->Color, false);

                    OpenGL_BindAndActivateTexture(0, ImmText->AtlasTextureID);

                    if (ImmText->IsOverlay)
                    {
                        glDisable(GL_DEPTH_TEST);
                    }

                    glDrawElementsBaseVertex(GL_TRIANGLES,
                                             Marker->IndexCount,
                                             GL_UNSIGNED_INT,
                                             (void *) (Marker->StartingIndex * sizeof(i32)),
                                             Marker->BaseVertexIndex);

                    if (ImmText->IsOverlay)
                    {
                        glEnable(GL_DEPTH_TEST);
                    }
                } break; // case RENDER_STATE_IMM_TEXT

                default:
                {
                    InvalidCodePath;
                } break;
            }
        } // for loop processing render unit marks

        if (RenderUnit->IsImmediate)
        {
            RenderUnit->VertexCount = 0;
            RenderUnit->IndexCount = 0;
            RenderUnit->MaterialCount = 1;
            RenderUnit->MarkerCount = 0;
        }
    }
}

#include "opusone_camera.cpp"
#include "opusone_assimp.cpp"
#include "opusone_render.cpp"
