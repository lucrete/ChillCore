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
#include "PostProcess.h"
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

        // Both take effect at the start of the next frame: rebuilding the
        // backbuffer mid-frame would pull the framebuffer out from under an
        // open render pass. The getters report the requested value straight
        // away, so a caller reads back what it just asked for.
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
        // Post-processing
        // ========================
        //
        // Enabling any effect on the stack redirects the opaque and
        // transparent passes into an offscreen colour target, which the
        // stack then resolves into the backbuffer. With every effect off
        // the scene renders straight to the backbuffer as before, and the
        // target is released.
        //
        // The target belongs to RenderManager: created on demand at
        // framebuffer resolution, rebuilt when that changes. It is
        // half-float where the backend can render to one, so tone mapping
        // and bloom have range above white to work with. Offscreen targets
        // are single-sample, so an active effect costs backbuffer
        // multisampling.
        PostProcess* GetPostProcess() const { return postProcess; }
        bool IsPostProcessEnabled() const;

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
        bool                       isBackbufferReconfigurePending;

        PostProcess*              postProcess;
        Gfx::SamplerHandle        postProcessSampler;
        Gfx::RenderTargetHandle   postProcessTarget;
        Gfx::TextureHandle        postProcessColorTexture;
        Gfx::TextureHandle        postProcessDepthTexture;
        int                       postProcessTargetWidth;
        int                       postProcessTargetHeight;
        int                       postProcessTargetSamples;

        void SortTransparentRenderables();
        void SortOpaqueRenderables();
        void UploadFrameUniforms();

        // Creates the offscreen scene target, or rebuilds it at the new
        // resolution if the framebuffer has resized since it was created.
        void EnsurePostProcessSampler();
        void EnsurePostProcessTarget(int width, int height);
        void DestroyPostProcessTarget();

        // Resolve half of a post-processed frame: hands the offscreen colour
        // texture to the effect stack, which draws into the backbuffer. No-op
        // unless an effect is enabled. Called from Render after the scene
        // passes end.
        void DrawPostProcessPass();
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