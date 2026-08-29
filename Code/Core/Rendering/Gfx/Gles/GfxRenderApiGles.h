#ifndef GFXRENDERAPIGLES_H
#define GFXRENDERAPIGLES_H

#include "GfxRenderApi.h"
#include "GlScopeTimer.h"
#include <GLES3/gl31.h>
#include <vector>

namespace CC::Gfx
{
    // ========================
    // GfxRenderApiGles
    // ========================
    //
    // OpenGL ES 3.1 implementation of CC::Gfx::RenderApi for Android.
    //
    // Pool-backed handles + dirty-state cache mirror the desktop GL
    // backend (GfxRenderApiOpenGl). The two diverge on:
    //   - No MSAA scene framebuffer in v1; rendering targets the default
    //     framebuffer directly. EndRenderPass does no resolve/blit.
    //   - No debug-message callback (KHR_debug not gated; v2 work).
    //   - No GPU timestamp queries (EXT_disjoint_timer_query not gated).
    //   - No BC compressed-texture formats (mobile uses ASTC; BC entries
    //     in TextureFormat assert).
    //   - glDrawElementsBaseVertex requires GLES 3.2; engine v1 paths
    //     supply vertexOffset == 0, so we use glDrawElements + assert.
    //   - glClearDepthf in place of glClearDepth.
    //
    // Resource handle encoding: handle.id = poolIndex (id 0 reserved as
    // invalid via the IsValid sentinel in GfxHandles).

    class RenderApiGles : public RenderApi
    {
    public:
        RenderApiGles();
        virtual ~RenderApiGles();

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

    private:
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
        GLenum                currentIndexType = GL_UNSIGNED_INT;
        GfxCapabilities       capabilities;
        BackbufferDescription backbufferDescription;

        // Which target the active render pass is bound to. An invalid handle
        // means the pass targets the default framebuffer; a valid handle
        // indexes an offscreen entry in renderTargets.
        RenderTargetHandle    currentRenderTarget;
        bool                  currentPassIsOffscreen = false;

        // Push-constant UBO. Single GL_DYNAMIC_DRAW buffer reused every draw
        // via glBufferSubData; bound once at slot
        // OBJECT_UNIFORMS_BINDING_SLOT on first use.
        GLuint pushConstantsUbo = 0;

        bool renderPassActive = false;

        GlCommon::ScopeTimerState scopeTimer;

        void QueryCapabilities();
    };
}

#endif // GFXRENDERAPIGLES_H
