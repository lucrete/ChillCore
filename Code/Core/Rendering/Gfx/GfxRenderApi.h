#ifndef GFXRENDERAPI_H
#define GFXRENDERAPI_H

#include "GfxHandles.h"
#include "GfxEnums.h"
#include "GfxDescriptions.h"
#include "GfxCapabilities.h"

namespace CC::Gfx
{
    // ========================
    // RenderApi - abstract graphics backend interface
    // ========================
    //
    // Engine-neutral graphics API. Every operation speaks CC::Gfx handles
    // and descriptions. Backends translate to OpenGL / Vulkan / D3D12 / Metal
    // at the edge. No native graphics API types appear in this header.
    //
    // Lifecycle:
    //   Init   - populate capabilities, create internal pools. Assumes the
    //            platform layer has already established a current graphics
    //            context or device.
    //   BeginFrame / EndFrame - frame brackets.
    //   BeginRenderPass / EndRenderPass - render pass brackets. Nested
    //            passes are not supported.
    //
    // Threading: not thread-safe. All calls from the render thread.

    class RenderApi
    {
    public:
        RenderApi();
        virtual ~RenderApi();
        static RenderApi* Get();

        // ========================
        // Lifecycle
        // ========================

        virtual void Init()     = 0;
        virtual void Shutdown() = 0;

        virtual const GfxCapabilities& GetCapabilities() const = 0;

        // ========================
        // Frame
        // ========================

        virtual void BeginFrame() = 0;
        virtual void EndFrame()   = 0;

        // ========================
        // Render pass
        // ========================

        // scopeName labels the pass on the GPU timeline: the pass opens a
        // timing scope under that name, so a frame with several passes reads
        // as several named phases rather than one repeated label.
        //
        // A name of the form "Group/Detail" groups the pass with its
        // neighbours: the profiler folds contiguous passes sharing a group
        // into one phase, while capture tools keep the full name. That is
        // what lets a phase stay a fixed part of the frame while the passes
        // inside it come and go. See the GPU timing section below.
        virtual void BeginRenderPass(RenderTargetHandle target, const char* scopeName)   = 0;
        virtual void EndRenderPass()                                                     = 0;

        // ========================
        // Resource creation / destruction
        // ========================

        [[nodiscard]] virtual BufferHandle       CreateBuffer(const BufferDescription& description)       = 0;
        virtual void                             DestroyBuffer(BufferHandle handle)                        = 0;
        virtual void                             UpdateBuffer(BufferHandle handle,
                                                              int offsetBytes, int sizeBytes,
                                                              const void* data)                            = 0;
        virtual void                             ReadBuffer(BufferHandle handle,
                                                            int offsetBytes, int sizeBytes,
                                                            void* destination)                             = 0;

        [[nodiscard]] virtual TextureHandle      CreateTexture(const TextureDescription& description)      = 0;
        virtual void                             DestroyTexture(TextureHandle handle)                       = 0;
        virtual void                             UpdateTexture(TextureHandle handle,
                                                               int mipLevel, int x, int y,
                                                               int width, int height,
                                                               const void* data)                            = 0;

        [[nodiscard]] virtual SamplerHandle      CreateSampler(const SamplerDescription& description)       = 0;
        virtual void                             DestroySampler(SamplerHandle handle)                        = 0;

        [[nodiscard]] virtual ShaderHandle       CreateShader(const ShaderDescription& description)          = 0;
        virtual void                             DestroyShader(ShaderHandle handle)                           = 0;

        [[nodiscard]] virtual PipelineHandle     CreatePipeline(const PipelineDescription& description)       = 0;
        virtual void                             DestroyPipeline(PipelineHandle handle)                        = 0;

        [[nodiscard]] virtual RenderTargetHandle CreateRenderTarget(const RenderTargetDescription& description) = 0;
        virtual void                             DestroyRenderTarget(RenderTargetHandle handle)                  = 0;

        // ========================
        // Command recording
        // ========================

        virtual void BindPipeline(PipelineHandle pipeline)                           = 0;
        virtual void BindVertexBuffer(int slot, BufferHandle buffer,
                                      int offsetBytes, int strideBytes)              = 0;
        virtual void BindIndexBuffer(BufferHandle buffer, IndexType type)            = 0;
        virtual void BindTexture(int slot, TextureHandle texture,
                                 SamplerHandle sampler)                              = 0;
        virtual void BindUniformBuffer(int slot, BufferHandle buffer,
                                       int offsetBytes, int sizeBytes)               = 0;
        virtual void BindStorageBuffer(int slot, BufferHandle buffer)                = 0;
        virtual void BindImage(int slot, TextureHandle texture,
                               int mipLevel, ImageAccess access,
                               TextureFormat format)                                 = 0;

        virtual void SetPushConstants(const void* data, int sizeBytes)               = 0;

        virtual void Draw(int vertexCount, int instanceCount, int firstVertex)       = 0;
        virtual void DrawIndexed(int indexCount, int instanceCount,
                                 int firstIndex, int vertexOffset)                   = 0;
        virtual void DrawIndirect(BufferHandle argsBuffer, int offsetBytes)          = 0;

        // ========================
        // Compute
        // ========================

        virtual void DispatchCompute(int groupsX, int groupsY, int groupsZ)          = 0;
        virtual void DispatchComputeIndirect(BufferHandle argsBuffer, int offsetBytes) = 0;

        // ========================
        // Synchronisation
        // ========================
        //
        // barrierBits is a bitmask of backend-neutral barrier kinds.
        // See BarrierBit below.

        virtual void MemoryBarrier(unsigned int barrierBits)                         = 0;

        // Forget any cached bind state. Call after any raw-backend code
        // path mutates state behind the backend's back (BeginRenderPass's
        // raw glClear / glDepthMask, ImGui's GL state churn, etc.) and at
        // frame boundaries. Default no-op for backends that don't cache.
        virtual void InvalidateCachedState() {}

        // Surface lifecycle hooks. Default no-ops; backends that need
        // to drop cached bind state when the on-screen surface goes /
        // comes back override. Distinct from a context-loss path —
        // the underlying graphics context (and every GL handle) can
        // survive the surface cycle when the platform layer preserves
        // it.
        virtual void OnSurfaceLost()      {}
        virtual void OnSurfaceRestored()  {}

        // Context lifecycle hooks. Fired when the underlying graphics
        // context is destroyed and recreated, which invalidates every
        // handle the backend has issued. Backends and engine
        // subsystems rebuild their GPU resources from CPU-authoritative
        // sources between these two calls. Default no-ops for backends
        // running on a platform that cannot lose its context.
        virtual void OnContextLost()      {}
        virtual void OnContextRestored()  {}

        // ========================
        // Backbuffer / swapchain
        // ========================
        //
        // ConfigureBackbuffer sets the backbuffer's size, MSAA sample count,
        // formats, and vsync policy atomically. Backends that need to
        // recreate the swapchain or MSAA framebuffer (Vulkan, DX12, GL MSAA)
        // do so here. Safe to call at any frame boundary.
        //
        // GetBackbuffer returns the RenderTargetHandle that
        // BeginRenderPass targets when the caller wants to draw to the
        // primary backbuffer. GetBackbufferSize returns the current
        // dimensions — these may differ from the window size on high-DPI
        // displays or when width/height in the descriptor were non-zero.

        virtual void                       ConfigureBackbuffer(const BackbufferDescription& description) {}
        virtual RenderTargetHandle         GetBackbuffer() const { return RenderTargetHandle(); }
        virtual void                       GetBackbufferSize(int& width, int& height) const { width = 0; height = 0; }

        // ========================
        // GPU timing and debug markers
        // ========================
        //
        // AddGpuTimestamp marks a point on the GPU command stream with a
        // name. Each call implicitly closes the previous named scope and
        // opens a new one. The duration recorded for `name` is the
        // GPU-side interval from this call to the NEXT AddGpuTimestamp call
        // (or the next BeginFrame, whichever comes first). Results are
        // read back a few frames later.
        //
        // The scope name is authoritative only at the AddGpuTimestamp call
        // site. Callers that display or record scope data iterate the
        // resolved frame by index — the backend owns the name→duration
        // mapping — so the same string is never written down in two places.
        //
        // When the backend supports debug markers, AddGpuTimestamp also
        // emits a label that RenderDoc / Nsight captures surface as a
        // named group.
        //
        // BeginFrame (declared above) is when the backend advances the
        // query ring and reads results from N frames ago. It also closes
        // any scope still open on the outgoing frame's slot.

        virtual void        AddGpuTimestamp(const char* name) {}
        virtual int         GetGpuScopeCount() const { return 0; }
        virtual const char* GetGpuScopeNameAt(int index) const { return ""; }
        virtual float       GetGpuScopeDurationMsAt(int index) const { return 0.0f; }
        virtual float       GetGpuFrameDurationMs() const { return 0.0f; }
        virtual bool        IsGpuTimerSupported() const { return false; }

    protected:
        static RenderApi* instance;
    };

    // ========================
    // Barrier bit flags
    // ========================
    //
    // Backend-neutral barrier kinds. Combine with bitwise OR. Backends
    // translate to glMemoryBarrier bits / VkPipelineBarrier / etc.

    namespace BarrierBit
    {
        static constexpr unsigned int VertexAttribRead  = 1u << 0;
        static constexpr unsigned int IndexRead         = 1u << 1;
        static constexpr unsigned int UniformRead       = 1u << 2;
        static constexpr unsigned int TextureFetch      = 1u << 3;
        static constexpr unsigned int ImageAccess       = 1u << 4;
        static constexpr unsigned int StorageBuffer     = 1u << 5;
        static constexpr unsigned int IndirectCommand   = 1u << 6;
        static constexpr unsigned int BufferUpdate      = 1u << 7;
        static constexpr unsigned int Framebuffer       = 1u << 8;
        static constexpr unsigned int All               = 0xFFFFFFFFu;
    }
}

#endif // GFXRENDERAPI_H
