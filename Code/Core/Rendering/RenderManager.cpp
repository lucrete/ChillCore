#include "RenderManager.h"
#include <memory.h>
#include <stdio.h>
#include <algorithm>
#include "CCAssert.h"
#include "PrintManager.h"
#include "PlatformWindow.h"
#include "GfxDescriptions.h"
#include "InputManager.h"
#include "LightManager.h"
#include "RenderableSphere.h"
#include "SceneObject.h"
#include "FrameTimer.h"
#include "FrameUniforms.h"
#include "LightDirectional.h"
#include "DevUi.h"
#include "TextRenderer.h"

#ifdef CC_GFX_BACKEND_GLES
    #include "GfxRenderApiGles.h"
#else
    #include "GfxRenderApiOpenGl.h"
#endif

namespace CC
{
    RenderManager* RenderManager::instance = nullptr;

    RenderManager::RenderManager()
        : renderableCount(0)
        , transparentRenderableCount(0)
        , fullscreenQuad(false)
        , fadeOverlay(nullptr)
        , fadeColor(0.0f, 0.0f, 0.0f, 0.0f)
        , postProcess(nullptr)
        , isBackbufferReconfigurePending(false)
        , postProcessTargetSamples(1)
        , postProcessTargetWidth(0)
        , postProcessTargetHeight(0)
    {
        CC_ASSERT(instance == nullptr, "RenderManager already created");
        instance = this;

        // Gfx backend first — its Init runs gladLoadGL on desktop which
        // all other GL-touching code depends on. The GLES backend is
        // selected on Android via CC_GFX_BACKEND_GLES.
#ifdef CC_GFX_BACKEND_GLES
        gfxApi = new Gfx::RenderApiGles();
#else
        gfxApi = new Gfx::RenderApiOpenGl();
#endif
        gfxApi->Init();

        // Seed the backbuffer descriptor (8x MSAA) and push it through —
        // this also creates the offscreen scene framebuffer inside the Gfx
        // backend on first call.
        backbufferDescription.sampleCount = 8;
        gfxApi->ConfigureBackbuffer(backbufferDescription);

        DevUi::Get()->Init();

        const Gfx::GfxCapabilities& caps = gfxApi->GetCapabilities();
        CCPrint(PrintManager::CHANNEL_RENDER, "Gfx backend: %s", caps.backendName);
        CCPrint(PrintManager::CHANNEL_RENDER, "Gfx device:  %s", caps.deviceName);
        CCPrint(PrintManager::CHANNEL_RENDER, "Gfx driver:  %s", caps.driverVersion);
        CCPrint(PrintManager::CHANNEL_ALWAYS,
                "Gfx caps: compute=%d ssbo=%d indirect=%d geom=%d tess=%d maxTex=%d maxCS=%dx%dx%d",
                caps.supportsComputeShaders ? 1 : 0,
                caps.supportsStorageBuffers ? 1 : 0,
                caps.supportsIndirectDraw ? 1 : 0,
                caps.supportsGeometryShader ? 1 : 0,
                caps.supportsTessellation ? 1 : 0,
                caps.maxTextureSize,
                caps.maxComputeWorkgroupSizeX,
                caps.maxComputeWorkgroupSizeY,
                caps.maxComputeWorkgroupSizeZ);

        renderables = new Renderable * [MAX_RENDERABLES];
        transparentRenderables = new Renderable * [MAX_RENDERABLES];

        textureManager = new TextureManager();
        shaderManager = new ShaderManager();
        materialManager = new MaterialManager();
        cameraManager = new CameraManager();
        lightManager = new LightManager();
        pipelineCache = new PipelineCache();

        Material* procArtMaterial = materialManager->GetMaterial("DefaultBasic");
        renderableFullscreenQuad = new RenderableFullscreenQuad(procArtMaterial);

        // Fullscreen fade overlay. Its material is alpha-blended so the
        // scene shows through until fadeColor.w drives a cover. Color and
        // alpha are set via SetScreenFadeColor / SetScreenFadeAlpha.
        Material* fadeMaterial = materialManager->GetMaterial("FullScreenFade");
        fadeMaterial->SetUniform("fadeColor", fadeColor);
        fadeOverlay = new RenderableFullscreenQuad(fadeMaterial);

        Gfx::BufferDescription frameUboDesc;
        frameUboDesc.sizeBytes = static_cast<int>(sizeof(FrameUniforms));
        frameUboDesc.usage     = Gfx::BufferUsage::Uniform;
        frameUboDesc.memory    = Gfx::BufferMemory::CpuToGpu;
        frameUboDesc.debugName = "FrameUniforms";
        frameUniformBuffer = gfxApi->CreateBuffer(frameUboDesc);

        RenderableSphere::InitSharedMesh();

        // After the material and shader managers exist: the stack builds its
        // own materials.
        postProcess = new PostProcess();
        postProcess->Init();
    }

    RenderManager::~RenderManager()
    {
        DevUi::Get()->Shutdown();

        RenderableSphere::ShutdownSharedMesh();

        if (frameUniformBuffer.IsValid())
        {
            gfxApi->DestroyBuffer(frameUniformBuffer);
        }

        if (postProcess != nullptr)
        {
            postProcess->Shutdown();
        }
        DestroyPostProcessTarget();

        if (postProcessSampler.IsValid())
        {
            gfxApi->DestroySampler(postProcessSampler);
        }
        delete postProcess;
        delete fadeOverlay;
        delete renderableFullscreenQuad;
        delete pipelineCache;
        delete lightManager;
        delete cameraManager;
        delete materialManager;
        delete shaderManager;
        delete textureManager;
        delete[] transparentRenderables;
        delete[] renderables;
        gfxApi->Shutdown();
        delete gfxApi;
        instance = nullptr;
    }

    RenderManager* RenderManager::Get()
    {
        CC_ASSERT(instance != nullptr, "RenderManager not created yet");
        return instance;
    }

    void RenderManager::StartFrame()
    {
        if (InputManager::Get()->EdgePositive(InputAction::DevToggleFullscreen))
        {
            PlatformWindow::Get()->ToggleFullscreen();
        }
        // Reconfiguring the backbuffer rebuilds framebuffers and leaves the
        // binding pointing elsewhere, so it can only run between passes.
        // Callers set the description whenever they like; it is applied here,
        // before the frame's pass opens.
        if (isBackbufferReconfigurePending)
        {
            gfxApi->ConfigureBackbuffer(backbufferDescription);
            isBackbufferReconfigurePending = false;
        }

        renderableCount = 0;
        transparentRenderableCount = 0;

        int width = 0;
        int height = 0;
        PlatformWindow::Get()->GetFramebufferSize(width, height);
        TextRenderer::Get()->BeginFrame(width, height);

        // The offscreen target is not optional. The scene is rendered in
        // scene-linear light and the post-process pass is what encodes it to
        // display space, so the pass runs every frame whether or not any
        // effect is enabled. Rendering the scene straight to the backbuffer
        // would present linear values as if they were sRGB.
        EnsurePostProcessTarget(width, height);

        // Begin the scene render pass, into the offscreen target whenever one
        // could be built. The backbuffer is the fallback for a zero-sized
        // framebuffer (a minimised window), where there is nothing to present.
        gfxApi->BeginRenderPass(SceneTargetForFrame());

        shaderManager->Update();
    }

    Gfx::RenderTargetHandle RenderManager::SceneTargetForFrame() const
    {
        Gfx::RenderTargetHandle result = gfxApi->GetBackbuffer();
        if (IsPostProcessEnabled())
        {
            result = postProcessTarget;
        }
        return result;
    }


    void RenderManager::UploadFrameUniforms()
    {
        FrameUniforms frameUniforms = {};

        const float* viewProj = cameraManager->GetViewProjectionMatrix();
        for (int i = 0; i < 16; i++)
        {
            frameUniforms.viewProj[i] = viewProj[i];
        }

        Vector3 cameraPosition = cameraManager->GetCameraPosition();
        frameUniforms.cameraPositionAndTime[0] = cameraPosition.x;
        frameUniforms.cameraPositionAndTime[1] = cameraPosition.y;
        frameUniforms.cameraPositionAndTime[2] = cameraPosition.z;
        frameUniforms.cameraPositionAndTime[3] = FrameTimer::Get()->TimeSinceStartup();

        Vector3 ambientColor = lightManager->GetAmbientLightColor();
        frameUniforms.ambientLightColorAndIntensity[0] = ambientColor.x;
        frameUniforms.ambientLightColorAndIntensity[1] = ambientColor.y;
        frameUniforms.ambientLightColorAndIntensity[2] = ambientColor.z;
        frameUniforms.ambientLightColorAndIntensity[3] = lightManager->GetAmbientLightIntensity();

        LightDirectional* directionalLight = lightManager->GetLight("DefaultLight");
        if (directionalLight != nullptr)
        {
            const Vector3& direction = directionalLight->GetDirection();
            frameUniforms.directionalLightDirAndIntensity[0] = direction.x;
            frameUniforms.directionalLightDirAndIntensity[1] = direction.y;
            frameUniforms.directionalLightDirAndIntensity[2] = direction.z;
            frameUniforms.directionalLightDirAndIntensity[3] = directionalLight->GetIntensity();

            const Vector3& color = directionalLight->GetColor();
            frameUniforms.directionalLightColor[0] = color.x;
            frameUniforms.directionalLightColor[1] = color.y;
            frameUniforms.directionalLightColor[2] = color.z;
        }

        gfxApi->UpdateBuffer(frameUniformBuffer, 0, static_cast<int>(sizeof(FrameUniforms)), &frameUniforms);
        gfxApi->BindUniformBuffer(FRAME_UNIFORMS_BINDING_SLOT, frameUniformBuffer, 0, static_cast<int>(sizeof(FrameUniforms)));
    }

    void RenderManager::EndFrame()
    {
        // Fade overlay: last draw before the swap so it covers both scene
        // and UI. Alpha 0 skips the draw. fadeColor is a CustomParams
        // member, so the write lands in the material's parameter block and
        // is uploaded on the spot. Order against PreRender does not matter.
        if (fadeColor.w > 0.0f)
        {
            fadeOverlay->PreRender();
            fadeOverlay->GetMaterial()->SetUniform("fadeColor", fadeColor);
            fadeOverlay->Render(nullptr);
        }

        gfxApi->EndFrame();
    }

    void RenderManager::SetScreenFadeAlpha(float alpha)
    {
        fadeColor.w = alpha;
    }

    void RenderManager::SetScreenFadeColor(const Vector4& color)
    {
        fadeColor = color;
    }

    // ========================
    // Post-processing
    // ========================

    bool RenderManager::IsPostProcessEnabled() const
    {
        return postProcessTarget.IsValid();
    }

    void RenderManager::EnsurePostProcessSampler()
    {
        if (!postProcessSampler.IsValid())
        {
            // Non-mipmapped, edge-clamped, linear. A mipmap-filtering
            // sampler on the single-level target texture reads as black
            // on some drivers (the same hazard the EndRenderPass blit
            // guards against).
            Gfx::SamplerDescription samplerDesc;
            samplerDesc.minFilter  = Gfx::FilterMode::Linear;
            samplerDesc.magFilter  = Gfx::FilterMode::Linear;
            samplerDesc.mipmapMode = Gfx::MipmapMode::None;
            samplerDesc.addressU   = Gfx::AddressMode::ClampToEdge;
            samplerDesc.addressV   = Gfx::AddressMode::ClampToEdge;
            samplerDesc.addressW   = Gfx::AddressMode::ClampToEdge;
            samplerDesc.debugName  = "RenderManager::PostProcessSampler";
            postProcessSampler = gfxApi->CreateSampler(samplerDesc);
        }
    }

    void RenderManager::EnsurePostProcessTarget(int width, int height)
    {
        // Sample count is part of what makes the target current: toggling
        // antialiasing has to rebuild it, or post-processed frames would keep
        // the sample count they were created with.
        bool isTargetCurrent = postProcessTarget.IsValid()
            && width == postProcessTargetWidth
            && height == postProcessTargetHeight
            && backbufferDescription.sampleCount == postProcessTargetSamples;

        if (!isTargetCurrent && width > 0 && height > 0)
        {
            DestroyPostProcessTarget();
            EnsurePostProcessSampler();

            // Half-float where the backend can render to one, so values above
            // white survive to be tone mapped instead of clipping in the
            // scene pass. Where it cannot, the stack still runs — tone mapping
            // simply has nothing above white left to compress.
            const Gfx::GfxCapabilities& caps = gfxApi->GetCapabilities();
            Gfx::TextureFormat colorFormat = caps.supportsHalfFloatRenderTargets
                ? Gfx::TextureFormat::Rgba16Float
                : Gfx::TextureFormat::Rgba8Unorm;

            Gfx::TextureDescription colorDesc;
            colorDesc.width          = width;
            colorDesc.height         = height;
            colorDesc.format         = colorFormat;
            colorDesc.isRenderTarget = true;
            colorDesc.debugName      = "RenderManager::PostProcessColor";
            postProcessColorTexture = gfxApi->CreateTexture(colorDesc);

            Gfx::TextureDescription depthDesc;
            depthDesc.width          = width;
            depthDesc.height         = height;
            depthDesc.format         = Gfx::TextureFormat::Depth24Stencil8;
            depthDesc.isRenderTarget = true;
            depthDesc.debugName      = "RenderManager::PostProcessDepth";
            postProcessDepthTexture = gfxApi->CreateTexture(depthDesc);

            Gfx::RenderTargetDescription targetDesc;
            targetDesc.width                = width;
            targetDesc.height               = height;
            targetDesc.colorAttachmentCount = 1;
            targetDesc.colorAttachments[0].texture       = postProcessColorTexture;
            targetDesc.colorAttachments[0].loadOp        = Gfx::LoadOp::Clear;
            targetDesc.colorAttachments[0].storeOp       = Gfx::StoreOp::Store;
            // Left at the attachment default of opaque black, which is what
            // the backbuffer scene pass clears to. A different clear colour
            // here would tint everything the scene does not cover, so an
            // effect that changes nothing would still change the frame.
            targetDesc.hasDepthStencil                   = true;
            targetDesc.depthStencilAttachment.texture    = postProcessDepthTexture;
            targetDesc.depthStencilAttachment.loadOp     = Gfx::LoadOp::Clear;
            targetDesc.depthStencilAttachment.storeOp    = Gfx::StoreOp::DontCare;
            // The scene keeps the backbuffer's antialiasing: the target is
            // multisampled and the backend resolves it into the colour texture
            // the effects sample.
            targetDesc.sampleCount                       = backbufferDescription.sampleCount;
            targetDesc.debugName                         = "RenderManager::PostProcessTarget";
            postProcessTarget = gfxApi->CreateRenderTarget(targetDesc);

            postProcessTargetWidth   = width;
            postProcessTargetHeight  = height;
            postProcessTargetSamples = backbufferDescription.sampleCount;
        }
    }

    void RenderManager::DestroyPostProcessTarget()
    {
        if (postProcessTarget.IsValid())
        {
            gfxApi->DestroyRenderTarget(postProcessTarget);
            postProcessTarget = Gfx::RenderTargetHandle();
        }
        if (postProcessColorTexture.IsValid())
        {
            gfxApi->DestroyTexture(postProcessColorTexture);
            postProcessColorTexture = Gfx::TextureHandle();
        }
        if (postProcessDepthTexture.IsValid())
        {
            gfxApi->DestroyTexture(postProcessDepthTexture);
            postProcessDepthTexture = Gfx::TextureHandle();
        }

        postProcessTargetWidth   = 0;
        postProcessTargetHeight  = 0;
        postProcessTargetSamples = 1;
    }

    void RenderManager::DrawPostProcessPass()
    {
        if (IsPostProcessEnabled())
        {
            postProcess->Execute(postProcessColorTexture, postProcessSampler,
                                 postProcessTargetWidth, postProcessTargetHeight);
        }
    }

    void RenderManager::AddRenderInfo(Renderable* renderable, void* info, int size)
    {
        if (renderable->GetMaterial()->IsTransparent())
        {
            CC_ASSERT(transparentRenderableCount < MAX_RENDERABLES, "Too many transparent renderables");
            transparentRenderables[transparentRenderableCount] = renderable;
            transparentRenderableCount++;
        }
        else
        {
            CC_ASSERT(renderableCount < MAX_RENDERABLES, "Too many renderables");
            renderables[renderableCount] = renderable;
            renderableCount++;
        }
    }

    void RenderManager::Render()
    {
        // Snapshot the post-update world into the frame UBO. Runs here
        // (rather than in StartFrame) so camera-follow components and any
        // other transform changes from appMain / sceneHierarchy update
        // are reflected. UBO upload is non-blocking; cost is microseconds.
        if (!fullscreenQuad)
        {
            cameraManager->Update();
        }
        UploadFrameUniforms();

        // BeginRenderPass and any third-party (ImGui) raw GL between frames
        // may have mutated state behind the Gfx backend's pipeline cache.
        // Drop the cache so the first bind of this frame always goes to
        // the driver.
        gfxApi->InvalidateCachedState();

        if (fullscreenQuad)
        {
            renderableFullscreenQuad->PreRender();
            renderableFullscreenQuad->Render(nullptr);
            gfxApi->AddGpuTimestamp("GpuAfterOpaque");
            gfxApi->AddGpuTimestamp("GpuAfterTransparent");
            gfxApi->EndRenderPass();
            gfxApi->AddGpuTimestamp("GpuAfterPostProcess");
            DrawPostProcessPass();
        }
        else
        {
            // Pass 1: Opaque objects (depth write ON, blend OFF).
            // Sorted by (pipeline, material) so same-pipeline draws are adjacent
            // and the backend's bind-skip cache can drop redundant BindPipeline calls.
            SortOpaqueRenderables();
            for (int i = 0; i < renderableCount; i++)
            {
                renderables[i]->PreRender();
                renderables[i]->Render(nullptr);
            }
            gfxApi->AddGpuTimestamp("GpuAfterOpaque");

            // Pass 2: Transparent objects (sorted back-to-front).
            // Each transparent renderable's pipeline bakes blend=on, depth-write=off.
            // State is self-restored by the next frame's BeginRenderPass
            // (depth mask) and EndRenderPass (blend, via the blit pipeline) —
            // RenderManager no longer manages transitional state.
            if (transparentRenderableCount > 0)
            {
                SortTransparentRenderables();

                for (int i = 0; i < transparentRenderableCount; i++)
                {
                    transparentRenderables[i]->PreRender();
                    transparentRenderables[i]->Render(nullptr);
                }
            }
            gfxApi->AddGpuTimestamp("GpuAfterTransparent");

            gfxApi->EndRenderPass();
            gfxApi->AddGpuTimestamp("GpuAfterPostProcess");
            DrawPostProcessPass();
        }
    }

    void RenderManager::GetWindowSize(int& width, int& height)
    {
        PlatformWindow::Get()->GetFramebufferSize(width, height);
    }

    void RenderManager::ToggleFullscreen()
    {
        PlatformWindow::Get()->ToggleFullscreen();
    }

    bool RenderManager::IsFullscreen() const
    {
        return PlatformWindow::Get()->IsFullscreen();
    }

    void RenderManager::SetAntialiasingEnabled(bool isEnabled)
    {
        int newSampleCount;
        if (isEnabled)
        {
            // Re-enabling without a fresh sample count uses the last non-1
            // value if we have one, else falls back to a sensible default.
            newSampleCount = (backbufferDescription.sampleCount > 1)
                ? backbufferDescription.sampleCount
                : 4;
        }
        else
        {
            newSampleCount = 1;
        }
        backbufferDescription.sampleCount = newSampleCount;
        isBackbufferReconfigurePending = true;
    }

    bool RenderManager::IsAntialiasingEnabled() const
    {
        return backbufferDescription.sampleCount > 1;
    }

    void RenderManager::SetMsaaSamples(int samples)
    {
        int clamped = samples > 1 ? samples : 1;
        backbufferDescription.sampleCount = clamped;
        isBackbufferReconfigurePending = true;
    }

    int RenderManager::GetMsaaSamples() const
    {
        return backbufferDescription.sampleCount;
    }

    void RenderManager::SortOpaqueRenderables()
    {
        std::sort(renderables, renderables + renderableCount,
            [](Renderable* a, Renderable* b)
            {
                uint32_t pa = a->GetPipelineHandle().id;
                uint32_t pb = b->GetPipelineHandle().id;
                bool result;
                if (pa != pb)
                {
                    result = (pa < pb);
                }
                else
                {
                    result = (a->GetMaterial() < b->GetMaterial());
                }
                return result;
            });
    }

    void RenderManager::SortTransparentRenderables()
    {
        Vector3 cameraPos = cameraManager->GetCameraPosition();

        std::sort(transparentRenderables, transparentRenderables + transparentRenderableCount,
            [&cameraPos](Renderable* a, Renderable* b)
            {
                Vector3 posA = a->GetOwner()->GetTransform().GetPosition();
                Vector3 posB = b->GetOwner()->GetTransform().GetPosition();

                Vector3 diffA = posA - cameraPos;
                Vector3 diffB = posB - cameraPos;

                float distA = diffA.x * diffA.x + diffA.y * diffA.y + diffA.z * diffA.z;
                float distB = diffB.x * diffB.x + diffB.y * diffB.y + diffB.z * diffB.z;

                return distA > distB;
            });
    }
}