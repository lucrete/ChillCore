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
        // Post-processing
        // ========================
        //
        // Setting a material redirects the opaque and transparent passes
        // into an offscreen colour target, then draws that material as a
        // fullscreen pass into the backbuffer with the target's colour
        // texture bound at unit 0. The target belongs to RenderManager: it
        // is created on demand at framebuffer resolution and rebuilt when
        // that resolution changes, so callers only choose the effect.
        // Passing nullptr restores the direct-to-backbuffer path and
        // releases the target.
        //
        // The material stays owned by the caller. Offscreen targets are
        // single-sample, so an active effect costs backbuffer multisampling.
        void SetPostProcessMaterial(Material* material);
        Material* GetPostProcessMaterial() const;
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

        Material*                 postProcessMaterial;
        RenderableFullscreenQuad* postProcessQuad;
        Gfx::SamplerHandle        postProcessSampler;
        Gfx::RenderTargetHandle   postProcessTarget;
        Gfx::TextureHandle        postProcessColorTexture;
        Gfx::TextureHandle        postProcessDepthTexture;
        int                       postProcessTargetWidth;
        int                       postProcessTargetHeight;

        void SortTransparentRenderables();
        void SortOpaqueRenderables();
        void UploadFrameUniforms();

        // Creates the offscreen scene target, or rebuilds it at the new
        // resolution if the framebuffer has resized since it was created.
        void EnsurePostProcessTarget(int width, int height);
        void DestroyPostProcessTarget();

        // Second pass of a post-processed frame: draws postProcessQuad into
        // the backbuffer, sampling the offscreen colour texture. No-op unless
        // an effect is set. Called from Render after the scene passes end.
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