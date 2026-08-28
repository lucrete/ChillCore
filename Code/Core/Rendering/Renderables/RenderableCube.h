#ifndef RENDERABLECUBE_H
#define RENDERABLECUBE_H
#include "RenderableMesh.h"

namespace CC
{
    class RenderableCube : public RenderableMesh
    {
    public:
        RenderableCube(Material* material);
        virtual ~RenderableCube();

        virtual const char* GetTypeName() const override { return "RenderableCube"; }

        virtual void AddToRenderList() override;

    private:
        // Cube definition with unique vertices for each face for proper texturing and normals
        float vertices[24 * 8] = {
            // positions          // texture coords   // normals
            // Front face (normal: 0, 0, 1)
            -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,     0.0f, 0.0f, 1.0f, // 0: front-bottom-left
             0.5f, -0.5f,  0.5f,  1.0f, 0.0f,     0.0f, 0.0f, 1.0f, // 1: front-bottom-right
             0.5f,  0.5f,  0.5f,  1.0f, 1.0f,     0.0f, 0.0f, 1.0f, // 2: front-top-right
            -0.5f,  0.5f,  0.5f,  0.0f, 1.0f,     0.0f, 0.0f, 1.0f, // 3: front-top-left

            // Back face (normal: 0, 0, -1)
            -0.5f, -0.5f, -0.5f,  1.0f, 0.0f,     0.0f, 0.0f, -1.0f, // 4: back-bottom-left
             0.5f, -0.5f, -0.5f,  0.0f, 0.0f,     0.0f, 0.0f, -1.0f, // 5: back-bottom-right
             0.5f,  0.5f, -0.5f,  0.0f, 1.0f,     0.0f, 0.0f, -1.0f, // 6: back-top-right
            -0.5f,  0.5f, -0.5f,  1.0f, 1.0f,     0.0f, 0.0f, -1.0f, // 7: back-top-left

            // Left face (normal: -1, 0, 0)
            -0.5f, -0.5f, -0.5f,  0.0f, 0.0f,    -1.0f, 0.0f, 0.0f, // 8: left-bottom-back
            -0.5f, -0.5f,  0.5f,  1.0f, 0.0f,    -1.0f, 0.0f, 0.0f, // 9: left-bottom-front
            -0.5f,  0.5f,  0.5f,  1.0f, 1.0f,    -1.0f, 0.0f, 0.0f, // 10: left-top-front
            -0.5f,  0.5f, -0.5f,  0.0f, 1.0f,    -1.0f, 0.0f, 0.0f, // 11: left-top-back

            // Right face (normal: 1, 0, 0)
             0.5f, -0.5f,  0.5f,  0.0f, 0.0f,     1.0f, 0.0f, 0.0f, // 12: right-bottom-front
             0.5f, -0.5f, -0.5f,  1.0f, 0.0f,     1.0f, 0.0f, 0.0f, // 13: right-bottom-back
             0.5f,  0.5f, -0.5f,  1.0f, 1.0f,     1.0f, 0.0f, 0.0f, // 14: right-top-back
             0.5f,  0.5f,  0.5f,  0.0f, 1.0f,     1.0f, 0.0f, 0.0f, // 15: right-top-front

             // Bottom face (normal: 0, -1, 0)
             -0.5f, -0.5f, -0.5f,  0.0f, 0.0f,    0.0f, -1.0f, 0.0f, // 16: bottom-left-back
              0.5f, -0.5f, -0.5f,  1.0f, 0.0f,    0.0f, -1.0f, 0.0f, // 17: bottom-right-back
              0.5f, -0.5f,  0.5f,  1.0f, 1.0f,    0.0f, -1.0f, 0.0f, // 18: bottom-right-front
             -0.5f, -0.5f,  0.5f,  0.0f, 1.0f,    0.0f, -1.0f, 0.0f, // 19: bottom-left-front

             // Top face (normal: 0, 1, 0)
             -0.5f,  0.5f,  0.5f,  0.0f, 0.0f,    0.0f, 1.0f, 0.0f, // 20: top-left-front
              0.5f,  0.5f,  0.5f,  1.0f, 0.0f,    0.0f, 1.0f, 0.0f, // 21: top-right-front
              0.5f,  0.5f, -0.5f,  1.0f, 1.0f,    0.0f, 1.0f, 0.0f, // 22: top-right-back
             -0.5f,  0.5f, -0.5f,  0.0f, 1.0f,    0.0f, 1.0f, 0.0f  // 23: top-left-back
        };

        unsigned int indices[36] = {
            // Front face
            0, 1, 2,
            0, 2, 3,

            // Back face
            5, 4, 7,
            5, 7, 6,

            // Left face
            8, 9, 10,
            8, 10, 11,

            // Right face
            12, 13, 14,
            12, 14, 15,

            // Bottom face
            16, 17, 18,
            16, 18, 19,

            // Top face
            20, 21, 22,
            20, 22, 23
        };
    };
}
#endif // RENDERABLECUBE_H