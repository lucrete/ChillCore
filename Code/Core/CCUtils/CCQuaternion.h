#ifndef CCQUATERNION_H
#define CCQUATERNION_H

#include "CCVector3.h"
#include <math.h>

namespace CC
{
    class Quaternion
    {
    public:        
        float x, y, z, w;
        
        Quaternion() : x(0.0f), y(0.0f), z(0.0f), w(1.0f) {}
        Quaternion(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
        
        static Quaternion FromAxisAngle(const Vector3& axis, float angle)
        {
            float halfAngle = angle * 0.5f;
            float s = sinf(halfAngle);
            return Quaternion(
                axis.x * s,
                axis.y * s,
                axis.z * s,
                cosf(halfAngle)
            );
        }

        static Quaternion FromEulerRadians(float x, float y, float z)
        {
            // Convert Euler angles to quaternion
            float cx = cosf(x * 0.5f);
            float sx = sinf(x * 0.5f);
            float cy = cosf(y * 0.5f);
            float sy = sinf(y * 0.5f);
            float cz = cosf(z * 0.5f);
            float sz = sinf(z * 0.5f);

            Quaternion q;
            q.w = cx * cy * cz + sx * sy * sz;
            q.x = sx * cy * cz - cx * sy * sz;
            q.y = cx * sy * cz + sx * cy * sz;
            q.z = cx * cy * sz - sx * sy * cz;
            return q;
        }

        static Quaternion FromEulerDegrees(float x, float y, float z)
        {
            const float DEG_TO_RAD = 3.14159265358979323846f / 180.0f;
            return FromEulerRadians(x * DEG_TO_RAD, y * DEG_TO_RAD, z * DEG_TO_RAD);
        }

        Vector3 ToEulerRadians() const
        {
            // Pitch (x-axis rotation)
            float sinr_cosp = 2.0f * (w * x + y * z);
            float cosr_cosp = 1.0f - 2.0f * (x * x + y * y);
            float pitch = atan2f(sinr_cosp, cosr_cosp);

            // Yaw (y-axis rotation)
            float sinp = 2.0f * (w * y - z * x);
            float yaw;
            if (fabsf(sinp) >= 1.0f)
            {
                yaw = copysignf(3.14159265358979323846f / 2.0f, sinp); // Use 90 degrees if out of range
            }
            else
            {
                yaw = asinf(sinp);
            }

            // Roll (z-axis rotation)
            float siny_cosp = 2.0f * (w * z + x * y);
            float cosy_cosp = 1.0f - 2.0f * (y * y + z * z);
            float roll = atan2f(siny_cosp, cosy_cosp);

            return Vector3(pitch, yaw, roll);
        }

        Vector3 ToEulerDegrees() const
        {
            const float RAD_TO_DEG = 180.0f / 3.14159265358979323846f;
            Vector3 euler = ToEulerRadians();
            return Vector3(euler.x * RAD_TO_DEG, euler.y * RAD_TO_DEG, euler.z * RAD_TO_DEG);
        }

        Quaternion Normalized() const
        {
            float magnitude = sqrtf(x * x + y * y + z * z + w * w);
            if (magnitude > 0.0f)
            {
                float invMagnitude = 1.0f / magnitude;
                return Quaternion(x * invMagnitude, y * invMagnitude, z * invMagnitude, w * invMagnitude);
            }
            return Quaternion(0.0f, 0.0f, 0.0f, 1.0f);
        }

        Quaternion operator*(const Quaternion& q) const
        {
            return Quaternion(
                w * q.x + x * q.w + y * q.z - z * q.y,
                w * q.y - x * q.z + y * q.w + z * q.x,
                w * q.z + x * q.y - y * q.x + z * q.w,
                w * q.w - x * q.x - y * q.y - z * q.z
            );
        }

        void ToMatrix4x4(float matrix[16]) const
        {
            float xx = x * x;
            float xy = x * y;
            float xz = x * z;
            float xw = x * w;
            float yy = y * y;
            float yz = y * z;
            float yw = y * w;
            float zz = z * z;
            float zw = z * w;

            matrix[0] = 1.0f - 2.0f * (yy + zz);
            matrix[1] = 2.0f * (xy - zw);
            matrix[2] = 2.0f * (xz + yw);
            matrix[3] = 0.0f;

            matrix[4] = 2.0f * (xy + zw);
            matrix[5] = 1.0f - 2.0f * (xx + zz);
            matrix[6] = 2.0f * (yz - xw);
            matrix[7] = 0.0f;

            matrix[8] = 2.0f * (xz - yw);
            matrix[9] = 2.0f * (yz + xw);
            matrix[10] = 1.0f - 2.0f * (xx + yy);
            matrix[11] = 0.0f;

            matrix[12] = 0.0f;
            matrix[13] = 0.0f;
            matrix[14] = 0.0f;
            matrix[15] = 1.0f;
        }

        // Rotate a vector by this quaternion
        Vector3 RotateVector(const Vector3& v) const
        {
            // Create a pure quaternion from the vector
            Quaternion vecQ(v.x, v.y, v.z, 0.0f);

            // Perform the rotation: q * v * q^-1
            Quaternion qInv(-x, -y, -z, w); // Conjugate (since quaternion is normalized)
            Quaternion result = (*this) * vecQ * qInv;

            return Vector3(result.x, result.y, result.z);
        }

        // Get the inverse (conjugate for unit quaternions)
        Quaternion Inverse() const
        {
            return Quaternion(-x, -y, -z, w);
        }
        
        static Quaternion Lerp(const Quaternion& a, const Quaternion& b, float t)
        {
            float t_ = 1.0f - t;
            return Quaternion(
                t_ * a.x + t * b.x,
                t_ * a.y + t * b.y,
                t_ * a.z + t * b.z,
                t_ * a.w + t * b.w
            ).Normalized();
        }

        static Quaternion Slerp(const Quaternion& a, const Quaternion& b, float t)
        {
            float dot = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;

            // Negate one of the quaternions 
            // if the dot is negative
            Quaternion q1 = a;
            Quaternion q2 = b;

            if (dot < 0.0f)
            {
                q2.x = -q2.x;
                q2.y = -q2.y;
                q2.z = -q2.z;
                q2.w = -q2.w;
                dot = -dot;
            }

            // Linear interpolation when close
            const float THRESHOLD = 0.9995f;
            if (dot > THRESHOLD)
            {
                return Lerp(q1, q2, t);
            }

            float angle = acosf(dot);
            float invSinAngle = 1.0f / sinf(angle);

            float scale0 = sinf((1.0f - t) * angle) * invSinAngle;
            float scale1 = sinf(t * angle) * invSinAngle;
            
            return Quaternion(
                scale0 * q1.x + scale1 * q2.x,
                scale0 * q1.y + scale1 * q2.y,
                scale0 * q1.z + scale1 * q2.z,
                scale0 * q1.w + scale1 * q2.w
            );
        }
    };
}

#endif // CCQUATERNION_H