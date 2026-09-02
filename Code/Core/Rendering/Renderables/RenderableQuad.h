#ifndef RENDERABLEQUAD_H
#define RENDERABLEQUAD_H
#include "Renderable.h"

namespace CC
{
    class RenderableQuad : public Renderable
    {
    public:
        RenderableQuad(Material* material);
        virtual ~RenderableQuad();

        virtual const char* GetTypeName() const override { return "RenderableQuad"; }

        virtual int Render(void* renderInfo) override;
        virtual void AddToRenderList() override;

    private:
        Gfx::BufferHandle   vertexBuffer;
        Gfx::BufferHandle   indexBuffer;

        // Quad definition. The tangent is constant: the quad is flat in its
        // local XY plane with u along +X, so no derivation is needed. Shaders
        // that declare no tangent attribute simply ignore it.
        float vertices[4 * 12] = {
            // positions          // texture coords  // normals         // tangents
            -0.5f, -0.5f, 0.0f,   0.0f, 0.0f,        0.0f, 0.0f, 1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
             0.5f, -0.5f, 0.0f,   1.0f, 0.0f,        0.0f, 0.0f, 1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
             0.5f,  0.5f, 0.0f,   1.0f, 1.0f,        0.0f, 0.0f, 1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
            -0.5f,  0.5f, 0.0f,   0.0f, 1.0f,        0.0f, 0.0f, 1.0f,  1.0f, 0.0f, 0.0f, 1.0f
        };

        unsigned int indices[6] = {
            0, 1, 2,
            0, 2, 3
        };
    };
}
#endif // RENDERABLEQUAD_H
