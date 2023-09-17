#ifndef OPUSONE_CAMERA_H
#define OPUSONE_CAMERA_H

#include "opusone_common.h"
#include "opusone_linmath.h"

enum camera_control_scheme
{
    CAMERA_CONTROL_MOUSE = 0,
    CAMERA_CONTROL_FLY_AROUND,
    CAMERA_CONTROL_FPS,
    CAMERA_CONTROL_COUNT
};

struct camera
{
    vec3 Position;
    f32 Theta;
    f32 Phi;
    f32 Radius;

    camera_control_scheme ControlScheme;
};

void
SetCameraControlScheme(camera *Camera, camera_control_scheme NewControlScheme);

void
InitializeCamera(camera *Camera, vec3 Position, f32 Theta, f32 Phi, f32 Radius, camera_control_scheme ControlScheme);

mat4
GetCameraViewMat(camera *Camera);

struct game_state;
struct game_input;
void
UpdateCameraForFrame(game_state *GameState, game_input *GameInput);

#endif
