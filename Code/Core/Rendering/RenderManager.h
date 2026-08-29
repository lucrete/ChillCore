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

        // ========================
        // Offscreen scene pass
        // ========================
        //
        // The frame loop opens the scene render pass in StartFrame, before
        // any AppState code runs, so a general caller cannot bracket its own
        // pass ahead of the backbuffer one. This is the affordance for that:
        // when set, the opaque and transparent passes render into `target`
        // instead of the backbuffer, then `postMaterial` is drawn as a
        // fullscreen pass sampling `colorTexture` (bound at texture unit 0)
        // into the backbuffer. Pass an invalid handle / null material, or
        // call ClearOffscreenScenePass, to restore the direct path.
        //
        // Ownership of `target`, `colorTexture`, and `postMaterial` stays
        // with the caller. General callers use this rather than the
        // Gfx::RenderApi pass brackets directly, which RenderManager owns.
        void SetOffscreenScenePass(Gfx::RenderTargetHandle target,
                                   Gfx::TextureHandle colorTexture,
                                   Material* postMaterial);
        void ClearOffscreenScenePass();
        bool HasOffscreenScenePass() const;

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

        struct OffscreenScenePass
        {
            Gfx::RenderTargetHandle target;
            Gfx::TextureHandle      colorTexture;
            Material*               postMaterial = nullptr;
            bool                    isEnabled = false;
        };
        OffscreenScenePass        offscreenScenePass;
        RenderableFullscreenQuad* offscreenPostQuad;
        Gfx::SamplerHandle        offscreenSampler;

        void SortTransparentRenderables();
        void SortOpaqueRenderables();
        void UploadFrameUniforms();

        // Second pass of an offscreen scene pass: draw offscreenPostQuad into
        // the backbuffer, sampling the offscreen colour texture. No-op unless
        // SetOffscreenScenePass is active. Called from Render after the scene
        // passes end.
        void DrawOffscreenPostPass();
        Gfx::RenderTargetHandle SceneTargetForFrame() const;

        static const int MAX_RENDERABLES = 1024;
        Renderable** renderables;
        int renderableCount;
        Renderable** transparentRenderables;
        int transparentRenderableCount;
        bool fullscreenQuad;
    };
}

#endif // RENDERMANAGER_H