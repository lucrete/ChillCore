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
        view.LookAt(cameraPos, cameraLookAt, cameraUp);
        projection.Perspective(fieldOfViewDegrees * (float)PI / 180.0f, ResolveAspectRatio(), nearDistance, farDistance);
        viewProjection = projection * view;
    }

    Mat4x4& CameraBase::GetViewProjectionMatrix()
    {
        return viewProjection;
    }

    const Mat4x4& CameraBase::GetViewMatrix() const
    {
        return view;
    }

    const Mat4x4& CameraBase::GetProjectionMatrix() const
    {
        return projection;
    }

    Vector3 CameraBase::GetCameraPosition()
    {
        return cameraPos;
    }

    void CameraBase::SetFieldOfViewDegrees(float degrees)
    {
        fieldOfViewDegrees = degrees;
    }

    float CameraBase::GetFieldOfViewDegrees() const
    {
        return fieldOfViewDegrees;
    }

    void CameraBase::SetNearDistance(float distance)
    {
        nearDistance = distance;
    }

    float CameraBase::GetNearDistance() const
    {
        return nearDistance;
    }

    void CameraBase::SetFarDistance(float distance)
    {
        farDistance = distance;
    }

    float CameraBase::GetFarDistance() const
    {
        return farDistance;
    }

    void CameraBase::SetViewportSize(int width, int height)
    {
        viewportWidth = width;
        viewportHeight = height;
    }

    void CameraBase::GetViewportSize(int& outWidth, int& outHeight) const
    {
        outWidth = viewportWidth;
        outHeight = viewportHeight;
    }

    float CameraBase::ResolveAspectRatio() const
    {
        int width = viewportWidth;
        int height = viewportHeight;
        if (width <= 0 || height <= 0)
        {
            RenderManager::Get()->GetWindowSize(width, height);
        }

        float result = 1.0f;
        if (width > 0 && height > 0)
        {
            result = width / (float)height;
        }
        return result;
    }
}
