#ifndef RENDERABLEFULLSCREENQUAD_H
#define RENDERABLEFULLSCREENQUAD_H
#include "Renderable.h"
#include <functional>

namespace CC
{
    class RenderableFullscreenQuad : public Renderable
    {
    public:
        RenderableFullscreenQuad(Material* material);
        virtual ~RenderableFullscreenQuad();

        virtual const char* GetTypeName() const override { return "RenderableFullscreenQuad"; }

        void PreRender();
        virtual int Render(void* renderInfo) override;
        virtual void AddToRenderList() override;
        Material* GetMaterial() const { return material; }

        using CallbackType = std::function<void(Material&)>;
        void SetMaterial(Material* newMaterial);
        void SetOnRenderCallback(CallbackType callback) { onRenderCallback = callback; }

    private:
        Gfx::BufferHandle   vertexBuffer;
        CallbackType onRenderCallback;
    };
}

#endif // RENDERABLEFULLSCREENQUAD_H
