#include "opusone_common.h"
#include "opusone_platform.h"
#include "opusone.h"
#include "opusone_opengl.h"
#include "opusone_math.h"
#include "opusone_linmath.h"

#include <cstdio>
#include <glad/glad.h>

void
UpdateCameraOrientation(camera *Camera, f32 DeltaRadius, f32 DeltaTheta, f32 DeltaPhi)
{
    f32 OldTheta = Camera->Theta;
    Camera->Theta += DeltaTheta;
    if (Camera->Theta < 0.0f)
    {
        Camera->Theta += 360.0f;
    }
    else if (Camera->Theta > 360.0f)
    {
        Camera->Theta -= 360.0f;
    }

    f32 OldPhi = Camera->Phi;
    Camera->Phi += DeltaPhi;
    if (Camera->Phi > 179.0f)
    {
        Camera->Phi = 179.0f;
    }
    else if (Camera->Phi < 1.0f)
    {
        Camera->Phi = 1.0f;
    }

    f32 OldRadius = Camera->Radius;
    Camera->Radius += DeltaRadius;
    if (Camera->Radius < 1.0f)
    {
        Camera->Radius = 1.0f;
    }
    
    if (!Camera->IsLookAround)
    {
        vec3 TranslationFromOldPositionToTarget = OldRadius * -VecSphericalToCartesian(OldTheta, OldPhi);
        vec3 TranslationFromTargetToNewPosition = OldRadius * VecSphericalToCartesian(Camera->Theta, Camera->Phi);
        Camera->Position += TranslationFromOldPositionToTarget + TranslationFromTargetToNewPosition;
    }
}

void
UpdateCameraPosition(camera *Camera, vec3 DeltaPositionLocal)
{
    vec3 Front = -VecSphericalToCartesian(Camera->Theta, Camera->Phi);
    vec3 Right = VecCross(Front, vec3 { 0.0f, 1.0f, 0.0f });
    vec3 Up = VecCross(Right, Front);
    mat3 LocalToWorld = Mat3FromCols(Right, Up, Front);
    Camera->Position += LocalToWorld * DeltaPositionLocal;
}

mat4
GetCameraViewMat(camera *Camera)
{
    vec3 Front = -VecSphericalToCartesian(Camera->Theta, Camera->Phi);
    vec3 CameraTarget;
    if (Camera->IsLookAround)
    {
        CameraTarget = Camera->Position + Front;
    }
    else
    {
        CameraTarget = Camera->Position + Camera->Radius * Front;
    }

    mat4 Result = GetViewMat(Camera->Position, CameraTarget, vec3 { 0.0f, 1.0f, 0.0f });
    return Result;
}

void
GameUpdateAndRender(game_input *GameInput, game_memory *GameMemory, b32 *GameShouldQuit)
{
    game_state *GameState = (game_state *) GameMemory->Storage;
    if (!GameMemory->IsInitialized)
    {
        GameState->Shader = OpenGL_BuildShaderProgram("resources/shaders/Basic.vs", "resources/shaders/Basic.fs");

        size_t AttribStrides[] = { sizeof(vec3), sizeof(vec3) };
        u32 AttribComponentCounts[] = { 3, 3 };
        OpenGL_PrepareVertexData(4, AttribStrides, AttribComponentCounts, ArrayCount(AttribStrides),
                                 6, sizeof(i32), true,
                                 &GameState->VAO, &GameState->VBO, &GameState->EBO);

        GameState->IndexCount = 6;

        GameState->Angle = 0.0f;

        GameState->Camera.Position = vec3 { 0.0f, 0.0f, 5.0f };
        GameState->Camera.Radius = 5.0f;
        GameState->Camera.Theta = 0.0f;
        GameState->Camera.Phi = 90.0f;
        GameState->Camera.IsLookAround = true;
        
        GameMemory->IsInitialized = true;
    }

    if (GameInput->CurrentKeyStates[SDL_SCANCODE_ESCAPE])
    {
        *GameShouldQuit = true;
    }

    f32 CameraRotationSensitivity = 0.1f;
    f32 CameraTranslationSensitivity = 0.01f;

    vec3 CameraTranslation = {};
    f32 CameraDeltaRadius = 0.0f;
    f32 CameraDeltaTheta = 0.0f;
    f32 CameraDeltaPhi = 0.0f;

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

    UpdateCameraOrientation(&GameState->Camera, CameraDeltaRadius, CameraDeltaTheta, CameraDeltaPhi);
    UpdateCameraPosition(&GameState->Camera, CameraTranslation);

    if (GameInput->CurrentKeyStates[SDL_SCANCODE_HOME] && !GameInput->KeyWasDown[SDL_SCANCODE_HOME])
    {
        GameState->Camera.IsLookAround = !GameState->Camera.IsLookAround;
        GameInput->KeyWasDown[SDL_SCANCODE_HOME] = true;
    }
    else if (!GameInput->CurrentKeyStates[SDL_SCANCODE_HOME])
    {
        GameInput->KeyWasDown[SDL_SCANCODE_HOME] = false;
    }

    glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    GameState->Angle += 100.0f * GameInput->DeltaTime;
    if (GameState->Angle > 360.0f)
    {
        GameState->Angle -= 360.0f;
    }

    mat3 Rotation = Mat3GetRotationAroundAxis(VecNormalize(vec3 { 0.0f, 1.0f, 1.0f }), ToRadiansF(GameState->Angle));
    
    vec3 Positions[] = {
        Rotation * vec3 { -1.0f, -1.0f, 0.0f },
        Rotation * vec3 {  1.0f, -1.0f, 0.0f },
        Rotation * vec3 {  1.0f,  1.0f, 0.0f },
        Rotation * vec3 { -1.0f,  1.0f, 0.0f }
    };
    vec3 Colors[] = {
        vec3 { 1.0f, 0.0f, 0.0f },
        vec3 { 0.0f, 1.0f, 0.0f },
        vec3 { 0.0f, 0.0f, 1.0f },
        vec3 { 1.0f, 1.0f, 1.0f }
    };

    size_t AttribMaxBytes[] = { sizeof(Positions), sizeof(Colors) };
    size_t AttribUsedBytes[] = { sizeof(Positions), sizeof(Colors) };

    void *AttribData[] = { Positions, Colors };
    i32 IndicesData[] = { 0, 1, 3,  3, 1, 2 };
    size_t IndicesUsedBytes = sizeof(IndicesData);

    OpenGL_SubVertexData(GameState->VBO, AttribMaxBytes, AttribUsedBytes, AttribData, ArrayCount(AttribData),
                         GameState->EBO, IndicesUsedBytes, IndicesData);

    OpenGL_UseShader(GameState->Shader);

    mat4 ProjectionMat = GetPerspecitveProjectionMat(70.0f, (f32) GameInput->ScreenWidth / (f32) GameInput->ScreenHeight, 0.1f, 1000.0f);
    OpenGL_SetUniformMat4F(GameState->Shader, "Projection", (f32 *) &ProjectionMat, false);
    mat4 ViewMat = GetCameraViewMat(&GameState->Camera);
    OpenGL_SetUniformMat4F(GameState->Shader, "View", (f32 *) &ViewMat, false);
    
    glBindVertexArray(GameState->VAO);
    glDrawElements(GL_TRIANGLES, GameState->IndexCount, GL_UNSIGNED_INT, 0);
}
