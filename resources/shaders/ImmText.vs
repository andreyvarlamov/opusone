#version 330 core

layout (location = 0) in vec2 In_Position;
layout (location = 1) in vec4 In_Color;
layout (location = 2) in vec3 In_UV;

out vec3 VS_UV;
out vec4 VS_Color;

void main()
{
        gl_Position = vec4(In_Position, 0.0f, 1.0f);
        VS_UV = In_UV;
        VS_Color = In_Color;
}