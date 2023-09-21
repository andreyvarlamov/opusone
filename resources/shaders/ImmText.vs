#version 330 core

layout (location = 0) in vec2 In_Position;
layout (location = 1) in vec2 In_UV;

out vec2 VS_UV;

int main()
{
        gl_Position = vec4(In_Position, 0.0f, 1.0f);
        VS_UV = In_UV;
}