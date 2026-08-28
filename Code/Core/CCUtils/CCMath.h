#ifndef CCMATH_H
#define CCMATH_H
#include "CCVector3.h"

namespace CC
{
    namespace Math
    {
        #define PI 3.1415927
        #define PIOVER2 1.57079635

        float Clamp(float min, float max, float inVal);
        Vector3 RotateVectorAroundAxis(Vector3 vecIn, Vector3 axis, float angle);
        float Cos(float rads);
        float Sin(float rads);
    }
}

#endif // CCMATH_H
