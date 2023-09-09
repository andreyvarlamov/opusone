#version 330 core

layout (location = 0) in vec3 In_Position;
layout (location = 1) in vec3 In_Color;

out vec3 VS_Color;

uniform mat4 Projection;

void main()
{
        gl_Position = Projection * vec4(In_Position, 1.0);
        VS_Color = In_Color;
}