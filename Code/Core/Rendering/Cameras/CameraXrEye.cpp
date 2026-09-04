#include "CameraXrEye.h"

#include <math.h>

namespace CC
{
    CameraXrEye::CameraXrEye()
    {
    }

    CameraXrEye::~CameraXrEye()
    {
    }

    void CameraXrEye::SetPose(const Vector3& position, const Quaternion& _orientation)
    {
        cameraPos   = position;
        orientation = _orientation;
    }

    void CameraXrEye::SetFieldOfViewAngles(float _angleLeft, float _angleRight, float _angleUp, float _angleDown)
    {
        angleLeft  = _angleLeft;
        angleRight = _angleRight;
        angleUp    = _angleUp;
        angleDown  = _angleDown;
    }

    void CameraXrEye::UpdateViewProjectionMatrix()
    {
        // A view matrix is the inverse of the camera's world transform. The
        // pose is a rotation and a translation with no scale, so the inverse
        // is the conjugate rotation applied after moving the world back by
        // the camera position.
        Mat4x4 rotationInverse;
        rotationInverse.FromQuaternion(orientation.Inverse());

        Mat4x4 translation;
        translation.SetTranslation(Vector3(-cameraPos.x, -cameraPos.y, -cameraPos.z));

        view = rotationInverse * translation;

        // The runtime gives four half-angles rather than one field of view,
        // and they are not symmetric: the display sits off-centre in front of
        // the eye. Projected onto the near plane they are the frustum edges.
        float left   = tanf(angleLeft)  * nearDistance;
        float right  = tanf(angleRight) * nearDistance;
        float bottom = tanf(angleDown)  * nearDistance;
        float top    = tanf(angleUp)    * nearDistance;

        projection.Frustum(left, right, bottom, top, nearDistance, farDistance);

        viewProjection = projection * view;
    }
}
