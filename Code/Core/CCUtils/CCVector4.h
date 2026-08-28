#ifndef CCVECTOR4_H
#define CCVECTOR4_H

namespace CC
{
    //===================================================================
    // Vector4: 4D point/vector. Mirrors Vector3's shape; additional
    // arithmetic operators can be added alongside their first caller.
    //===================================================================
    class Vector4
    {
public:
	Vector4() {x=0;y=0;z=0;w=0;};
	Vector4(float X, float Y, float Z, float W):x(X),y(Y),z(Z),w(W){}

	// Pointer to array for opengl calls.
    // Assumes x, y, z, w are contiguous in memory.
	operator const float* () const { return &x; }

	///////////////////////////////////
	// Set vector values directly
	///////////////////////////////////
	void SetValues(float xNew, float yNew, float zNew, float wNew)
	{
		x = xNew;
		y = yNew;
		z = zNew;
		w = wNew;
	}

	///////////////////////////////////
	// The vector components.
	///////////////////////////////////
	float x;
	float y;
	float z;
	float w;
    };
}

#endif // CCVECTOR4_H
