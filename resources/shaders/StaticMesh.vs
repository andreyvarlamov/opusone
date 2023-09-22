#version 330 core

layout (location = 0) in vec3 In_Position;
layout (location = 1) in vec3 In_Tangent;
layout (location = 2) in vec3 In_Bitangent;
layout (location = 3) in vec3 In_Normal;
layout (location = 4) in vec4 In_Color;
layout (location = 5) in vec2 In_UV;

out vec3 VS_LightDirectionTangentSpace;
out vec3 VS_ViewPositionTangentSpace;
out vec3 VS_FragmentPositionTangentSpace;
out vec4 VS_Color;
out vec2 VS_UV;

uniform mat4 Projection;
uniform mat4 View;

uniform mat4 Model;

uniform vec3 LightDirection;
uniform vec3 ViewPosition;

void main()
{
    vec4 TransformedPosition = Model * vec4(In_Position, 1.0);
    gl_Position = Projection * View * TransformedPosition;
    VS_UV = In_UV;
        
    mat3 NormalMatrix = mat3(transpose(inverse(Model)));
    vec3 Tangent = normalize(NormalMatrix * In_Tangent);
    vec3 Bitangent = normalize(NormalMatrix * In_Bitangent);
    vec3 Normal = normalize(NormalMatrix * In_Normal);

    mat3 TBN = transpose(mat3(Tangent, Bitangent, Normal));
    VS_LightDirectionTangentSpace = TBN * LightDirection;
    VS_ViewPositionTangentSpace = TBN * ViewPosition;
    VS_FragmentPositionTangentSpace = TBN * vec3(TransformedPosition);
}
