#include "opusone_camera.h"

#include "opusone_platform.h"
#include "opusone_common.h"
#include "opusone_math.h"
#include "opusone_linmath.h"

#include <cstdio>

internal inline void
UpdateCameraOrientation(camera *Camera, f32 DeltaTheta, f32 DeltaPhi)
{
    Camera->Theta += DeltaTheta;
    if (Camera->Theta < 0.0f)
    {
        Camera->Theta += 360.0f;
    }
    else if (Camera->Theta > 360.0f)
    {
        Camera->Theta -= 360.0f;
    }
    Camera->Phi += DeltaPhi;
    if (Camera->Phi > 179.0f)
    {
        Camera->Phi = 179.0f;
    }
    else if (Camera->Phi < 1.0f)
    {
        Camera->Phi = 1.0f;
    }
}

void
UpdateCameraSphericalOrientation(camera *Camera, f32 DeltaRadius, f32 DeltaTheta, f32 DeltaPhi)
{
    vec3 TranslationFromOldPositionToTarget = Camera->Radius * -VecSphericalToCartesian(Camera->Theta, Camera->Phi);

    UpdateCameraOrientation(Camera, DeltaTheta, DeltaPhi);
    
    Camera->Radius += DeltaRadius;
    if (Camera->Radius < 1.0f)
    {
        Camera->Radius = 1.0f;
    }
    
    vec3 TranslationFromTargetToNewPosition = Camera->Radius * VecSphericalToCartesian(Camera->Theta, Camera->Phi);
    Camera->Position += TranslationFromOldPositionToTarget + TranslationFromTargetToNewPosition;
}

void
UpdateCameraForceRadius(camera *Camera, f32 Radius)
{
    vec3 TranslationFromOldPositionToTarget = Camera->Radius * -VecSphericalToCartesian(Camera->Theta, Camera->Phi);
    Camera->Position += TranslationFromOldPositionToTarget;
    
    Camera->Radius = Radius;
}

void
SetThirdPersonCameraTarget(camera *Camera, vec3 Position)
{
    Camera->Position = Position + Camera->Radius * VecSphericalToCartesian(Camera->Theta, Camera->Phi);
}

void
InitializeCamera(camera *Camera, vec3 Position, f32 Theta, f32 Phi, f32 Radius)
{
    Camera->Position = Position;
    Camera->Theta = Theta;
    Camera->Phi = Phi;
    Camera->Radius = Radius;
}

mat4
GetCameraViewMat(camera *Camera)
{
    vec3 Front = -VecSphericalToCartesian(Camera->Theta, Camera->Phi);
    vec3 CameraTarget = Camera->Position + Front;
    mat4 Result = Mat4GetView(Camera->Position, CameraTarget, vec3 { 0.0f, 1.0f, 0.0f });
    return Result;
}
