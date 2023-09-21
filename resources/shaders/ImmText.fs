#version 330 core

out vec4 Out_FragColor;

in vec3 VS_UV;
in vec4 VS_Color;

uniform sampler2D FontAtlas;

void main()
{
        vec4 Texel = texture(FontAtlas, VS_UV.xy);
        Out_FragColor = VS_Color * vec4(Texel.rgb, Texel.a + VS_UV.z);
}