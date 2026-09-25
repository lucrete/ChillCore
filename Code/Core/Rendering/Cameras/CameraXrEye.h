#ifndef CAMERAXREYE_H
#define CAMERAXREYE_H

#include "CameraBase.h"
#include "CCQuaternion.h"

namespace CC
{
    // One eye of a head-mounted display. The pose and the frustum come from
    // the runtime each frame, so neither the look-at nor the symmetric field
    // of view the base class builds from applies: both matrices are built
    // here instead.
    class CameraXrEye : public CameraBase
    {
    public:
        CameraXrEye();
        virtual ~CameraXrEye();

        virtual void UpdateViewProjectionMatrix() override;

        // Frustum half-angles in radians, signed from the view axis. Left and
        // down are negative, and the four are not symmetric.
        void SetPose(const Vector3& position, const Quaternion& orientation);
        void SetFieldOfViewAngles(float angleLeft, float angleRight, float angleUp, float angleDown);

    private:
        Quaternion orientation;

        float angleLeft  = 0.0f;
        float angleRight = 0.0f;
        float angleUp    = 0.0f;
        float angleDown  = 0.0f;
    };
}

#endif // CAMERAXREYE_H
