#ifndef RENDERABLE_H
#define RENDERABLE_H

#include "Component.h"
#include "Material.h"
#include "Transform.h"
#include "GfxHandles.h"
#include "GfxDescriptions.h"

namespace CC
{
    class Renderable : public Component
    {
    public:
        Renderable(Material* material);
        virtual ~Renderable();

        virtual void Init() override {}
        virtual void Update() override;
        virtual void Shutdown() override {}

        virtual const char* GetTypeName() const override { return "Renderable"; }

        void PreRender();
        virtual int Render(void* renderInfo) = 0;
        virtual void AddToRenderList() = 0;
        void AddToRenderList(void* info, int size);
        void SetMaterial(Material* newMaterial);
        Material* GetMaterial() const { return material; }

        Gfx::PipelineHandle GetPipelineHandle() const { return pipelineHandle; }

    protected:
        Material* material;
        Gfx::PipelineHandle pipelineHandle;

        Transform& GetTransform();
        const Transform& GetTransform() const;

        // Build a pipeline whose shader + blend + depth state reflects the given
        // material and whose vertex input matches the given layout. Used by
        // subclasses at construction time.
        static Gfx::PipelineHandle CreatePipelineForMaterial(const Material* material,
                                                             const Gfx::VertexLayout& vertexLayout);
    };
}
#endif // RENDERABLE_H
