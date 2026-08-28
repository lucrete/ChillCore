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
        void SetActiveCamera(const char* cameraName);
        void RegisterCamera(const char* cameraName, CameraBase* camera);
        Mat4x4& GetViewProjectionMatrix();
        Vector3 GetCameraPosition();

    private:
        static CameraManager* instance;
        std::map<const char*, CameraBase*> cameraMap;
        CameraBase* activeCamera;
        const char* activeCameraName;
        bool isFreeCameraActive;

    };
}

#endif // CAMERAMANAGER_H
