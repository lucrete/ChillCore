#ifndef CAMERAFREE_H
#define CAMERAFREE_H

#include "CameraBase.h"

namespace CC
{
    class CameraFree : public CameraBase
    {
    public:
        CameraFree();
        ~CameraFree();

        void Update();
        // Devices are read only while the scene holds the Window target.
        // On-screen sticks are read either way: a value exists only while a
        // surface holding the target sets one.
        void UpdateTransform(bool isReadingDevices);

    private:
        float velocityMove;
        float velocityMoveTouch;
        float velocityRotateMouse;
        float velocityRotateGamepad;
        float velocityRotateTouch;
        float maxAngleUpDown;
        float angleUpDown;
        float angleLeftRight;


    };
}

#endif // CAMERAFREE_H
