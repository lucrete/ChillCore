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
        void UpdateTransform();

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
