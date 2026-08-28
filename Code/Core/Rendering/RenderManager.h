#ifndef RENDERMANAGER_H
#define RENDERMANAGER_H

#include "Renderable.h"
#include "TextureManager.h"
#include "MaterialManager.h"
#include "ShaderManager.h"
#include "CameraManager.h"
#include "LightManager.h"
#include "RenderableFullscreenQuad.h"
#include "GfxHandles.h"
#include "GfxDescriptions.h"
#include "PipelineCache.h"
#include "CCVector4.h"

namespace CC
{
    namespace Gfx { class RenderApi; }

    class RenderManager
    {
    public:
        RenderManager();
        virtual ~RenderManager();

        static RenderManager* Get();

        void StartFrame();
        void EndFrame();
        void AddRenderInfo(Renderable* renderable, void* info, int size);
        void Render();

        void GetWindowSize(int& width, int& height);

        void ToggleFullscreen();
        bool IsFullscreen() const;

        void SetAntialiasingEnabled(bool isEnabled);
        bool IsAntialiasingEnabled() const;

        void SetMsaaSamples(int samples);
        int GetMsaaSamples() const;

        void ToggleFullscreenQuad() { fullscreenQuad = !fullscreenQuad; }
        bool IsFullscreenQuad() const { return fullscreenQuad; }
        RenderableFullscreenQuad* GetRenderableFullscreenQuad() const { return renderableFullscreenQuad; }

        // Fullscreen fade overlay driven by StateMachine transitions (or any
        // caller that wants a post-UI tint). Color defaults to opaque black;
        // a fade alpha of 0 skips the draw. Drawn inside EndFrame, after
        // UI, before swap. SetScreenFadeAlpha updates just the alpha
        // component of the current color.
        void SetScreenFadeAlpha(float alpha);
        void SetScreenFadeColor(const Vector4& color);

        PipelineCache* GetPipelineCache() const { return pipelineCache; }

    private:
        static RenderManager* instance;
        Gfx::RenderApi* gfxApi;
        TextureManager* textureManager;
        ShaderManager* shaderManager;
        MaterialManager* materialManager;
        CameraManager* cameraManager;
        LightManager* lightManager;
        RenderableFullscreenQuad* renderableFullscreenQuad;
        RenderableFullscreenQuad* fadeOverlay;
        Vector4                   fadeColor;
        PipelineCache* pipelineCache;

        Gfx::BufferHandle     frameUniformBuffer;
        Gfx::BackbufferDescription backbufferDescription;

        void SortTransparentRenderables();
        void SortOpaqueRenderables();
        void UploadFrameUniforms();

        static const int MAX_RENDERABLES = 1024;
        Renderable** renderables;
        int renderableCount;
        Renderable** transparentRenderables;
        int transparentRenderableCount;
        bool fullscreenQuad;
    };
}

#endif // RENDERMANAGER_H