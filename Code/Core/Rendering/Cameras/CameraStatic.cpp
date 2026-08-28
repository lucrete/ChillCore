#include "CameraStatic.h"

namespace CC
{
    CameraStatic::CameraStatic()
    {
        cameraPos = { 0.0f, 0.5f, 5.0f };
        cameraLookAt = { 0.0f, 0.5f, 0.0f };
        cameraUp = { 0.0f, 1.0f, 0.0f };
    }

    CameraStatic::~CameraStatic() 
    {
    }
}
