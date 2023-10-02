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

internal void
UpdateCameraSphericalOrientation(camera *Camera, f32 DeltaRadius, f32 DeltaTheta, f32 DeltaPhi);

void
UpdateCameraForceRadius(camera *Camera, f32 Radius);

void
SetThirdPersonCameraTarget(camera *Camera, vec3 Position);

void
InitializeCamera(camera *Camera, vec3 Position, f32 Theta, f32 Phi, f32 Radius);

mat4
GetCameraViewMat(camera *Camera);

#endif
