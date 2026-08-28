#ifndef GFXDESCRIPTIONS_H
#define GFXDESCRIPTIONS_H

#include "GfxHandles.h"
#include "GfxEnums.h"

namespace CC::Gfx
{
    // ========================
    // Limits
    // ========================

    static constexpr int MAX_VERTEX_ATTRIBUTES = 8;
    static constexpr int MAX_COLOR_ATTACHMENTS = 4;
    static constexpr int MAX_BOUND_TEXTURES = 16;
    static constexpr int MAX_BOUND_UNIFORM_BUFFERS = 8;
    static constexpr int MAX_BOUND_STORAGE_BUFFERS = 8;

    // ========================
    // Buffer description
    // ========================

    struct BufferDescription
    {
        int          sizeBytes    = 0;
        BufferUsage  usage        = BufferUsage::Vertex;
        BufferMemory memory       = BufferMemory::GpuOnly;
        const void*  initialData  = nullptr;
        const char*  debugName    = nullptr;
    };

    // ========================
    // Texture description
    // ========================

    struct TextureDescription
    {
        int           width          = 0;
        int           height         = 0;
        int           depth          = 1;
        int           mipLevels      = 1;
        int           arrayLayers    = 1;
        TextureFormat format         = TextureFormat::Rgba8Unorm;
        bool          isRenderTarget = false;
        bool          isStorage      = false;
        bool          isCubemap      = false;
        const void*   initialData    = nullptr;
        const char*   debugName      = nullptr;
    };

    // ========================
    // Sampler description
    // ========================

    struct SamplerDescription
    {
        FilterMode  minFilter    = FilterMode::Linear;
        FilterMode  magFilter    = FilterMode::Linear;
        MipmapMode  mipmapMode   = MipmapMode::Linear;
        AddressMode addressU     = AddressMode::Repeat;
        AddressMode addressV     = AddressMode::Repeat;
        AddressMode addressW     = AddressMode::Repeat;
        float       maxAnisotropy = 1.0f;
        const char* debugName    = nullptr;
    };

    // ========================
    // Vertex input layout
    // ========================

    struct VertexAttribute
    {
        int              location   = 0;
        int              offsetBytes = 0;
        VertexAttribType type       = VertexAttribType::Float32;
        int              components = 3;
    };

    struct VertexLayout
    {
        VertexAttribute attributes[MAX_VERTEX_ATTRIBUTES];
        int             attributeCount = 0;
        int             strideBytes    = 0;
    };

    // ========================
    // Shader description
    // ========================
    //
    // Source is provided per-stage. Backends that need cross-compilation
    // (SPIR-V, HLSL, MSL) translate from the supplied GLSL at creation time.
    // Pure-compute shaders leave vertex and fragment sources empty.

    struct ShaderDescription
    {
        const char* vertexSource   = nullptr;
        const char* fragmentSource = nullptr;
        const char* computeSource  = nullptr;
        const char* debugName      = nullptr;
    };

    // ========================
    // Blend state
    // ========================

    struct BlendState
    {
        bool        enabled        = false;
        BlendFactor srcColorFactor = BlendFactor::SrcAlpha;
        BlendFactor dstColorFactor = BlendFactor::OneMinusSrcAlpha;
        BlendOp     colorOp        = BlendOp::Add;
        BlendFactor srcAlphaFactor = BlendFactor::One;
        BlendFactor dstAlphaFactor = BlendFactor::OneMinusSrcAlpha;
        BlendOp     alphaOp        = BlendOp::Add;
    };

    // ========================
    // Depth / stencil state
    // ========================

    struct DepthStencilState
    {
        bool      depthTestEnabled  = true;
        bool      depthWriteEnabled = true;
        CompareOp depthCompare      = CompareOp::Less;
    };

    // ========================
    // Rasterizer state
    // ========================

    struct RasterizerState
    {
        CullMode  cullMode  = CullMode::Back;
        FrontFace frontFace = FrontFace::CounterClockwise;
    };

    // ========================
    // Pipeline description
    // ========================
    //
    // Bundles all state a draw call needs. Created once, bound as a unit.
    // Backends that have native pipeline objects (Vulkan, D3D12, Metal)
    // build them here. The GL backend stores the bundle and replays the
    // underlying state calls on BindPipeline.

    struct PipelineDescription
    {
        ShaderHandle      shader;
        VertexLayout      vertexLayout;
        PrimitiveTopology topology          = PrimitiveTopology::Triangles;
        RasterizerState   rasterizer;
        DepthStencilState depthStencil;
        BlendState        blend;
        TextureFormat     colorFormat       = TextureFormat::Rgba8Unorm;
        TextureFormat     depthFormat       = TextureFormat::Depth24Stencil8;
        const char*       debugName         = nullptr;
    };

    // ========================
    // Render target description
    // ========================

    struct RenderTargetAttachment
    {
        TextureHandle texture;
        int           mipLevel    = 0;
        int           arrayLayer  = 0;
        LoadOp        loadOp      = LoadOp::Clear;
        StoreOp       storeOp     = StoreOp::Store;
        float         clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    };

    struct RenderTargetDescription
    {
        RenderTargetAttachment colorAttachments[MAX_COLOR_ATTACHMENTS];
        int                    colorAttachmentCount = 0;
        RenderTargetAttachment depthStencilAttachment;
        bool                   hasDepthStencil      = false;
        int                    width                = 0;
        int                    height               = 0;
        const char*            debugName            = nullptr;
    };

    // ========================
    // Backbuffer description
    // ========================
    //
    // Settings the backend applies to the primary swapchain / offscreen
    // backbuffer. Backends recreate the underlying resources (MSAA
    // framebuffer, swap images) when ConfigureBackbuffer is called with a
    // description that differs from the current one.
    //
    // width / height of 0 means "match the window" — the backend queries
    // the platform window. sampleCount of 1 disables MSAA; any value > 1
    // must not exceed GfxCapabilities::maxMsaaSamples.

    struct BackbufferDescription
    {
        int           width        = 0;
        int           height       = 0;
        int           sampleCount  = 1;
        TextureFormat colorFormat  = TextureFormat::Rgba8Unorm;
        TextureFormat depthFormat  = TextureFormat::Depth24Stencil8;
        bool          vsyncEnabled = true;
    };
}

#endif // GFXDESCRIPTIONS_H
