#ifndef OPUSONE_OPENGL_H
#define OPUSONE_OPENGL_H

#include <cstdio>
#include <glad/glad.h>

#include "opusone.h"

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

void
OpenGL_PrepareVertexData(u32 VertexCount, size_t *AttribStrides, u32 *AttribComponentCounts, u32 AttribCount,
                         u32 IndexCount, size_t IndexSize, b32 IsDynamicDraw,
                         u32 *Out_VAO, u32 *Out_VBO, u32 *Out_EBO)
{
    Assert(Out_VAO);
    Assert(AttribStrides);
    Assert(AttribComponentCounts);
    Assert(AttribCount > 0);
    Assert(VertexCount > 0);

    glGenVertexArrays(1, Out_VAO);
    glBindVertexArray(*Out_VAO);
    Assert(*Out_VAO);

    u32 VBO;
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    size_t TotalVertexSize = 0;
    for (u32 AttribIndex = 0; AttribIndex < AttribCount; ++AttribIndex)
    {
        TotalVertexSize += AttribStrides[AttribIndex];
    }
    
    glBufferData(GL_ARRAY_BUFFER, TotalVertexSize * VertexCount, 0, (IsDynamicDraw ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW));
    for (u32 AttribIndex = 0; AttribIndex < AttribCount; ++AttribIndex)
    {
        size_t StrideSize = AttribStrides[AttribIndex];
        size_t OffsetSize = 0;
        for (u32 Index = 0; Index < AttribIndex; ++Index)
        {
            OffsetSize += AttribStrides[Index] * VertexCount;
        }
        glVertexAttribPointer(AttribIndex, AttribComponentCounts[AttribIndex], GL_FLOAT, GL_FALSE,
                              (GLsizei) StrideSize, (void *) OffsetSize);
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
        glBufferSubData(GL_ARRAY_BUFFER, Offset, AttribUsedBytes[AttribIndex], AttribData[AttribIndex]);
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