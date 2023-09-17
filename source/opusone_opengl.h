#ifndef OPUSONE_OPENGL_H
#define OPUSONE_OPENGL_H

#include <cstdio>
#include <glad/glad.h>

#include "opusone_common.h"
#include "opusone_linmath.h"
#include "opusone_vertspec.h"

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
OpenGL_PrepareVertexData(u32 VertexCount, vert_spec *VertSpec,
                         u32 IndexCount, size_t IndexSize, b32 IsDynamicDraw,
                         u32 *Out_VAO, u32 *Out_VBO, u32 *Out_EBO)
{
    Assert(Out_VAO);
    Assert(VertSpec);
    Assert(VertSpec->AttribCount > 0);
    Assert(VertexCount > 0);

    glGenVertexArrays(1, Out_VAO);
    glBindVertexArray(*Out_VAO);
    Assert(*Out_VAO);

    u32 VBO;
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);

    glBufferData(GL_ARRAY_BUFFER, VertSpec->VertexSize * VertexCount, 0, (IsDynamicDraw ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW));

    size_t AttribOffset = 0;
    
    for (u32 AttribIndex = 0;
         AttribIndex < VertSpec->AttribCount;
         ++AttribIndex)
    {
        vert_attrib_spec *Spec = VertSpec->AttribSpecs + AttribIndex;
        switch (Spec->Type)
        {
            case VERT_ATTRIB_TYPE_INT:
            {
                glVertexAttribIPointer(AttribIndex, Spec->ComponentCount, GL_INT, (GLsizei) Spec->Stride, (void *) AttribOffset);
            } break;

            default:
            {
                glVertexAttribPointer(AttribIndex, Spec->ComponentCount, GL_FLOAT, GL_FALSE,
                                      (GLsizei) Spec->Stride, (void *) AttribOffset);
            } break;
        }
        glEnableVertexAttribArray(AttribIndex);

        AttribOffset += Spec->Stride * VertexCount;
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

void
OpenGL_PrepareVertexData(render_unit *RenderUnit, b32 IsDynamicDraw)
{
    Assert(RenderUnit);

    OpenGL_PrepareVertexData(RenderUnit->MaxVertexCount, &RenderUnit->VertSpec,
                             RenderUnit->MaxIndexCount, sizeof(i32), IsDynamicDraw,
                             &RenderUnit->VAO, &RenderUnit->VBO, &RenderUnit->EBO);
}

void
OpenGL_SubVertexData(render_unit *RenderUnit, void **AttribData, void *IndicesData, u32 VertexCount, u32 IndexCount)
{
    Assert(RenderUnit);
    Assert(RenderUnit->VAO);
    Assert(RenderUnit->VBO);
    Assert(RenderUnit->EBO);
    Assert(RenderUnit->VertSpec.AttribCount > 0);
    Assert(AttribData);
    Assert(IndicesData);
    Assert(VertexCount > 0);
    Assert(IndexCount > 0);
    Assert(RenderUnit->VertexCount + VertexCount <= RenderUnit->MaxVertexCount);
    Assert(RenderUnit->IndexCount + IndexCount <= RenderUnit->MaxIndexCount);
    
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, RenderUnit->VBO);

    size_t AttribOffset = 0;
    
    for (u32 AttribIndex = 0;
         AttribIndex < RenderUnit->VertSpec.AttribCount;
         ++AttribIndex)
    {
        vert_attrib_spec *Spec = RenderUnit->VertSpec.AttribSpecs + AttribIndex;

        if (AttribData[AttribIndex] && Spec->Stride > 0)
        {
            size_t BytesToCopy = VertexCount * Spec->Stride;
            size_t BytesAlreadyUsed = RenderUnit->VertexCount * Spec->Stride;
            glBufferSubData(GL_ARRAY_BUFFER, AttribOffset + BytesAlreadyUsed, BytesToCopy, AttribData[AttribIndex]);
        }

        AttribOffset += RenderUnit->MaxVertexCount * Spec->Stride;
    }
    RenderUnit->VertexCount += VertexCount;

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, RenderUnit->EBO);

    {
        size_t BytesToCopy = sizeof(i32) * IndexCount;
        size_t BytesAlreadyUsed = sizeof(i32) * RenderUnit->IndexCount;
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, BytesAlreadyUsed, BytesToCopy, IndicesData);
    }
    RenderUnit->IndexCount += IndexCount;

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
