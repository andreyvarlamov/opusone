#version 330 core

out vec4 Out_FragColor;

in vec3 VS_Color;

void main()
{
        Out_FragColor = vec4(VS_Color, 1.0);
}