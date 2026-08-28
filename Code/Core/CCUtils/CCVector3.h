#ifndef CCVECTOR3_H
#define CCVECTOR3_H

#include <math.h>

namespace CC
{
    //===================================================================
    // Vector3: 3D point/vector class that prefers readability over
    // the performance of a pure C array approach.
    //===================================================================
    class Vector3
    {
public:
	Vector3() {x=0;y=0;z=0;};
	Vector3(float X, float Y, float Z):x(X),y(Y),z(Z){}

	// Pointer to array for opengl calls. 
    // Assumes x, y, z are contigous in memory.
	operator const float* () const { return &x; }

	///////////////////////////////////
	// Add two vectors
	///////////////////////////////////
	Vector3 operator+(Vector3 vector)
	{
		// Returns a vector without modifying this one
		return Vector3(vector.x + x, vector.y + y, vector.z + z);
	}

	///////////////////////////////////
	// Subtract a vector from this one
	///////////////////////////////////
	Vector3 operator-(Vector3 vector)
	{
		// Returns a vector without modifying this one
		return Vector3(x - vector.x, y - vector.y, z - vector.z);
	}

	///////////////////////////////////
	// Multiply by a scalar
	///////////////////////////////////
	Vector3 operator*(float num)
	{
		// Returns a vector without modifying this one
		return Vector3(x * num, y * num, z * num);
	}

	///////////////////////////////////
	// Divide by a scalar
	///////////////////////////////////
	Vector3 operator/(float num)
	{
		// Returns a vector without modifying this one
		return Vector3(x / num, y / num, z / num);
	}

	///////////////////////////////////
	// Cross Product
	///////////////////////////////////
	Vector3 Cross(Vector3 vector2)
	{
		Vector3 vNormal;

		// Calculate the cross product with
		// the non communitive equation
		vNormal.x = ((y * vector2.z) - (z * vector2.y));
		vNormal.y = ((z * vector2.x) - (x * vector2.z));
		vNormal.z = ((x * vector2.y) - (y * vector2.x));

		// Return the cross product
		return vNormal;
	};

	///////////////////////////////////
	// Dot Product
	///////////////////////////////////
	float Dot(Vector3 vector)
	{
		return (x * vector.x) + (y * vector.y) + (z * vector.z);
	}

	///////////////////////////////////
	// Magnitude of this vector
	// Expensive? - uses sqrt
	///////////////////////////////////
	float Magnitude(void)
	{
		return (float)sqrt( (x * x) + (y * y) + (z * z) );
	}

	///////////////////////////////////
	// Normalization of this vector (length = 1);
	///////////////////////////////////
	void Normalize(void)
	{
		float magnitude = Magnitude();

		x = x/magnitude;
		y = y/magnitude;
		z = z/magnitude;
	}

	///////////////////////////////////
	// Set vector values directly
	///////////////////////////////////
	void SetValues(float xNew, float yNew, float zNew)
	{
		x = xNew;
		y = yNew;
		z = zNew;
	}

	///////////////////////////////////
	// The vector position/direction.
	///////////////////////////////////
	float x;
	float y;
	float z;
    };
}

#endif // CCVECTOR3_H
