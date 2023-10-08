#include "opusone_render.h"

internal u32
CompileShaderFromPath_(const char *Path, u32 ShaderType)
{
    printf("Compiling shader at %s: ", Path);
    char *Source = Platform_ReadFile(Path);
    u32 Shader = glCreateShader(ShaderType);
    glShaderSource(Shader, 1, &Source, 0);
    glCompileShader(Shader);
    Platform_Free(Source);
    Source = 0;

    i32 Success = 0;
    glGetShaderiv(Shader, GL_COMPILE_STATUS, &Success);
    if (Success)
    {
        printf("Done.\n");
        
        return Shader;
    }
    else
    {
        char LogBuffer[1024];
        i32 ReturnedSize;
        glGetShaderInfoLog(Shader, 1024, &ReturnedSize, LogBuffer);
        Assert(ReturnedSize < 1024);
        Assert(LogBuffer[ReturnedSize] == '\0');
        printf("ERROR:\n%s\n", LogBuffer);

        return 0;
    }
}

internal u32
LinkShaders_(u32 VertexShader, u32 FragmentShader)
{
    u32 ShaderProgram = glCreateProgram();
    glAttachShader(ShaderProgram, VertexShader);
    glAttachShader(ShaderProgram, FragmentShader);
    glLinkProgram(ShaderProgram);

    i32 Success = 0;
    glGetProgramiv(ShaderProgram, GL_LINK_STATUS, &Success);
    if (Success)
    {
        return ShaderProgram;
    }
    else
    {
        char LogBuffer[1024];
        i32 ReturnedSize;
        glGetProgramInfoLog(ShaderProgram, 1024, &ReturnedSize, LogBuffer);
        Assert(ReturnedSize < 1024);
        Assert(LogBuffer[ReturnedSize] == '\0');
        printf("ERROR when linking shaders:\n%s\n", LogBuffer);

        return 0;
    }
}

u32
OpenGL_BuildShaderProgram(const char *VertexPath, const char *FragmentPath)
{
    u32 VertexShader = CompileShaderFromPath_(VertexPath, GL_VERTEX_SHADER);
    u32 FragmentShader = CompileShaderFromPath_(FragmentPath, GL_FRAGMENT_SHADER);

    u32 ShaderProgram = LinkShaders_(VertexShader, FragmentShader);

    glDeleteShader(VertexShader);
    glDeleteShader(FragmentShader);

    Assert(ShaderProgram != 0);

    return ShaderProgram;
}

inline void
OpenGL_UseShader(u32 ShaderID)
{
    glUseProgram(ShaderID);
}

inline b32
OpenGL_SetUniformInt(u32 ShaderID, const char *UniformName, i32 Value, b32 UseProgram)
{
    if (UseProgram)
    {
        glUseProgram(ShaderID);
    }

    i32 UniformLocation = glGetUniformLocation(ShaderID, UniformName);

    if (UniformLocation == -1)
    {
        return false;
    }

    glUniform1i(UniformLocation, Value);
    return true;
}

inline b32
OpenGL_SetUniformVec3F(u32 ShaderID, const char *UniformName, f32 *Value, b32 UseProgram)
{
    if (UseProgram)
    {
        glUseProgram(ShaderID);
    }
    
    i32 UniformLocation = glGetUniformLocation(ShaderID, UniformName);
    
    if (UniformLocation == -1)
    {
        return false;
    }

    glUniform3fv(UniformLocation, 1, Value);
    return true;
}

inline b32
OpenGL_SetUniformVec4F(u32 ShaderID, const char *UniformName, f32 *Value, b32 UseProgram)
{
    if (UseProgram)
    {
        glUseProgram(ShaderID);
    }
    
    i32 UniformLocation = glGetUniformLocation(ShaderID, UniformName);
    
    if (UniformLocation == -1)
    {
        return false;
    }

    glUniform4fv(UniformLocation, 1, Value);
    return true;
}

inline b32
OpenGL_SetUniformMat3F(u32 ShaderID, const char *UniformName, f32 *Value, b32 UseProgram)
{
    if (UseProgram)
    {
        glUseProgram(ShaderID);
    }
    
    i32 UniformLocation = glGetUniformLocation(ShaderID, UniformName);

    if (UniformLocation == -1)
    {
        return false;
    }

    glUniformMatrix3fv(UniformLocation, 1, false, Value);
    return true;
}

inline b32
OpenGL_SetUniformMat4F(u32 ShaderID, const char *UniformName, f32 *Value, b32 UseProgram)
{
    if (UseProgram)
    {
        glUseProgram(ShaderID);
    }
    
    i32 UniformLocation = glGetUniformLocation(ShaderID, UniformName);

    if (UniformLocation == -1)
    {
        return false;
    }

    glUniformMatrix4fv(UniformLocation, 1, false, Value);
    return true;
}

void
OpenGL_PrepareVertexDataHelper(u32 VertexCount, u32 IndexCount,
                               size_t *AttribStrides, u8 *AttribComponentCounts,
                               GLenum *GLDataTypes, gl_vert_attrib_type *GLAttribTypes, u32 AttribCount,
                               size_t IndexSize, GLenum GLUsage,
                               u32 *Out_VAO, u32 *Out_VBO, u32 *Out_EBO)
{
    Assert(VertexCount > 0);
    Assert(IndexCount > 0);
    Assert(AttribStrides);
    Assert(AttribComponentCounts);
    Assert(GLDataTypes);
    Assert(GLAttribTypes);
    Assert(AttribCount > 0);
    Assert(IndexSize > 0);
    Assert(Out_VAO);
    Assert(Out_VBO);
    Assert(Out_EBO);
    
    glGenVertexArrays(1, Out_VAO);
    glBindVertexArray(*Out_VAO);
    Assert(*Out_VAO);
    
    glGenBuffers(1, Out_VBO);
    glBindBuffer(GL_ARRAY_BUFFER, *Out_VBO);
    Assert(*Out_VBO);
    
    size_t TotalVertexSize = 0;
    for (u32 AttribIndex = 0;
         AttribIndex < AttribCount;
         ++AttribIndex)
    {
        TotalVertexSize += AttribStrides[AttribIndex];
    }
    
    glBufferData(GL_ARRAY_BUFFER, TotalVertexSize * VertexCount, 0, GLUsage);

    size_t ByteOffset = 0;
    for (u32 AttribIndex = 0;
         AttribIndex < AttribCount;
         ++AttribIndex)
    {
        switch (GLAttribTypes[AttribIndex])
        {
            case OO_GL_VERT_ATTRIB_INT:
            {
                glVertexAttribIPointer(AttribIndex, AttribComponentCounts[AttribIndex], GLDataTypes[AttribIndex],
                                       (GLsizei) AttribStrides[AttribIndex], (void *) ByteOffset);
            } break;

            case OO_GL_VERT_ATTRIB_FLOAT:
            {
                glVertexAttribPointer(AttribIndex, AttribComponentCounts[AttribIndex], GLDataTypes[AttribIndex], GL_FALSE,
                                      (GLsizei) AttribStrides[AttribIndex], (void *) ByteOffset);
            } break;
            
            default:
            {
                InvalidCodePath;
            } break;
        }
        glEnableVertexAttribArray(AttribIndex);

        ByteOffset += VertexCount * AttribStrides[AttribIndex];
    }
    
    glGenBuffers(1, Out_EBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, *Out_EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, IndexCount * IndexSize, 0, GLUsage);
    Assert(*Out_EBO);
}

void
OpenGL_SubVertexDataHelper(u32 StartingVertex, u32 VertexCount, u32 MaxVertexCount,
                           u32 StartingIndex, u32 IndexCount,
                           size_t *AttribStrides, u32 AttribCount, size_t IndexSize,
                           u32 VBO, u32 EBO, void **AttribData, void *IndicesData)
{
    Assert(VertexCount > 0);
    Assert(IndexCount > 0);
    Assert(AttribStrides);
    Assert(AttribCount > 0);
    Assert(IndexSize > 0);
    Assert(VBO);
    Assert(EBO);
    Assert(AttribData);
    Assert(IndicesData);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);

    size_t AttribStartOffset = 0;
    for (u32 AttribIndex = 0;
         AttribIndex < AttribCount;
         ++AttribIndex)
    {
        if (AttribData[AttribIndex])
        {
            size_t BytesToCopy = AttribStrides[AttribIndex] * VertexCount;
            size_t ByteOffset = AttribStartOffset + (AttribStrides[AttribIndex] * StartingVertex);
            glBufferSubData(GL_ARRAY_BUFFER, ByteOffset, BytesToCopy, AttribData[AttribIndex]);
        }
        AttribStartOffset += AttribStrides[AttribIndex] * MaxVertexCount;
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        size_t BytesToCopy = IndexSize * IndexCount;
        size_t ByteOffset = IndexSize * StartingIndex;
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, ByteOffset, BytesToCopy, IndicesData);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }
}

u32
OpenGL_LoadTexture(u8 *ImageData, u32 Width, u32 Height, u32 Pitch, u32 BytesPerPixel)
{
    // TODO: Handle different image data formats better
    Assert(Width * BytesPerPixel == Pitch);
    Assert(BytesPerPixel == 4 || BytesPerPixel == 3);
    
    u32 TextureID;

    glGenTextures(1, &TextureID);
    Assert(TextureID);
    glBindTexture(GL_TEXTURE_2D, TextureID);
    u32 InternalFormat = (BytesPerPixel == 4 ? GL_RGBA8 : GL_RGB8);
    u32 Format = (BytesPerPixel == 4 ? GL_RGBA : GL_RGB);
    glTexImage2D(GL_TEXTURE_2D, 0, InternalFormat, Width, Height, 0, Format, GL_UNSIGNED_BYTE, ImageData);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    return TextureID;
}

u32
OpenGL_LoadFontAtlasTexture(u8 *ImageData, u32 Width, u32 Height, u32 Pitch, u32 BytesPerPixel)
{
    // TODO: Handle this in the main LoadTexture?
    Assert(Width * BytesPerPixel == Pitch);
    Assert(BytesPerPixel == 4);
    
    u32 TextureID;

    glGenTextures(1, &TextureID);
    Assert(TextureID);
    glBindTexture(GL_TEXTURE_2D, TextureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, Width, Height, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, ImageData);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    return TextureID;
}

void
OpenGL_BindAndActivateTexture(u32 TextureUnitIndex, u32 TextureID)
{
    glActiveTexture(GL_TEXTURE0 + TextureUnitIndex);
    glBindTexture(GL_TEXTURE_2D, TextureID);
}

void
PrepareVertexDataForRenderUnit(render_unit *RenderUnit)
{
    Assert(RenderUnit);

    switch (RenderUnit->VertSpecType)
    {
        case VERT_SPEC_STATIC_MESH:
        {
            size_t AttribStrides[] = {
                sizeof(vec3),
                sizeof(vec3),
                sizeof(vec3),
                sizeof(vec3),
                sizeof(vec4),
                sizeof(vec2)
            };

            u8 AttribComponentCounts[] = {
                3,
                3,
                3,
                3,
                4,
                2
            };

            GLenum GLDataTypes[] = {
                GL_FLOAT,
                GL_FLOAT,
                GL_FLOAT,
                GL_FLOAT,
                GL_FLOAT,
                GL_FLOAT
            };

            gl_vert_attrib_type GLAttribTypes[] = {
                OO_GL_VERT_ATTRIB_FLOAT,
                OO_GL_VERT_ATTRIB_FLOAT,
                OO_GL_VERT_ATTRIB_FLOAT,
                OO_GL_VERT_ATTRIB_FLOAT,
                OO_GL_VERT_ATTRIB_FLOAT,
                OO_GL_VERT_ATTRIB_FLOAT
            };
            
            u32 AttribCount = ArrayCount(AttribStrides);

            OpenGL_PrepareVertexDataHelper(RenderUnit->MaxVertexCount, RenderUnit->MaxIndexCount,
                                           AttribStrides, AttribComponentCounts,
                                           GLDataTypes, GLAttribTypes, AttribCount,
                                           sizeof(i32), GL_STATIC_DRAW,
                                           &RenderUnit->VAO, &RenderUnit->VBO, &RenderUnit->EBO);
        } break;

        case VERT_SPEC_SKINNED_MESH:
        {
            size_t AttribStrides[] = {
                sizeof(vec3),
                sizeof(vec3),
                sizeof(vec3),
                sizeof(vec3),
                sizeof(vec4),
                sizeof(vec2),
                4 * sizeof(u32),
                4 * sizeof(f32)
            };

            u8 AttribComponentCounts[] = {
                3,
                3,
                3,
                3,
                4,
                2,
                4,
                4
            };

            GLenum GLDataTypes[] = {
                GL_FLOAT,
                GL_FLOAT,
                GL_FLOAT,
                GL_FLOAT,
                GL_FLOAT,
                GL_FLOAT,
                GL_INT,
                GL_FLOAT
            };

            gl_vert_attrib_type GLAttribTypes[] = {
                OO_GL_VERT_ATTRIB_FLOAT,
                OO_GL_VERT_ATTRIB_FLOAT,
                OO_GL_VERT_ATTRIB_FLOAT,
                OO_GL_VERT_ATTRIB_FLOAT,
                OO_GL_VERT_ATTRIB_FLOAT,
                OO_GL_VERT_ATTRIB_FLOAT,
                OO_GL_VERT_ATTRIB_INT,
                OO_GL_VERT_ATTRIB_FLOAT
            };
            
            u32 AttribCount = ArrayCount(AttribStrides);

            OpenGL_PrepareVertexDataHelper(RenderUnit->MaxVertexCount, RenderUnit->MaxIndexCount,
                                           AttribStrides, AttribComponentCounts,
                                           GLDataTypes, GLAttribTypes, AttribCount,
                                           sizeof(i32), GL_STATIC_DRAW,
                                           &RenderUnit->VAO, &RenderUnit->VBO, &RenderUnit->EBO);
        } break;

        case VERT_SPEC_DEBUG_DRAW:
        {
            size_t AttribStrides[] = {
                sizeof(vec3),
                sizeof(vec3),
                sizeof(vec4)
            };

            u8 AttribComponentCounts[] = {
                3,
                3,
                4
            };

            GLenum GLDataTypes[] = {
                GL_FLOAT,
                GL_FLOAT,
                GL_FLOAT
            };

            gl_vert_attrib_type GLAttribTypes[] = {
                OO_GL_VERT_ATTRIB_FLOAT,
                OO_GL_VERT_ATTRIB_FLOAT,
                OO_GL_VERT_ATTRIB_FLOAT,
            };
            
            u32 AttribCount = ArrayCount(AttribStrides);

            OpenGL_PrepareVertexDataHelper(RenderUnit->MaxVertexCount, RenderUnit->MaxIndexCount,
                                           AttribStrides, AttribComponentCounts,
                                           GLDataTypes, GLAttribTypes, AttribCount,
                                           sizeof(i32), GL_DYNAMIC_DRAW,
                                           &RenderUnit->VAO, &RenderUnit->VBO, &RenderUnit->EBO);
        } break;

        case VERT_SPEC_IMM_TEXT:
        {
            size_t AttribStrides[] = {
                sizeof(vec2),
                sizeof(vec4),
                sizeof(vec3)
            };

            u8 AttribComponentCounts[] = {
                2,
                4,
                3
            };

            GLenum GLDataTypes[] = {
                GL_FLOAT,
                GL_FLOAT,
                GL_FLOAT
            };

            gl_vert_attrib_type GLAttribTypes[] = {
                OO_GL_VERT_ATTRIB_FLOAT,
                OO_GL_VERT_ATTRIB_FLOAT,
                OO_GL_VERT_ATTRIB_FLOAT,
            };
            
            u32 AttribCount = ArrayCount(AttribStrides);

            OpenGL_PrepareVertexDataHelper(RenderUnit->MaxVertexCount, RenderUnit->MaxIndexCount,
                                           AttribStrides, AttribComponentCounts,
                                           GLDataTypes, GLAttribTypes, AttribCount,
                                           sizeof(i32), GL_DYNAMIC_DRAW,
                                           &RenderUnit->VAO, &RenderUnit->VBO, &RenderUnit->EBO);
        } break;
        
        default:
        {
            InvalidCodePath;
        } break;
    }
}

void
SubVertexDataForRenderUnit(render_unit *RenderUnit,
                           void **AttribData, u32 AttribCount, void *IndicesData,
                           u32 VertexToSubCount, u32 IndexToSubCount)
{
    Assert(RenderUnit);
    Assert(AttribData);
    Assert(AttribCount > 0);
    Assert(IndicesData);
    Assert(VertexToSubCount > 0);
    Assert(IndexToSubCount > 0);
    Assert(RenderUnit->VertexCount + VertexToSubCount <= RenderUnit->MaxVertexCount);
    Assert(RenderUnit->IndexCount + IndexToSubCount <= RenderUnit->MaxIndexCount);
    
    switch (RenderUnit->VertSpecType)
    {
        case VERT_SPEC_STATIC_MESH:
        {
            size_t AttribStrides[] = {
                sizeof(vec3),
                sizeof(vec3),
                sizeof(vec3),
                sizeof(vec3),
                sizeof(vec4),
                sizeof(vec2)
            };

            Assert(AttribCount == ArrayCount(AttribStrides));

            OpenGL_SubVertexDataHelper(RenderUnit->VertexCount, VertexToSubCount, RenderUnit->MaxVertexCount,
                                       RenderUnit->IndexCount, IndexToSubCount,
                                       AttribStrides, AttribCount, sizeof(i32),
                                       RenderUnit->VBO, RenderUnit->EBO, AttribData, IndicesData);

            RenderUnit->VertexCount += VertexToSubCount;
            RenderUnit->IndexCount += IndexToSubCount;
        } break;

        case VERT_SPEC_SKINNED_MESH:
        {
            size_t AttribStrides[] = {
                sizeof(vec3),
                sizeof(vec3),
                sizeof(vec3),
                sizeof(vec3),
                sizeof(vec4),
                sizeof(vec2),
                4 * sizeof(u32),
                4 * sizeof(f32)
            };

            Assert(AttribCount == ArrayCount(AttribStrides));

            OpenGL_SubVertexDataHelper(RenderUnit->VertexCount, VertexToSubCount, RenderUnit->MaxVertexCount,
                                       RenderUnit->IndexCount, IndexToSubCount,
                                       AttribStrides, AttribCount, sizeof(i32),
                                       RenderUnit->VBO, RenderUnit->EBO, AttribData, IndicesData);

            RenderUnit->VertexCount += VertexToSubCount;
            RenderUnit->IndexCount += IndexToSubCount;
        } break;

        case VERT_SPEC_DEBUG_DRAW:
        {
            size_t AttribStrides[] = {
                sizeof(vec3),
                sizeof(vec3),
                sizeof(vec4)
            };

            Assert(AttribCount == ArrayCount(AttribStrides));

            OpenGL_SubVertexDataHelper(RenderUnit->VertexCount, VertexToSubCount, RenderUnit->MaxVertexCount,
                                       RenderUnit->IndexCount, IndexToSubCount,
                                       AttribStrides, AttribCount, sizeof(i32),
                                       RenderUnit->VBO, RenderUnit->EBO, AttribData, IndicesData);

            RenderUnit->VertexCount += VertexToSubCount;
            RenderUnit->IndexCount += IndexToSubCount;
        } break;

        case VERT_SPEC_IMM_TEXT:
        {
            size_t AttribStrides[] = {
                sizeof(vec2),
                sizeof(vec4),
                sizeof(vec3)
            };

            Assert(AttribCount == ArrayCount(AttribStrides));

            OpenGL_SubVertexDataHelper(RenderUnit->VertexCount, VertexToSubCount, RenderUnit->MaxVertexCount,
                                       RenderUnit->IndexCount, IndexToSubCount,
                                       AttribStrides, AttribCount, sizeof(i32),
                                       RenderUnit->VBO, RenderUnit->EBO, AttribData, IndicesData);

            RenderUnit->VertexCount += VertexToSubCount;
            RenderUnit->IndexCount += IndexToSubCount;
        } break;

        default:
        {
            InvalidCodePath;
        } break;
    }
}

void
InitializeRenderUnit(render_unit *RenderUnit, vert_spec_type VertSpecType,
                     u32 MaxMaterialCount, u32 MaxMarkerCount, u32 MaxVertexCount, u32 MaxIndexCount,
                     b32 IsImmediate, b32 IsOverlay, u32 ShaderID, memory_arena *Arena)
{
    Assert(RenderUnit);
    Assert(MaxMarkerCount > 0);
    Assert(MaxVertexCount > 0);
    Assert(MaxIndexCount > 0);
    Assert(Arena);
    
    *RenderUnit = {};

    RenderUnit->VertSpecType = VertSpecType;
    RenderUnit->MaterialCount = 1; // Material0 is null
    RenderUnit->MaxMaterialCount = RenderUnit->MaterialCount + MaxMaterialCount;
    RenderUnit->MaxMarkerCount = MaxMarkerCount;
    RenderUnit->MaxVertexCount = MaxVertexCount;
    RenderUnit->MaxIndexCount = MaxIndexCount;
    RenderUnit->IsImmediate = IsImmediate;
    RenderUnit->IsOverlay = IsOverlay;

    RenderUnit->Materials = MemoryArena_PushArray(Arena, RenderUnit->MaxMaterialCount, render_data_material);
    RenderUnit->Markers = MemoryArena_PushArray(Arena, RenderUnit->MaxMarkerCount, render_marker);

    PrepareVertexDataForRenderUnit(RenderUnit);

    RenderUnit->ShaderID = ShaderID;
}

void
BindTexturesForMaterial(render_data_material *Material)
{
    for (u32 TextureIndex = 0;
         TextureIndex < TEXTURE_TYPE_COUNT;
         ++TextureIndex)
    {
        // TODO: set sampler IDs for shader here?
        // Not sure if it's still expensive to "unbind" texture, set to ID = 0
        // Which this will keep doing for every new material, for unused textures.
        // Maybe it's better to only bind texture if there's a new texture ID in the material
        // But set uniform values for sampler IDs to not point them to any active texture?
        OpenGL_BindAndActivateTexture(TextureIndex, Material->TextureIDs[TextureIndex]);
    }
}
