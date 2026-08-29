#include "CameraManager.h"
#include "CameraFree.h"
#include "CCAssert.h"
#include "InputManager.h"

namespace CC
{
    CameraManager* CameraManager::instance = NULL;

    CameraManager::CameraManager()
        : activeCamera(nullptr)
        , isFreeCameraActive(false)
    {
        instance = this;
        CameraBase* camera = new CameraFree();
        RegisterCamera(FREE_CAMERA_NAME, camera);
    }

    CameraManager::~CameraManager() 
    {
        cameraMap.clear();
    }

    CameraManager* CameraManager::Get()
    {
        return instance;
    }

    void CameraManager::RegisterCamera(const std::string& cameraName, CameraBase* camera)
    {
        cameraMap[cameraName] = camera;
    }

    void CameraManager::SetActiveCamera(const std::string& cameraName)
    {
        CameraBase* camera = FindCamera(cameraName);

        CC_ASSERT(camera != nullptr, "SetActiveCamera called with an unregistered camera name");

        if (camera != nullptr)
        {
            activeCamera = camera;
            activeCameraName = cameraName;
        }
    }

    CameraBase* CameraManager::FindCamera(const std::string& cameraName) const
    {
        CameraBase* camera = nullptr;
        std::map<std::string, CameraBase*>::const_iterator entry = cameraMap.find(cameraName);

        if (entry != cameraMap.end())
        {
            camera = entry->second;
        }

        return camera;
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
            CameraBase* toggled = FindCamera(isFreeCameraActive ? activeCameraName : FREE_CAMERA_NAME);

            if (toggled != nullptr)
            {
                activeCamera = toggled;
                isFreeCameraActive = !isFreeCameraActive;
                InputManager::Get()->LockMouseCursor(isFreeCameraActive);
            }
        }

        CC_ASSERT(activeCamera != nullptr, "No active camera; the AppState must call SetActiveCamera in Init");

        if (activeCamera != nullptr)
        {
            activeCamera->Update();
            activeCamera->UpdateViewProjectionMatrix();
        }
    }
}
