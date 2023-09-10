#ifndef OPUSONE_H
#define OPUSONE_H

#include "opusone_common.h"
#include "opusone_math.h"
#include "opusone_linmath.h"

struct camera
{
    vec3 Position;
    f32 Radius;
    f32 Theta;
    f32 Phi;
    b32 IsLookAround;
};

struct game_state
{
    u32 Shader;
    u32 VAO;
    u32 VBO;
    u32 EBO;
    u32 IndexCount;

    f32 Angle;

    camera Camera;
};

#endif
