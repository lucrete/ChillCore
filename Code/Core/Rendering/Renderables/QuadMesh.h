#ifndef QUADMESH_H
#define QUADMESH_H

namespace CC
{
    // Two-triangle quad in clip space: position2 + texCoord2 interleaved.
    static constexpr float QUAD_VERTICES[] =
    {
        -1.0f,  1.0f,   0.0f, 1.0f,
        -1.0f, -1.0f,   0.0f, 0.0f,
         1.0f, -1.0f,   1.0f, 0.0f,
        -1.0f,  1.0f,   0.0f, 1.0f,
         1.0f, -1.0f,   1.0f, 0.0f,
         1.0f,  1.0f,   1.0f, 1.0f,
    };

    static constexpr int QUAD_VERTEX_COUNT = 6;
    static constexpr int QUAD_STRIDE_BYTES = 4 * sizeof(float);
}

#endif // QUADMESH_H
