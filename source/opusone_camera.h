#ifndef OPUSONE_CAMERA_H
#define OPUSONE_CAMERA_H

#include "opusone_common.h"
#include "opusone_linmath.h"

struct camera
{
    vec3 Position;
    f32 Yaw;
    f32 Pitch;
    f32 ThirdPersonRadius;
    b32 IsThirdPerson;

    b32 IsDirty_;
    vec3 Front_;
    vec3 Up_;
    vec3 Right_;
};

void
CameraSetPositionInLocalSpace(camera *Camera, vec3 DeltaPosition);

void
CameraSetWorldPosition(camera *Camera, vec3 Position);

void
CameraSetDeltaOrientation(camera *Camera, f32 DeltaYaw, f32 DeltaPitch, f32 DeltaRadius);

void
CameraSetIsThirdPerson(camera *Camera, b32 IsThirdPerson);

void
CameraSetOrientation(camera *Camera, f32 Yaw, f32 Pitch);

void
CameraSetYawFromQuat(camera *Camera, quat Q);

mat4
CameraGetViewMat(camera *Camera);

vec3
CameraGetFront(camera *Camera);

vec3
CameraGetTruePosition(camera *Camera);

quat
CameraGetYawQuat(camera *Camera);

#endif
