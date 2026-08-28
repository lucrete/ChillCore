#ifndef CCVECTOR2_H
#define CCVECTOR2_H

#include <math.h>

namespace CC
{
    //===================================================================
    // Vector2: 2D point/vector class that prefers readability over
    // the performance of a pure C array approach.
    //===================================================================
    class Vector2
    {
public:
	Vector2() { x = 0; y = 0; };
	Vector2(float X, float Y) :x(X), y(Y) {}

	// Pointer to array for opengl calls. 
	// Assumes x, y are contigous in memory.
	operator const float* () const { return &x; }

	///////////////////////////////////
	// Add two vectors
	///////////////////////////////////
	Vector2 operator+(Vector2 vector)
	{
		// Returns a vector without modifying this one
		return Vector2(vector.x + x, vector.y + y);
	}

	///////////////////////////////////
	// Subtract a vector from this one
	///////////////////////////////////
	Vector2 operator-(Vector2 vector)
	{
		// Returns a vector without modifying this one
		return Vector2(x - vector.x, y - vector.y);
	}

	///////////////////////////////////
	// Multiply by a scalar
	///////////////////////////////////
	Vector2 operator*(float num)
	{
		// Returns a vector without modifying this one
		return Vector2(x * num, y * num);
	}

	///////////////////////////////////
	// Divide by a scalar
	///////////////////////////////////
	Vector2 operator/(float num)
	{
		// Returns a vector without modifying this one
		return Vector2(x / num, y / num);
	}

	///////////////////////////////////
	// Magnitude of this vector
	// Expensive? - uses sqrt
	///////////////////////////////////
	float Magnitude(void)
	{
		return (float)sqrt((x * x) + (y * y));
	}

	///////////////////////////////////
	// Normalization of this vector (length = 1);
	///////////////////////////////////
	void Normalize(void)
	{
		float magnitude = Magnitude();

		x = x / magnitude;
		y = y / magnitude;
	}

	///////////////////////////////////
	// Set vector values directly
	///////////////////////////////////
	void SetValues(float xNew, float yNew)
	{
		x = xNew;
		y = yNew;
	}

	///////////////////////////////////
	// The vector position/direction.
	///////////////////////////////////
	float x;
	float y;
    };
}

#endif // CCVECTOR2_H