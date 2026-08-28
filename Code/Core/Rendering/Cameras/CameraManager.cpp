#include "CameraManager.h"
#include "CameraFree.h"
#include "InputManager.h"

namespace CC
{
    CameraManager* CameraManager::instance = NULL;

    CameraManager::CameraManager()
        : isFreeCameraActive(false)
    {
        instance = this;
        CameraBase* camera = new CameraFree();
        RegisterCamera("CameraFree", camera);
    }

    CameraManager::~CameraManager() 
    {
        cameraMap.clear();
    }

    CameraManager* CameraManager::Get()
    {
        return instance;
    }

    void CameraManager::RegisterCamera(const char* cameraName, CameraBase* camera)
    {
        cameraMap[cameraName] = camera;
    }

    void CameraManager::SetActiveCamera(const char* cameraName)
    {
        activeCamera = cameraMap[cameraName];
        activeCameraName = cameraName;
    }

    Mat4x4& CameraManager::GetViewProjectionMatrix()
    {
        return activeCamera->GetViewProjectionMatrix();
    }

    Vector3 CameraManager::GetCameraPosition()
    {
        return activeCamera->GetCameraPosition();
    }

    void CameraManager::Update()
    {
        if (InputManager::Get()->IsWorldInteractable()
            && InputManager::Get()->EdgePositive(InputAction::DevToggleFreeCam))
        {
            activeCamera = cameraMap[isFreeCameraActive ? activeCameraName : "CameraFree"];
            isFreeCameraActive = !isFreeCameraActive;
            InputManager::Get()->LockMouseCursor(isFreeCameraActive);
        }

        activeCamera->Update();
        activeCamera->UpdateViewProjectionMatrix();
    }
}
