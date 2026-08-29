#ifndef GFXRENDERAPIOPENGL_H
#define GFXRENDERAPIOPENGL_H

#include "GfxRenderApi.h"
#include "GlScopeTimer.h"
#include <glad/gl.h>
#include <vector>

namespace CC
{
    class FrameBufferOpenGl;
}

namespace CC::Gfx
{
    // ========================
    // GfxRenderApiOpenGl
    // ========================
    //
    // OpenGL 4.3 core implementation of CC::Gfx::RenderApi.
    //
    // Does not own the GLFW window or GL context. The legacy RenderApi
    // (global namespace) currently owns window lifecycle during the
    // Phase 1 migration. This class attaches to whatever context is
    // current when Init is called.
    //
    // Resource handle encoding: handle.id = poolIndex + 1. Id 0 is invalid.

    class RenderApiOpenGl : public RenderApi
    {
    public:
        RenderApiOpenGl();
        virtual ~RenderApiOpenGl();

        // Lifecycle
        virtual void Init() override;
        virtual void Shutdown() override;
        virtual const GfxCapabilities& GetCapabilities() const override;

        // Frame
        virtual void BeginFrame() override;
        virtual void EndFrame() override;

        // Render pass
        virtual void BeginDefaultRenderPass(const float clearColor[4], float clearDepth) override;
        virtual void BeginRenderPass(RenderTargetHandle target) override;
        virtual void EndRenderPass() override;

        // Buffers
        virtual BufferHandle CreateBuffer(const BufferDescription& description) override;
        virtual void         DestroyBuffer(BufferHandle handle) override;
        virtual void         UpdateBuffer(BufferHandle handle, int offsetBytes, int sizeBytes, const void* data) override;
        virtual void         ReadBuffer(BufferHandle handle, int offsetBytes, int sizeBytes, void* destination) override;

        // Textures
        virtual TextureHandle CreateTexture(const TextureDescription& description) override;
        virtual void          DestroyTexture(TextureHandle handle) override;
        virtual void          UpdateTexture(TextureHandle handle, int mipLevel, int x, int y, int width, int height, const void* data) override;

        // Samplers
        virtual SamplerHandle CreateSampler(const SamplerDescription& description) override;
        virtual void          DestroySampler(SamplerHandle handle) override;

        // Shaders
        virtual ShaderHandle CreateShader(const ShaderDescription& description) override;
        virtual void         DestroyShader(ShaderHandle handle) override;

        // Pipelines
        virtual PipelineHandle CreatePipeline(const PipelineDescription& description) override;
        virtual void           DestroyPipeline(PipelineHandle handle) override;

        // Render targets
        virtual RenderTargetHandle CreateRenderTarget(const RenderTargetDescription& description) override;
        virtual void               DestroyRenderTarget(RenderTargetHandle handle) override;

        // Command recording
        virtual void BindPipeline(PipelineHandle pipeline) override;
        virtual void BindVertexBuffer(int slot, BufferHandle buffer, int offsetBytes, int strideBytes) override;
        virtual void BindIndexBuffer(BufferHandle buffer, IndexType type) override;
        virtual void BindTexture(int slot, TextureHandle texture, SamplerHandle sampler) override;
        virtual void BindUniformBuffer(int slot, BufferHandle buffer, int offsetBytes, int sizeBytes) override;
        virtual void BindStorageBuffer(int slot, BufferHandle buffer) override;
        virtual void BindImage(int slot, TextureHandle texture, int mipLevel, ImageAccess access, TextureFormat format) override;

        virtual void SetPushConstants(const void* data, int sizeBytes) override;

        virtual void Draw(int vertexCount, int instanceCount, int firstVertex) override;
        virtual void DrawIndexed(int indexCount, int instanceCount, int firstIndex, int vertexOffset) override;
        virtual void DrawIndirect(BufferHandle argsBuffer, int offsetBytes) override;

        // Compute
        virtual void DispatchCompute(int groupsX, int groupsY, int groupsZ) override;
        virtual void DispatchComputeIndirect(BufferHandle argsBuffer, int offsetBytes) override;

        // Synchronisation
        virtual void MemoryBarrier(unsigned int barrierBits) override;

        virtual void InvalidateCachedState() override;

        virtual void               ConfigureBackbuffer(const BackbufferDescription& description) override;
        virtual RenderTargetHandle GetBackbuffer() const override;
        virtual void               GetBackbufferSize(int& width, int& height) const override;

        virtual void        AddGpuTimestamp(const char* name) override;
        virtual int         GetGpuScopeCount() const override;
        virtual const char* GetGpuScopeNameAt(int index) const override;
        virtual float       GetGpuScopeDurationMsAt(int index) const override;
        virtual float       GetGpuFrameDurationMs() const override;
        virtual bool        IsGpuTimerSupported() const override;

    private:
        // Lazy-build the FullScreenBlit pipeline + vertex buffer on first
        // EndRenderPass — called only after ShaderManager has compiled
        // the FullScreenBlit shader.
        void EnsureBlitPipeline();

        // Wrap up GPU scope tracking for the just-ended frame and open the
        // first scope of the next frame's slot. Called from EndFrame after
        // SwapBuffers so any GL back-pressure (query resolve stalls,
        // driver-side queue waits) is charged to the SwapBuffers CPU phase.
        void AdvanceScopeRing();

        // ========================
        // Internal pool entries
        // ========================

        struct GlBuffer
        {
            GLuint       glHandle      = 0;
            GLenum       target        = 0;
            GLenum       usageHint     = 0;
            int          sizeBytes     = 0;
            BufferUsage  usage         = BufferUsage::Vertex;
            BufferMemory memory        = BufferMemory::GpuOnly;
            bool         isAlive       = false;
        };

        struct GlTexture
        {
            GLuint        glHandle     = 0;
            GLenum        target       = 0;
            TextureFormat format       = TextureFormat::Unknown;
            int           width        = 0;
            int           height       = 0;
            bool          isAlive      = false;
        };

        struct GlSampler
        {
            GLuint glHandle = 0;
            bool   isAlive  = false;
        };

        struct GlShader
        {
            GLuint program  = 0;
            bool   isCompute = false;
            bool   isAlive  = false;
        };

        struct GlPipeline
        {
            PipelineDescription description;
            GLuint              vao          = 0;
            bool                isAlive      = false;
        };

        struct GlRenderTarget
        {
            GLuint                  fbo                  = 0;
            int                     width                = 0;
            int                     height               = 0;
            RenderTargetDescription description;
            bool                    isAlive              = false;
        };

        // ========================
        // Pools
        // ========================
        //
        // Handle id = index + 1. Free list stores indices awaiting reuse.

        std::vector<GlBuffer>       buffers;
        std::vector<GlTexture>      textures;
        std::vector<GlSampler>      samplers;
        std::vector<GlShader>       shaders;
        std::vector<GlPipeline>     pipelines;
        std::vector<GlRenderTarget> renderTargets;

        std::vector<uint32_t>       freeBufferSlots;
        std::vector<uint32_t>       freeTextureSlots;
        std::vector<uint32_t>       freeSamplerSlots;
        std::vector<uint32_t>       freeShaderSlots;
        std::vector<uint32_t>       freePipelineSlots;
        std::vector<uint32_t>       freeRenderTargetSlots;

        // ========================
        // State tracking
        // ========================

        PipelineHandle        currentPipeline;
        GLenum                currentIndexType = 0x1405;  // GL_UNSIGNED_INT default
        GfxCapabilities       capabilities;
        BackbufferDescription backbufferDescription;

        // Push-constant UBO. Single GL_DYNAMIC_DRAW buffer reused every draw
        // via glBufferSubData; bound once at slot
        // OBJECT_UNIFORMS_BINDING_SLOT on first use.
        GLuint pushConstantsUbo = 0;

        // Offscreen scene framebuffer (with optional MSAA). Created in
        // ConfigureBackbuffer; resolved + blitted to the default framebuffer
        // by EndRenderPass. Engine-side scene rendering targets this FB.
        CC::FrameBufferOpenGl* sceneFrameBuffer = nullptr;

        // Lazy-built blit pipeline. Created on first EndRenderPass once
        // ShaderManager has compiled the FullScreenBlit shader.
        BufferHandle    blitVertexBuffer;
        PipelineHandle  blitPipeline;
        ShaderHandle    blitShader;
        bool            renderPassActive = false;

        // ========================
        // GPU scope timing
        // ========================
        //
        // Ring-buffer state lives in GlCommon::ScopeTimerState (shared
        // with the GLES backend). The GL calls that consume / produce
        // it (AddGpuTimestamp, AdvanceScopeRing, ResolveFrameScopes)
        // stay here because their entry points are GL-version-specific.

        GlCommon::ScopeTimerState scopeTimer;

        void ResolveFrameScopes(GlCommon::GpuScopeFrame& frame);

        // ========================
        // Helpers
        // ========================

        void QueryCapabilities();
    };
}

#endif // GFXRENDERAPIOPENGL_H
