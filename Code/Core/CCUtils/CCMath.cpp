#include "CCMath.h"
#include <stdio.h>
#include <math.h>

float CC::Math::Clamp(float min, float max, float inVal)
{
    float retVal = inVal;
    if (inVal > max)
    {
        retVal = max;
    }
    else if (inVal < min)
    {
        retVal = min;
    }

    return retVal;
}

CC::Vector3 CC::Math::RotateVectorAroundAxis(Vector3 vecIn, Vector3 axis, float angle)
{
    Vector3 retVal;

    float cosTheta = (float)cos(angle);
    float sinTheta = (float)sin(angle);

    retVal.x  = (cosTheta + (1 - cosTheta) * axis.x * axis.x)       * vecIn.x;
    retVal.x += ((1 - cosTheta) * axis.x * axis.y - axis.z * sinTheta)  * vecIn.y;
    retVal.x += ((1 - cosTheta) * axis.x * axis.z + axis.y * sinTheta)  * vecIn.z;

    retVal.y  = ((1 - cosTheta) * axis.x * axis.y + axis.z * sinTheta)  * vecIn.x;
    retVal.y += (cosTheta + (1 - cosTheta) * axis.y * axis.y)       * vecIn.y;
    retVal.y += ((1 - cosTheta) * axis.y * axis.z - axis.x * sinTheta)  * vecIn.z;

    retVal.z  = ((1 - cosTheta) * axis.x * axis.z - axis.y * sinTheta)  * vecIn.x;
    retVal.z += ((1 - cosTheta) * axis.y * axis.z + axis.x * sinTheta)  * vecIn.y;
    retVal.z += (cosTheta + (1 - cosTheta) * axis.z * axis.z) * vecIn.z;

    return retVal;
}

float CC::Math::Cos(float rads)
{
    return (float)cos(rads);
}

float CC::Math::Sin(float rads)
{
    return (float)sin(rads);
}
