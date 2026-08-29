#ifndef CAMERAMANAGER_H
#define CAMERAMANAGER_H

#include "CameraBase.h"
#include <map>
#include <string>
namespace CC
{
    class CameraManager {
    public:
        CameraManager();
        virtual ~CameraManager();

        static CameraManager* Get();

        void Update();
        void SetActiveCamera(const std::string& cameraName);
        void RegisterCamera(const std::string& cameraName, CameraBase* camera);
        Mat4x4& GetViewProjectionMatrix();
        Vector3 GetCameraPosition();

    private:
        static constexpr const char* FREE_CAMERA_NAME = "CameraFree";

        static CameraManager* instance;

        CameraBase* FindCamera(const std::string& cameraName) const;

        std::map<std::string, CameraBase*> cameraMap;
        CameraBase* activeCamera;
        std::string activeCameraName;
        bool isFreeCameraActive;

    };
}

#endif // CAMERAMANAGER_H
