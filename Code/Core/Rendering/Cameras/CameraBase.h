#ifndef CAMERABASE_H
#define CAMERABASE_H

#include "CCVector3.h"
#include "CCMat4x4.h"

namespace CC
{
    class CameraBase {
    public:
        CameraBase();
        virtual ~CameraBase();

        virtual void Update();

        // Rebuilds view, projection and their product. Overridden where the
        // matrices come from somewhere other than a look-at and a symmetric
        // field of view.
        virtual void UpdateViewProjectionMatrix();

        Mat4x4& GetViewProjectionMatrix();
        const Mat4x4& GetViewMatrix() const;
        const Mat4x4& GetProjectionMatrix() const;
        Vector3 GetCameraPosition();

        void SetFieldOfViewDegrees(float degrees);
        float GetFieldOfViewDegrees() const;
        void SetNearDistance(float distance);
        float GetNearDistance() const;
        void SetFarDistance(float distance);
        float GetFarDistance() const;

        // Size of the surface this camera projects into, used for the
        // aspect ratio. Zero means follow the window, which is what a
        // camera rendering to the backbuffer wants. A camera rendering
        // into an offscreen target of a different shape sets it.
        void SetViewportSize(int width, int height);
        void GetViewportSize(int& outWidth, int& outHeight) const;

    protected:
        static constexpr float DEFAULT_FIELD_OF_VIEW_DEGREES = 75.0f;
        static constexpr float DEFAULT_NEAR_DISTANCE = 0.1f;
        static constexpr float DEFAULT_FAR_DISTANCE = 100.0f;

        Vector3 cameraPos = { 0.0f, 1.0f, 3.0f };
        Vector3 cameraLookAt = { 0.0f, 1.0f, -1.0f };
        Vector3 cameraUp = { 0.0f, 1.0f, 0.0f };

        Mat4x4 view;
        Mat4x4 projection;
        Mat4x4 viewProjection;

        float fieldOfViewDegrees = DEFAULT_FIELD_OF_VIEW_DEGREES;
        float nearDistance = DEFAULT_NEAR_DISTANCE;
        float farDistance = DEFAULT_FAR_DISTANCE;

        int viewportWidth = 0;
        int viewportHeight = 0;

        // Resolves the aspect ratio from the viewport size, falling back to
        // the window. Guards a zero-sized surface, which a minimised window
        // reports.
        float ResolveAspectRatio() const;
    };
}

#endif // CAMERABASE_H
