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
#include "RenderView.h"
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

        // The scene is rendered once per view. StartFrame builds a one-entry
        // list naming the offscreen scene target and the backbuffer, which is
        // a flat frame. A stereo caller replaces it with one entry per eye
        // before Render runs; the renderable list is collected once and
        // submitted per view.
        void SetRenderViews(const RenderView* viewArray, int count);

        // Returns to the built-in one-view frame. Call when the external
        // source stops supplying views, or the last set stays in use.
        void ClearRenderViews();
        int GetRenderViewCount() const { return viewCount; }
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
        // The opaque and transparent passes render into an offscreen colour
        // target, which the stack then resolves into the backbuffer. This
        // happens every frame, not only when an effect is enabled: the scene
        // is rendered in scene-linear light, and the stack's final pass is
        // what encodes it to display space. Individual effects still switch
        // on and off independently.
        //
        // The target belongs to RenderManager: created on demand at
        // framebuffer resolution, rebuilt when that or the sample count
        // changes. It is half-float where the backend can render to one, so
        // tone mapping and bloom have range above white to work with, and it
        // carries the backbuffer's sample count so antialiasing survives.
        PostProcess* GetPostProcess() const { return postProcess; }

        // Whether the offscreen target stands. False only where none could be
        // built, such as a zero-sized framebuffer.
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

        void BuildDefaultViewList();
        void RenderSingleView(const RenderView& view);

        // Draws the first view into the backbuffer so the desktop window
        // shows what the external target received. Ends with the default
        // framebuffer bound, which UI and the developer overlay draw into.
        void DrawMirrorPass();
        void EnsureMirrorResources();

        void SortTransparentRenderables();
        void SortOpaqueRenderables();
        void UploadFrameUniforms();

        // Creates the offscreen scene target, or rebuilds it at the new
        // resolution if the framebuffer has resized since it was created.
        void EnsurePostProcessSampler();
        void EnsurePostProcessTarget(int width, int height);
        void DestroyPostProcessTarget();

        // Resolve half of the frame: hands the offscreen colour texture to
        // the effect stack, which applies any enabled effects, encodes to
        // display space and draws into the backbuffer. Called from Render
        // after the scene passes end.
        void DrawPostProcessPass(const RenderView& view);
        Gfx::RenderTargetHandle SceneTargetForFrame() const;

        // Two is stereo. Nothing today asks for more, and a fixed array
        // keeps the per-frame list free of allocation.
        static const int MAX_RENDER_VIEWS = 2;
        RenderView views[MAX_RENDER_VIEWS];
        int        viewCount;
        // Whether the view list came from outside. The built-in list is
        // rebuilt every StartFrame; an external one is left alone, and the
        // scene target sizing that serves the built-in list is skipped
        // because each external view brings its own targets.
        bool       hasExternalViews;

        RenderableFullscreenQuad* mirrorQuad;
        Gfx::SamplerHandle        mirrorSampler;

        static const int MAX_RENDERABLES = 1024;
        Renderable** renderables;
        int renderableCount;
        Renderable** transparentRenderables;
        int transparentRenderableCount;
        bool fullscreenQuad;
    };
}

#endif // RENDERMANAGER_H