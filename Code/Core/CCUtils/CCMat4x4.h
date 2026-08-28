#ifndef CCMAT4X4_H
#define CCMAT4X4_H
#include <math.h>
#include "CCVector3.h"
#include "CCQuaternion.h"

namespace CC
{
    //===================================================================
    // Mat4x4: 4x4 matrix class that prefers readability over
    // the performance of a pure C array approach.
    // Uses column-major indexing.
    //===================================================================
    class Mat4x4
    {
public:
	Mat4x4()
	{
		Identity();
	}

	Mat4x4(const float mat[4][4])
	{
		for (int col = 0; col < 4; col++)
		{
			for (int row = 0; row < 4; row++)
			{
				m[col][row] = mat[col][row];
			}
		}
	}

	// Pointer to array for opengl calls.
	// Assumes column-major matrices 
	// are contiguous in memory.
	operator const float* () const
	{
		return &m[0][0];
	}

	///////////////////////////////////
	// Set to identity matrix
	///////////////////////////////////
	void Identity(void)
	{
		for (int col = 0; col < 4; col++)
		{
			for (int row = 0; row < 4; row++)
			{
				m[col][row] = (col == row) ? 1.0f : 0.0f;
			}
		}
	}

	///////////////////////////////////
	// Matrix multiplication
	///////////////////////////////////
	Mat4x4 operator*(const Mat4x4& matrix) const
	{
		Mat4x4 result;

		for (int col = 0; col < 4; col++)
		{
			for (int row = 0; row < 4; row++)
			{
				result.m[col][row] = 0.0f;
				for (int k = 0; k < 4; k++)
				{
					// Standard matrix multiplication in column-major form
					result.m[col][row] += m[k][row] * matrix.m[col][k];
				}
			}
		}
		return result;
	}

	void SetScale(const Vector3& scale)
	{
		Identity();

		m[0][0] = scale.x;
		m[1][1] = scale.y;
		m[2][2] = scale.z;
	}

	void SetTranslation(const Vector3& position)
	{
		Identity();
		// In column-major, translation is in the 4th column
		m[3][0] = position.x;
		m[3][1] = position.y;
		m[3][2] = position.z;
	}

	///////////////////////////////////
	// Create a look-at matrix
	///////////////////////////////////
	void LookAt(Vector3 eye, Vector3 lookAt, Vector3 up)
	{
		Vector3 forward = lookAt - eye;
		forward.Normalize();

		Vector3 right = forward.Cross(up);
		right.Normalize();

		Vector3 cameraUp = right.Cross(forward);
		cameraUp.Normalize();

		// Set up the rotation part of the view matrix
		// For a view matrix, we're creating the inverse of the camera's rotation
		// The columns represent the camera's local basis vectors

		// First row
		m[0][0] = right.x;
		m[1][0] = right.y;
		m[2][0] = right.z;
		m[3][0] = 0.0f;

		// Second row
		m[0][1] = cameraUp.x;
		m[1][1] = cameraUp.y;
		m[2][1] = cameraUp.z;
		m[3][1] = 0.0f;

		// Third row - negate forward for OpenGL convention
		m[0][2] = -forward.x;
		m[1][2] = -forward.y;
		m[2][2] = -forward.z;
		m[3][2] = 0.0f;

		// Fourth row
		m[0][3] = 0.0f;
		m[1][3] = 0.0f;
		m[2][3] = 0.0f;
		m[3][3] = 1.0f;

		// Set the translation part (last column in column-major)
		// Apply the negative translation for the eye position
		Mat4x4 translation;
		translation.SetTranslation(Vector3(-eye.x, -eye.y, -eye.z));

		// Combine the rotation and translation
		*this = *this * translation;
	}

	///////////////////////////////////
	// Create a perspective projection matrix
	///////////////////////////////////
	void Perspective(float fovy, float aspect, float nearDistance, float farDistance)
	{
		float f = 1.0f / tanf(fovy * 0.5f);
		float nf = 1.0f / (nearDistance - farDistance);

		m[0][0] = f / aspect;
		m[0][1] = 0.0f;
		m[0][2] = 0.0f;
		m[0][3] = 0.0f;

		m[1][0] = 0.0f;
		m[1][1] = f;
		m[1][2] = 0.0f;
		m[1][3] = 0.0f;

		m[2][0] = 0.0f;
		m[2][1] = 0.0f;
		m[2][2] = (farDistance + nearDistance) * nf;
		m[2][3] = -1.0f;

		m[3][0] = 0.0f;
		m[3][1] = 0.0f;
		m[3][2] = 2.0f * farDistance * nearDistance * nf;
		m[3][3] = 0.0f;
	}

	///////////////////////////////////
	// Create an orthographic projection matrix
	///////////////////////////////////
	void Orthographic(float left, float right, float bottom, float top, float nearDist, float farDist)
	{
		float rl = 1.0f / (right - left);
		float tb = 1.0f / (top - bottom);
		float fn = 1.0f / (farDist - nearDist);

		m[0][0] = 2.0f * rl;
		m[0][1] = 0.0f;
		m[0][2] = 0.0f;
		m[0][3] = 0.0f;

		m[1][0] = 0.0f;
		m[1][1] = 2.0f * tb;
		m[1][2] = 0.0f;
		m[1][3] = 0.0f;

		m[2][0] = 0.0f;
		m[2][1] = 0.0f;
		m[2][2] = -2.0f * fn;
		m[2][3] = 0.0f;

		m[3][0] = -(right + left) * rl;
		m[3][1] = -(top + bottom) * tb;
		m[3][2] = -(farDist + nearDist) * fn;
		m[3][3] = 1.0f;
	}

	void FromQuaternion(const Quaternion& q)
	{
		Identity();

		float xx = q.x * q.x;
		float xy = q.x * q.y;
		float xz = q.x * q.z;
		float xw = q.x * q.w;

		float yy = q.y * q.y;
		float yz = q.y * q.z;
		float yw = q.y * q.w;

		float zz = q.z * q.z;
		float zw = q.z * q.w;

		// Fill the rotation part of the matrix (3x3 upper-left)
		// Column 3 and Row 3 stay as identity (0,0,0,1)
		// Column 0
		m[0][0] = 1.0f - 2.0f * (yy + zz);
		m[0][1] = 2.0f * (xy + zw);
		m[0][2] = 2.0f * (xz - yw);

		// Column 1
		m[1][0] = 2.0f * (xy - zw);
		m[1][1] = 1.0f - 2.0f * (xx + zz);
		m[1][2] = 2.0f * (yz + xw);

		// Column 2
		m[2][0] = 2.0f * (xz + yw);
		m[2][1] = 2.0f * (yz - xw);
		m[2][2] = 1.0f - 2.0f * (xx + yy);		
	}

	///////////////////////////////////
	// The matrix elements (column-major)
	// m[column][row]
	///////////////////////////////////
	float m[4][4];
    };
}

#endif // CCMAT4X4_H