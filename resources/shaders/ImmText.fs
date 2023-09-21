#version 330 core

out vec4 Out_FragColor;

in vec2 VS_UV;

uniform sampler2D FontAtlas;
uniform vec4 Color;

void main()
{
        Out_FragColor = vec4(1.0f, 1.0f, 1.0f, 1.0f);
}