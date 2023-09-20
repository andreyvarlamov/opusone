#version 330 core

out vec4 Out_FragColor;

in vec4 VS_Color;

void main()
{
        Out_FragColor = VS_Color;
}