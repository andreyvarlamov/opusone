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

enum camera_control_scheme
{
    CAMERA_CONTROL_MOUSE = 0,
    CAMERA_CONTROL_FLY_AROUND,
    CAMERA_CONTROL_FPS,
    CAMERA_CONTROL_COUNT
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
    camera_control_scheme CameraControlScheme;
};

#endif
