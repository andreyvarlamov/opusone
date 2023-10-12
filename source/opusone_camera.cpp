#include "opusone_camera.h"

#include "opusone_platform.h"
#include "opusone_common.h"
#include "opusone_math.h"
#include "opusone_linmath.h"

internal void
RecalculateInternals(camera *Camera)
{
    Camera->Front_ = GetCartesianVecFromYawPitch(Camera->Yaw, Camera->Pitch);
    Camera->Right_ = VecNormalize(VecCross(Camera->Front_, Vec3(0,1,0)));
    Camera->Up_ = VecCross(Camera->Right_, Camera->Front_);

    Camera->IsDirty_ = false;
}

void
CameraSetPositionInLocalSpace(camera *Camera, vec3 DeltaPosition)
{
    if (Camera->IsDirty_)
    {
        RecalculateInternals(Camera);
    }

    Camera->Position += DeltaPosition.X * Camera->Right_;
    Camera->Position += DeltaPosition.Y * Camera->Up_;
    Camera->Position += -DeltaPosition.Z * Camera->Front_;
}

void
CameraSetWorldPosition(camera *Camera, vec3 Position)
{
    Camera->Position = Position;
}

void
CameraSetDeltaOrientation(camera *Camera, f32 DeltaYaw, f32 DeltaPitch, f32 DeltaRadius)
{
    Camera->Yaw += DeltaYaw;
    if (Camera->Yaw < 0.0f)
    {
        Camera->Yaw += 360.0f;
    }
    else if (Camera->Yaw > 360.0f)
    {
        Camera->Yaw -= 360.0f;
    }
    
    Camera->Pitch += DeltaPitch;
    if (Camera->Pitch < -89.0f)
    {
        Camera->Pitch = -89.0f;
    }
    else if (Camera->Pitch > 89.0f)
    {
        Camera->Pitch = 89.0f;
    }
    
    Camera->ThirdPersonRadius += DeltaRadius;
    if (Camera->ThirdPersonRadius < 0.0f)
    {
        Camera->ThirdPersonRadius = 0.0f;
    }
    else if (Camera->ThirdPersonRadius > 100.0f)
    {
        Camera->ThirdPersonRadius = 100.0f;
    }

    Camera->IsDirty_ = true;
}

void
CameraSetIsThirdPerson(camera *Camera, b32 IsThirdPerson)
{
    Camera->IsThirdPerson = IsThirdPerson;
}

void
CameraSetOrientation(camera *Camera, f32 Yaw, f32 Pitch)
{
    Camera->Yaw = Yaw;
    while (Camera->Yaw > 360.0f)
    {
        Camera->Yaw -= 360.0f;
    }
    while (Camera->Yaw < 0.0f)
    {
        Camera->Yaw += 360.0f;
    }

    Camera->Pitch = Pitch;
    if (Camera->Pitch < -89.0f)
    {
        Camera->Pitch = -89.0f;
    }
    else if (Camera->Pitch > 89.0f)
    {
        Camera->Pitch = 89.0f;
    }

    Camera->IsDirty_ = true;
}

void
CameraSetYawFromQuat(camera *Camera, quat Q)
{
    // TODO: Make it work properly with an arbitrary quaternion, not just around y axis
    f32 Yaw = ToDegreesF(2.0f * ArcCosF(Q.W));
    Camera->Yaw = Yaw;
    Camera->IsDirty_ = true;
}

mat4
CameraGetViewMat(camera *Camera)
{
    if (Camera->IsDirty_)
    {
        RecalculateInternals(Camera);
    }

    f32 Radius = Camera->IsThirdPerson ? Camera->ThirdPersonRadius : 0.0f;
    mat4 Result = Mat4GetView(Camera->Position, Camera->Front_, Camera->Right_, Camera->Up_, Radius);

    return Result;
}

vec3
CameraGetFront(camera *Camera)
{
    if (Camera->IsDirty_)
    {
        RecalculateInternals(Camera);
    }

    return Camera->Front_;
}

vec3
CameraGetTruePosition(camera *Camera)
{
    if (Camera->IsDirty_)
    {
        RecalculateInternals(Camera);
    }

    f32 Radius = Camera->IsThirdPerson ? Camera->ThirdPersonRadius : 0.0f;
    vec3 Result = Camera->Position - Radius * Camera->Front_;
    return Result;
}

quat
CameraGetYawQuat(camera *Camera)
{
    // TODO: Make it work properly with an arbitrary quaternion, not just around y axis
    quat Result = Quat(Vec3(0,1,0), ToRadiansF(Camera->Yaw));
    return Result;
}
