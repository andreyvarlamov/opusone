#version 330 core

out vec4 Out_FragColor;

in vec3 VS_Color;
in vec2 VS_UVs;

uniform sampler2D DiffuseMap;
uniform sampler2D SpecularMap;
uniform sampler2D EmissionMap;
uniform sampler2D NormalMap;

void main()
{
        Out_FragColor = texture(DiffuseMap, VS_UVs);
}