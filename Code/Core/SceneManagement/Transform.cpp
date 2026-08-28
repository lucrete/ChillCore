#include "Transform.h"
#include <math.h>

namespace CC
{
    Transform::Transform()
    {
        position = Vector3(0.0f, 0.0f, 0.0f);
        rotation = Quaternion(); // Identity quaternion
        scale = Vector3(1.0f, 1.0f, 1.0f);
    }

    Transform::~Transform()
    {
    }

    void Transform::Rotate(const Vector3& eulerAngles)
    {
        // Convert euler rotation to quaternion and multiply with current rotation
        Quaternion rotationQuat = Quaternion::FromEulerDegrees(eulerAngles.x, eulerAngles.y, eulerAngles.z);
        rotation = rotationQuat * rotation;
    }

    void Transform::RotateAxisAngle(const Vector3& axis, float angle)
    {
        // Convert angle from degrees to radians
        float radians = angle * 3.14159265358979323846f / 180.0f;

        // Create a quaternion from axis-angle and multiply with current rotation
        Quaternion rotationQuat = Quaternion::FromAxisAngle(axis, radians);
        rotation = rotationQuat * rotation;
    }

    void Transform::GetModelMatrix(Mat4x4& modelMatrix) const
    {
        modelMatrix.Identity();

        Mat4x4 scaleMatrix;
        scaleMatrix.SetScale(scale);

        Mat4x4 rotationMatrix;
        rotationMatrix.FromQuaternion(rotation);

        Mat4x4 translationMatrix;
        translationMatrix.SetTranslation(position);

        // Apply transformations in order: scale, then rotate, then translate
        Mat4x4 tempModelMatrix = rotationMatrix * scaleMatrix;
        modelMatrix = translationMatrix * tempModelMatrix;
    }
}