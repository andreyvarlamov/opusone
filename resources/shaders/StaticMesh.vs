#version 330 core

layout (location = 0) in vec3 In_Position;
layout (location = 1) in vec3 In_Tangents;
layout (location = 2) in vec3 In_Bitangents;
layout (location = 3) in vec3 In_Normals;
layout (location = 4) in vec4 In_Color;
layout (location = 5) in vec2 In_UVs;

out vec3 VS_Color;
out vec2 VS_UVs;

uniform mat4 Projection;
uniform mat4 View;

uniform mat4 Model;

void main()
{
        gl_Position = Projection * View * Model * vec4(In_Position, 1.0);
        VS_UVs = In_UVs;
}