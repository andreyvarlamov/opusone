#version 330 core

layout (location = 0) in vec3  In_Position;
layout (location = 1) in vec3  In_Tangent;
layout (location = 2) in vec3  In_Bitangent;
layout (location = 3) in vec3  In_Normal;
layout (location = 4) in vec4  In_Color;
layout (location = 5) in vec2  In_UV;
layout (location = 6) in ivec4 In_BoneIDs;
layout (location = 7) in vec4  In_BoneWeights;

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

#define MAX_BONES 128
uniform mat4 BoneTransforms[MAX_BONES];

void main()
{
    vec4 P = vec4(In_Position, 1.0);
    vec4 B0_P = BoneTransforms[In_BoneIDs[0]] * P;
    vec4 B1_P = BoneTransforms[In_BoneIDs[1]] * P;
    vec4 B2_P = BoneTransforms[In_BoneIDs[2]] * P;
    vec4 B3_P = BoneTransforms[In_BoneIDs[3]] * P;
        
    vec4 BoneTransformedPosition = (In_BoneWeights[0] * B0_P +
                                    In_BoneWeights[1] * B1_P +
                                    In_BoneWeights[2] * B2_P +
                                    In_BoneWeights[3] * B3_P);

    vec4 TransformedPosition = Model * BoneTransformedPosition;
    gl_Position = Projection * View * TransformedPosition;
    VS_UV = In_UV;
        
    mat3 NormalMatrix0 = mat3(transpose(inverse(Model * BoneTransforms[In_BoneIDs[0]])));
    vec3 Tangent0 = normalize(NormalMatrix0 * In_Tangent);
    vec3 Bitangent0 = normalize(NormalMatrix0 * In_Bitangent);
    vec3 Normal0 = normalize(NormalMatrix0 * In_Normal);

    mat3 NormalMatrix1 = mat3(transpose(inverse(Model * BoneTransforms[In_BoneIDs[1]])));
    vec3 Tangent1 = normalize(NormalMatrix1 * In_Tangent);
    vec3 Bitangent1 = normalize(NormalMatrix1 * In_Bitangent);
    vec3 Normal1 = normalize(NormalMatrix1 * In_Normal);

    mat3 NormalMatrix2 = mat3(transpose(inverse(Model * BoneTransforms[In_BoneIDs[2]])));
    vec3 Tangent2 = normalize(NormalMatrix2 * In_Tangent);
    vec3 Bitangent2 = normalize(NormalMatrix2 * In_Bitangent);
    vec3 Normal2 = normalize(NormalMatrix2 * In_Normal);

    mat3 NormalMatrix3 = mat3(transpose(inverse(Model * BoneTransforms[In_BoneIDs[3]])));
    vec3 Tangent3 = normalize(NormalMatrix3 * In_Tangent);
    vec3 Bitangent3 = normalize(NormalMatrix3 * In_Bitangent);
    vec3 Normal3 = normalize(NormalMatrix3 * In_Normal);

    vec3 Tangent = (In_BoneWeights[0] * Tangent0 +
                    In_BoneWeights[1] * Tangent1 +
                    In_BoneWeights[2] * Tangent2 +
                    In_BoneWeights[3] * Tangent3);

    vec3 Bitangent = (In_BoneWeights[0] * Bitangent0 +
                      In_BoneWeights[1] * Bitangent1 +
                      In_BoneWeights[2] * Bitangent2 +
                      In_BoneWeights[3] * Bitangent3);

    vec3 Normal = (In_BoneWeights[0] * Normal0 +
                   In_BoneWeights[1] * Normal1 +
                   In_BoneWeights[2] * Normal2 +
                   In_BoneWeights[3] * Normal3);

    mat3 TBN = transpose(mat3(Tangent, Bitangent, Normal));
    VS_LightDirectionTangentSpace = TBN * LightDirection;
    VS_ViewPositionTangentSpace = TBN * ViewPosition;
    VS_FragmentPositionTangentSpace = TBN * vec3(TransformedPosition);
}
