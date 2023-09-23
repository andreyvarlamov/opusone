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

    vec3 DiffuseTexel = texture(DiffuseMap, VS_UV).rgb;
    float AmbientBrightness = 0.2;
    vec3 NormalizedLightDirection = normalize(-VS_LightDirectionTangentSpace);
    float DiffuseBrightness = max(dot(NormalizedLightDirection, Normal), 0.0);
    vec3 FragDiffuse = DiffuseTexel * (AmbientBrightness + ((1 - AmbientBrightness) * DiffuseBrightness));

    vec3 SpecularTexel = texture(SpecularMap, VS_UV).rgb;
    vec3 NormalizedViewDirection = normalize(VS_ViewPositionTangentSpace - VS_FragmentPositionTangentSpace);
    vec3 HalfwayDirection = normalize(NormalizedLightDirection + NormalizedViewDirection);
    float SpecularBrightness = pow(max(dot(Normal, HalfwayDirection), 0.0), 128.0);
    vec3 FragSpecular = SpecularTexel * SpecularBrightness;

    float EmissionBrightness = 0.5;
    vec3 FragEmission = texture(EmissionMap, VS_UV).rgb;
    
    Out_FragColor = vec4(FragDiffuse + FragSpecular + FragEmission, 1.0);
}
