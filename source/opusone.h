#ifndef OPUSONE_H
#define OPUSONE_H

#include "opusone_common.h"
#include "opusone_math.h"
#include "opusone_linmath.h"
#include "opusone_camera.h"

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
