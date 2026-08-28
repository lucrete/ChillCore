#include "CameraBase.h"
#include "RenderManager.h"
#include "CCMath.h"
namespace CC
{
    CameraBase::CameraBase() 
    {
        cameraLookAt = cameraPos + Vector3(0.0f, 0.0f, -1.0f);
    }

    CameraBase::~CameraBase() 
    {
    }

    void CameraBase::Update()
    {        
    }

    void CameraBase::UpdateViewProjectionMatrix()
    {
        Mat4x4 view;
        view.LookAt(cameraPos, cameraLookAt, cameraUp);

        int width = 0;
        int height = 0;
        RenderManager::Get()->GetWindowSize(width, height);
        float ratio = width / (float)height;

        // Todo only recalculate projection matrix if fov has changed
        Mat4x4 projection;
        float fov = 75;
        projection.Perspective(fov * (float)PI / 180.0f, ratio, 0.1f, 100.0f);
        viewProjection = projection * view;
    }

    Mat4x4& CameraBase::GetViewProjectionMatrix()
    {
        return viewProjection;
    }

    Vector3 CameraBase::GetCameraPosition()
    {
        return cameraPos;
    }
}
