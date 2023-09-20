#version 330 core

layout (location = 0) in vec3  In_Position;
layout (location = 1) in vec3  In_Tangent;
layout (location = 2) in vec3  In_Bitangent;
layout (location = 3) in vec3  In_Normal;
layout (location = 4) in vec4  In_Color;
layout (location = 5) in vec2  In_UV;
layout (location = 6) in ivec4 In_BoneIDs;
layout (location = 7) in vec4  In_BoneWeights;

out vec4 VS_Color;
out vec2 VS_UV;

uniform mat4 Projection;
uniform mat4 View;

uniform mat4 Model;

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

        gl_Position = Projection * View * Model * BoneTransformedPosition;
        VS_UV = In_UV;
}