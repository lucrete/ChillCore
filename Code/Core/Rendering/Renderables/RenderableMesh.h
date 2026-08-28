#ifndef RENDERABLEMESH_H
#define RENDERABLEMESH_H

#include "Renderable.h"

namespace CC
{
    class RenderableMesh : public Renderable
    {
    public:
        RenderableMesh(Material* material);
        virtual ~RenderableMesh();

        virtual const char* GetTypeName() const override { return "RenderableMesh"; }

        // Initialize with 8-float vertices (position3 + uv2 + normal3)
        void Initialize(const float* vertices, const unsigned int* indices, int verticesSize, int indicesSize);

        // Initialize with 12-float vertices (position3 + uv2 + normal3 + tangent4)
        void InitializeWithTangents(const float* vertices, const unsigned int* indices, int verticesSize, int indicesSize);

        virtual int Render(void* renderInfo) override;
        virtual void AddToRenderList() override;
        void SetIndexCount(int indexCount) { m_IndexCount = indexCount; }

        // Accessors for renderables that share geometry but need their own
        // pipeline (e.g. RenderableSphere uses a shared mesh but each
        // instance has its own material and therefore its own shader/pipeline).
        Gfx::BufferHandle GetVertexBuffer() const { return vertexBuffer; }
        Gfx::BufferHandle GetIndexBuffer()  const { return indexBuffer; }
        int               GetVertexStride() const { return vertexStride; }
        int               GetIndexCount()   const { return m_IndexCount; }

    protected:
        Gfx::BufferHandle   vertexBuffer;
        Gfx::BufferHandle   indexBuffer;
        int                 m_IndexCount;
        int                 vertexStride;
    };
}

#endif // RENDERABLEMESH_H
