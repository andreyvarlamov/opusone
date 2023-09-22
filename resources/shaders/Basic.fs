#version 330 core

out vec4 Out_FragColor;

in vec3 VS_LightDirectionTangentSpace;
in vec3 VS_ViewPositionTangentSpace;
in vec3 VS_FragmentPositionTangentSpace;
in vec4 VS_Color;
in vec2 VS_UV;

uniform sampler2D DiffuseMap;
uniform sampler2D SpecularMap;
uniform sampler2D EmissionMap;
uniform sampler2D NormalMap;

void main()
{
    vec3 Normal = vec3(0, 0, 1);
#if 0
    vec3 NormalTexel = texture(NormalMap, VS_UV).rgb;
    Normal = Normalize(NormalTexel * 2.0 - 1.0);
#endif

    float FragBrightness = 0.3;

    vec3 NormalizedLightDirection = normalize(-VS_LightDirectionTangentSpace);
    float DiffuseBrightness = max(dot(NormalizedLightDirection, Normal), 0.0);
    FragBrightness += DiffuseBrightness * 0.8;

#if 0
    vec3 NormalizedViewDirection = normalize(VS_ViewPositionTangentSpace - VS_FragmentPositionTangentSpace);
    vec3 HalfwayDirection = Normalize(NormalizedLightDirection + NormalizedViewDirection);
    float SpecularBrigthness = pow(max(dot(Normal, HalfDirwection), 0.0), 128.0);
    vec3 SpecularTexel = texture(SpecularMap, VS_UV).rgb;
    vec3 SpecularColor = SpecularColorSample * SpecularBrightness;

    vec3 EmissionColor = texture(EmissionMap, VS_UV).rgb;
#endif

    vec3 FragColor = texture(DiffuseMap, VS_UV).rgb;
    
    Out_FragColor = vec4(FragBrightness * FragColor, 1.0);
}
