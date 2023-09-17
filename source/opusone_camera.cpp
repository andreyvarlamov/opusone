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

internal void
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

internal inline mat3
GetCameraLocalToWorldTransform(camera *Camera)
{
    vec3 Front = -VecSphericalToCartesian(Camera->Theta, Camera->Phi);
    vec3 Right = VecCross(Front, vec3 { 0.0f, 1.0f, 0.0f });
    vec3 Up = VecCross(Right, Front);
    mat3 LocalToWorld = Mat3FromCols(Right, Up, Front);
    return LocalToWorld;
}

internal void
UpdateCameraPosition(camera *Camera, vec3 DeltaPositionLocal)
{
    mat3 LocalToWorld = GetCameraLocalToWorldTransform(Camera);
    Camera->Position += LocalToWorld * DeltaPositionLocal;
}

internal void
UpdateCameraHorizontalPosition(camera *Camera, f32 MoveSpeedWorld, vec3 MoveDirLocal)
{
    mat3 LocalToWorld = GetCameraLocalToWorldTransform(Camera);
    vec3 DeltaPositionWorld = LocalToWorld * MoveDirLocal;
    DeltaPositionWorld.Y = 0.0f;
    Camera->Position += MoveSpeedWorld * VecNormalize(DeltaPositionWorld);
}

void
SetCameraControlScheme(camera *Camera, camera_control_scheme NewControlScheme)
{
    NewControlScheme = (camera_control_scheme) (((i32) NewControlScheme % (i32) CAMERA_CONTROL_COUNT));
    if (Camera->ControlScheme != CAMERA_CONTROL_MOUSE && NewControlScheme == CAMERA_CONTROL_MOUSE)
    {
        PlatformSetRelativeMouse(false);
    }
    else if (Camera->ControlScheme == CAMERA_CONTROL_MOUSE && NewControlScheme != CAMERA_CONTROL_MOUSE)
    {
        PlatformSetRelativeMouse(true);
    }

    printf("Camera control scheme: %d -> %d\n", Camera->ControlScheme, NewControlScheme);

    Camera->ControlScheme = NewControlScheme;
}

void
InitializeCamera(camera *Camera, vec3 Position, f32 Theta, f32 Phi, f32 Radius, camera_control_scheme ControlScheme)
{
    Camera->Position = Position;
    Camera->Theta = Theta;
    Camera->Phi = Phi;
    Camera->Radius = Radius;

    Camera->ControlScheme = (camera_control_scheme) 0;
    SetCameraControlScheme(Camera, ControlScheme);
}

mat4
GetCameraViewMat(camera *Camera)
{
    vec3 Front = -VecSphericalToCartesian(Camera->Theta, Camera->Phi);
    vec3 CameraTarget = Camera->Position + Front;
    mat4 Result = Mat4GetView(Camera->Position, CameraTarget, vec3 { 0.0f, 1.0f, 0.0f });
    return Result;
}

void
UpdateCameraForFrame(game_state *GameState, game_input *GameInput)
{
    f32 CameraRotationSensitivity = 0.1f;
    f32 CameraTranslationSensitivity = 0.01f;
    f32 CameraMovementSpeed = 5.0f;
    
    vec3 CameraTranslation = {};
    f32 CameraDeltaRadius = 0.0f;
    f32 CameraDeltaTheta = 0.0f;
    f32 CameraDeltaPhi = 0.0f;

    switch (GameState->Camera.ControlScheme)
    {
        case CAMERA_CONTROL_MOUSE:
        {
            if (GameInput->MouseButtonState[1])
            {
                if (GameInput->CurrentKeyStates[SDL_SCANCODE_LSHIFT] && GameInput->CurrentKeyStates[SDL_SCANCODE_LCTRL])
                {
                    CameraDeltaRadius = (f32) -GameInput->MouseDeltaY * CameraTranslationSensitivity;
                }
                else if (GameInput->CurrentKeyStates[SDL_SCANCODE_LSHIFT])
                {
                    CameraTranslation.X = (f32) -GameInput->MouseDeltaX * CameraTranslationSensitivity;
                    CameraTranslation.Y = (f32) GameInput->MouseDeltaY * CameraTranslationSensitivity;
                }
                else if (GameInput->CurrentKeyStates[SDL_SCANCODE_LCTRL])
                {
                    CameraDeltaRadius = (f32) GameInput->MouseDeltaY * CameraTranslationSensitivity;
                    CameraTranslation.Z = (f32) -GameInput->MouseDeltaY * CameraTranslationSensitivity;
                }
                else
                {
                    CameraDeltaTheta = (f32) -GameInput->MouseDeltaX * CameraRotationSensitivity;
                    CameraDeltaPhi = (f32) -GameInput->MouseDeltaY * CameraRotationSensitivity;
                }
            }

            UpdateCameraSphericalOrientation(&GameState->Camera, CameraDeltaRadius, CameraDeltaTheta, CameraDeltaPhi);
            UpdateCameraPosition(&GameState->Camera, CameraTranslation);
        } break;
        case CAMERA_CONTROL_FLY_AROUND:
        case CAMERA_CONTROL_FPS:
        {
            CameraDeltaTheta = (f32) -GameInput->MouseDeltaX * CameraRotationSensitivity;
            CameraDeltaPhi = (f32) -GameInput->MouseDeltaY * CameraRotationSensitivity;
            UpdateCameraOrientation(&GameState->Camera, CameraDeltaTheta, CameraDeltaPhi);

            if (GameInput->CurrentKeyStates[SDL_SCANCODE_W])
            {
                CameraTranslation.Z = 1.0f;
            }
            if (GameInput->CurrentKeyStates[SDL_SCANCODE_S])
            {
                CameraTranslation.Z = -1.0f;
            }
            if (GameInput->CurrentKeyStates[SDL_SCANCODE_A])
            {
                CameraTranslation.X = -1.0f;
            }
            if (GameInput->CurrentKeyStates[SDL_SCANCODE_D])
            {
                CameraTranslation.X = 1.0f;
            }
            if (GameState->Camera.ControlScheme == CAMERA_CONTROL_FLY_AROUND)
            {
                if (GameInput->CurrentKeyStates[SDL_SCANCODE_LSHIFT])
                {
                    CameraTranslation.Y = -1.0f;
                }
                if (GameInput->CurrentKeyStates[SDL_SCANCODE_SPACE])
                {
                    CameraTranslation.Y = 1.0f;
                }
                CameraTranslation = CameraMovementSpeed * GameInput->DeltaTime * VecNormalize(CameraTranslation);
                UpdateCameraPosition(&GameState->Camera, CameraTranslation);
            }
            else
            {
                UpdateCameraHorizontalPosition(&GameState->Camera, CameraMovementSpeed * GameInput->DeltaTime, VecNormalize(CameraTranslation));
            }
        } break;
        default:
        {
            InvalidCodePath;
        } break;
    }

    if (GameInput->CurrentKeyStates[SDL_SCANCODE_F1] && !GameInput->KeyWasDown[SDL_SCANCODE_F1])
    {
        SetCameraControlScheme(&GameState->Camera, (camera_control_scheme) ((i32) GameState->Camera.ControlScheme + 1));
        GameInput->KeyWasDown[SDL_SCANCODE_F1] = true;
    }
    else if (!GameInput->CurrentKeyStates[SDL_SCANCODE_F1])
    {
        GameInput->KeyWasDown[SDL_SCANCODE_F1] = false;
    }
}
