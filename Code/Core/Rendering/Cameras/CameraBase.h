#ifndef CAMERABASE_H
#define CAMERABASE_H

#include "CCVector3.h"
#include "CCMat4x4.h"

namespace CC
{
    class CameraBase {
    public:
        CameraBase();
        ~CameraBase();

        virtual void Update();
        void UpdateViewProjectionMatrix();
        Mat4x4& GetViewProjectionMatrix();
        Vector3 GetCameraPosition();

    protected:
        Vector3 cameraPos = { 0.0f, 1.0f, 3.0f };
        Vector3 cameraLookAt = { 0.0f, 1.0f, -1.0f };
        Vector3 cameraUp = { 0.0f, 1.0f, 0.0f };
        Mat4x4 viewProjection;

    };
}

#endif // CAMERABASE_H
