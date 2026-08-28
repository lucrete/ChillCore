#include "CameraFree.h"
#include "CCMath.h"
#include <iostream>
#include "InputManager.h"

namespace CC
{
    CameraFree::CameraFree()
        : velocityMove(10.0f)
        , velocityMoveTouch(5.0f)
        , velocityRotateMouse(0.2f)
        , velocityRotateGamepad(0.05f)
        , velocityRotateTouch(0.025f)
        , maxAngleUpDown(0.7f)
        , angleLeftRight(0.0f)
        , angleUpDown(0.0f)
    {
    }

    CameraFree::~CameraFree()
    {
    }

    void CameraFree::Update()
    {
        CC::InputManager* input = CC::InputManager::Get();
        if (input->IsWorldInteractable())
        {
            if (input->EdgePositiveMouseButton(true))
            {
                input->LockMouseCursor(true);
            }
            UpdateTransform();
        }
        if (input->EdgePositive(InputAction::DevReleaseMouse))
        {
            input->LockMouseCursor(false);
        }
    }

    void CameraFree::UpdateTransform()
    {
        float deltaSeconds = 0.016f;

        // Find the vector that is normal to both the up and the
        // direction the camera is pointing.
        Vector3 cameraXAxis = cameraLookAt - cameraPos;
        cameraXAxis = cameraXAxis.Cross(cameraUp);
        cameraXAxis.Normalize();

        CC::InputManager* input = CC::InputManager::Get();
        ActiveInputType inputType = input->GetActiveInputType();

        // Touch joysticks travel a much shorter distance than a gamepad
        // thumb, so dial movement and rotation back to half the gamepad
        // values when the user is on touch. Mouse keeps its own constant.
        float velocityMoveActive = inputType == ActiveInputType::Touch
            ? velocityMoveTouch
            : velocityMove;

        ///////////////////
        // Position
        ///////////////////
        float leftStickX = 0.0f;
        float leftStickY = 0.0f;
        input->GetAnalogStickValues(CC::InputManager::STICK_LEFT, leftStickX, leftStickY);

        // strafe
        float xDelta = 0.0f;
        if (leftStickX != 0.0f)
        {
            xDelta = leftStickX * velocityMoveActive * deltaSeconds;
        }
        else if (input->IsPressed(CC::InputAction::DevFreeCamMoveLeft))
        {
            xDelta = -velocityMoveActive * deltaSeconds;
        }
        else if (input->IsPressed(CC::InputAction::DevFreeCamMoveRight))
        {
            xDelta = velocityMoveActive * deltaSeconds;
        }

        Vector3 xPosDelta = cameraXAxis * xDelta;
        cameraPos = cameraPos + xPosDelta;
        cameraLookAt = cameraLookAt + xPosDelta;

        // Forward/Back
        float zDelta = 0.0f;
        if (leftStickY != 0.0f)
        {
            zDelta = leftStickY * velocityMoveActive * deltaSeconds;
        }
        else if (input->IsPressed(CC::InputAction::DevFreeCamMoveForward))
        {
            zDelta = velocityMoveActive * deltaSeconds;
        }
        else if (input->IsPressed(CC::InputAction::DevFreeCamMoveBackward))
        {
            zDelta = -velocityMoveActive * deltaSeconds;
        }

        Vector3 forward = cameraLookAt - cameraPos;
        forward.Normalize();
        Vector3 zPosDelta = forward * zDelta;
        cameraPos = cameraPos + zPosDelta;
        cameraLookAt = cameraLookAt + zPosDelta;

        // Up/Down
        float yDelta = 0.0f;
        if (input->IsPressed(CC::InputAction::DevFreeCamMoveUp))
        {
            yDelta = velocityMoveActive * deltaSeconds;
        }
        else if (input->IsPressed(CC::InputAction::DevFreeCamMoveDown))
        {
            yDelta = -velocityMoveActive * deltaSeconds;
        }
        Vector3 yPosDelta = cameraUp * yDelta;
        cameraPos = cameraPos + yPosDelta;
        cameraLookAt = cameraLookAt + yPosDelta;

        ///////////////////
        // Rotation
        ///////////////////
        float rightStickX = 0.0f;
        float rightStickY = 0.0f;
        input->GetAnalogStickValues(CC::InputManager::STICK_RIGHT, rightStickX, rightStickY);

        float velocityRotate;
        if (inputType == ActiveInputType::KeyboardMouse)
        {
            velocityRotate = velocityRotateMouse;
        }
        else if (inputType == ActiveInputType::Touch)
        {
            velocityRotate = -velocityRotateTouch;
        }
        else
        {
            velocityRotate = -velocityRotateGamepad;
        }
        float deltaAngleLeftRight = rightStickX * velocityRotate;
        angleLeftRight += deltaAngleLeftRight;

        float deltaAngleUpDown = rightStickY * velocityRotate;
        angleUpDown += deltaAngleUpDown;
        angleUpDown = Math::Clamp(-maxAngleUpDown, maxAngleUpDown, angleUpDown);

        // Pitch around world X first, then yaw around world Y. World-
        // fixed axes avoid gimbal lock — the previous code pitched around
        // a per-frame-derived right axis (forward × up), which becomes
        // parallel to the (0,0,1) base back vector once the camera has
        // yawed ~90 degrees, silently killing pitch in that orientation.
        Vector3 newTarget = Math::RotateVectorAroundAxis(Vector3(0, 0, 1), Vector3(1, 0, 0), angleUpDown);
        newTarget = Math::RotateVectorAroundAxis(newTarget, Vector3(0, 1, 0), angleLeftRight);
        cameraLookAt = cameraPos - newTarget;
    }
}