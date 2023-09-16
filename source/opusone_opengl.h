#ifndef OPUSONE_OPENGL_H
#define OPUSONE_OPENGL_H

#include <cstdio>
#include <glad/glad.h>

#include "opusone_common.h"

enum vertex_attrib_type
{
    VERTEX_ATTRIB_TYPE_FLOAT,
    VERTEX_ATTRIB_TYPE_INT,
    VERTEX_ATTRIB_TYPE_COUNT
};

struct vertex_attrib_spec
{
    u8 Stride;
    u8 ComponentCount;
    vertex_attrib_type Type;
    size_t ByteOffset;
};

inline void
CalculateVertexAttribOffsets(vertex_attrib_spec *AttribSpecs, u32 AttribCount, u32 VertexCount)
{
    Assert(AttribSpecs);
    Assert(AttribCount > 0);
    Assert(VertexCount > 0);
    
    size_t Offset = 0;
    
    for (u32 AttribIndex = 0;
         AttribIndex < AttribCount;
         ++AttribIndex)
    {
        vertex_attrib_spec *Spec = AttribSpecs + AttribIndex;
        Spec->ByteOffset = Offset;
        Offset += VertexCount * Spec->Stride;
    }
}

internal u32
CompileShaderFromPath_(const char *Path, u32 ShaderType)
{
    printf("Compiling shader at %s: ", Path);
    char *Source = PlatformReadFile(Path);
    u32 Shader = glCreateShader(ShaderType);
    glShaderSource(Shader, 1, &Source, 0);
    glCompileShader(Shader);
    PlatformFree(Source);
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
OpenGL_SetUniformVec3F(u32 ShaderID, const char *UniformName, f32 *Value, b32 UseProgram);

inline b32
OpenGL_SetUniformVec4F(u32 ShaderID, const char *UniformName, f32 *Value, b32 UseProgram);

inline b32
OpenGL_SetUniformMat3F(u32 ShaderID, const char *UniformName, f32 *Value, b32 UseProgram);

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
OpenGL_PrepareVertexData(u32 VertexCount, vertex_attrib_spec *AttribSpecs, u32 AttribCount,
                         u32 IndexCount, size_t IndexSize, b32 IsDynamicDraw,
                         u32 *Out_VAO, u32 *Out_VBO, u32 *Out_EBO)
{
    Assert(Out_VAO);
    Assert(AttribSpecs);
    Assert(AttribCount > 0);
    Assert(VertexCount > 0);

    glGenVertexArrays(1, Out_VAO);
    glBindVertexArray(*Out_VAO);
    Assert(*Out_VAO);

    u32 VBO;
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);

    size_t TotalVertexSize = 0;
    for (u32 AttribIndex = 0;
         AttribIndex < AttribCount;
         ++AttribIndex)
    {
        TotalVertexSize += AttribSpecs[AttribIndex].Stride;
    }
    glBufferData(GL_ARRAY_BUFFER, TotalVertexSize * VertexCount, 0, (IsDynamicDraw ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW));
    
    for (u32 AttribIndex = 0;
         AttribIndex < AttribCount;
         ++AttribIndex)
    {
        vertex_attrib_spec *Spec = AttribSpecs + AttribIndex;
        
        switch (Spec->Type)
        {
            case VERTEX_ATTRIB_TYPE_INT:
            {
                glVertexAttribIPointer(AttribIndex, Spec->ComponentCount, GL_INT, (GLsizei) Spec->Stride, (void *) Spec->ByteOffset);
            } break;

            default:
            {
                glVertexAttribPointer(AttribIndex, Spec->ComponentCount, GL_FLOAT, GL_FALSE,
                                      (GLsizei) Spec->Stride, (void *) Spec->ByteOffset);
            } break;
        }
        glEnableVertexAttribArray(AttribIndex);
    }
    
    Assert(VBO);
    if (Out_VBO) *Out_VBO = VBO;

    if (IndexCount > 0 && IndexSize > 0)
    {
        u32 EBO;
        glGenBuffers(1, &EBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, IndexCount * IndexSize, 0, (IsDynamicDraw ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW));

        Assert(EBO);
        if (Out_EBO) *Out_EBO = EBO;
    }
}

#if 0
void
OpenGL_SubVertexData(u32 VBO, size_t *AttribMaxBytes, size_t *AttribUsedBytes, void **AttribData, u32 AttribCount,
                     u32 EBO, size_t IndicesUsedBytes, void *IndicesData)
{
    Assert(VBO);
    Assert(AttribMaxBytes);
    Assert(AttribUsedBytes);
    Assert(AttribData);
    Assert(AttribCount > 0);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);

    size_t Offset = 0;
    for (u32 AttribIndex = 0;
         AttribIndex < AttribCount;
         ++AttribIndex)
    {
        if (AttribData[AttribIndex])
        {
            glBufferSubData(GL_ARRAY_BUFFER, Offset, AttribUsedBytes[AttribIndex], AttribData[AttribIndex]);
        }
        Offset += AttribMaxBytes[AttribIndex];
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    if (EBO > 0 && IndicesUsedBytes > 0 && IndicesData)
    {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, IndicesUsedBytes, IndicesData);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }
}
#endif

void
OpenGL_PrepareVertexData(render_unit *RenderUnit,
                         vertex_attrib_spec *AttribSpecs, u32 AttribCount, b32 IsDynamicDraw)
{
    Assert(RenderUnit);
    Assert(AttribSpecs);
    Assert(AttribCount > 0);

    OpenGL_PrepareVertexData(RenderUnit->MaxVertexCount, AttribSpecs, AttribCount,
                             RenderUnit->MaxIndexCount, sizeof(i32), IsDynamicDraw,
                             &RenderUnit->VAO, &RenderUnit->VBO, &RenderUnit->EBO);
}

void
OpenGL_SubVertexData(render_unit *RenderUnit,
                     void **AttribData, vertex_attrib_spec *AttribSpecs, u32 AttribCount, void *IndicesData,
                     u32 VertexToSubCount, u32 IndexToSubCount)
{
    Assert(RenderUnit);
    Assert(RenderUnit->VAO);
    Assert(RenderUnit->VBO);
    Assert(RenderUnit->EBO);
    Assert(AttribCount > 0);
    Assert(AttribSpecs);
    Assert(AttribData);
    Assert(IndicesData);
    Assert(VertexToSubCount > 0);
    Assert(IndexToSubCount > 0);
    Assert(RenderUnit->VertexCount + VertexToSubCount <= RenderUnit->MaxVertexCount);
    Assert(RenderUnit->IndexCount + IndexToSubCount <= RenderUnit->MaxIndexCount);
    
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, RenderUnit->VBO);

    for (u32 AttribIndex = 0;
         AttribIndex < AttribCount;
         ++AttribIndex)
    {
        vertex_attrib_spec *Spec = AttribSpecs + AttribIndex;

        if (AttribData[AttribIndex] && Spec->Stride > 0)
        {
            size_t BytesToCopy = VertexToSubCount * Spec->Stride;
            size_t Offset = Spec->ByteOffset + (RenderUnit->VertexCount * Spec->Stride);
            glBufferSubData(GL_ARRAY_BUFFER, Offset, BytesToCopy, AttribData[AttribIndex]);
        }
    }
    RenderUnit->VertexCount += VertexToSubCount;

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, RenderUnit->EBO);

    {
        size_t BytesToCopy = sizeof(i32) * IndexToSubCount;
        size_t Offset = sizeof(i32) * RenderUnit->IndexCount;
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, Offset, BytesToCopy, IndicesData);
    }
    RenderUnit->IndexCount += IndexToSubCount;

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

u32
OpenGL_LoadTexture(u8 *ImageData, u32 Width, u32 Height, u32 Pitch, u32 BytesPerPixel)
{
    // TODO: Handle different image data formats better
    Assert(Width * BytesPerPixel == Pitch);
    Assert(BytesPerPixel == 4);
    
    u32 TextureID;

    glGenTextures(1, &TextureID);
    glBindTexture(GL_TEXTURE_2D, TextureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, Width, Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, ImageData);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    return TextureID;
}

void
OpenGL_BindTexturesForMaterial(render_data_material *Material)
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
        glActiveTexture(GL_TEXTURE0 + TextureIndex);
        glBindTexture(GL_TEXTURE_2D, Material->TextureIDs[TextureIndex]);
    }

}

#endif
