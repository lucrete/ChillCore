#ifndef TRANSFORM_H
#define TRANSFORM_H

#include "CCVector3.h"
#include "CCMat4x4.h"
#include "CCQuaternion.h"

namespace CC
{
    class Transform
    {
    public:
        Transform();
        virtual ~Transform();

        void SetPosition(const Vector3& _position) { position = _position; }
        void SetRotation(const Vector3& eulerAngles) { rotation = Quaternion::FromEulerDegrees(eulerAngles.x, eulerAngles.y, eulerAngles.z); }
        void SetRotationQuaternion(const Quaternion& quaternion) { rotation = quaternion; }
        void SetScale(const Vector3& _scale) { scale = _scale; }

        void Translate(const Vector3& translation) { position = position + translation; }
        void Rotate(const Vector3& eulerAngles);
        void RotateAxisAngle(const Vector3& axis, float angle);
        void Scale(const float _scale) { scale = scale * _scale; }

        const Vector3& GetPosition() const { return position; }
        Vector3 GetRotation() const { return rotation.ToEulerDegrees(); }
        const Quaternion& GetRotationQuaternion() const { return rotation; }
        const Vector3& GetScale() const { return scale; }

        void GetModelMatrix(Mat4x4& modelMatrix) const;

    private:
        Vector3 position;
        Quaternion rotation;
        Vector3 scale;
    };
}

#endif // TRANSFORM_H