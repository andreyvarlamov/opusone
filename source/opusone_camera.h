#ifndef OPUSONE_CAMERA_H
#define OPUSONE_CAMERA_H

#include "opusone_common.h"
#include "opusone_linmath.h"

struct camera
{
    vec3 Position;
    f32 Theta;
    f32 Phi;
    f32 Radius;
};

enum camera_control_scheme
{
    CAMERA_CONTROL_MOUSE = 0,
    CAMERA_CONTROL_FLY_AROUND,
    CAMERA_CONTROL_FPS,
    CAMERA_CONTROL_COUNT
};

#endif
