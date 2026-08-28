#ifndef GFXENUMS_H
#define GFXENUMS_H

namespace CC::Gfx
{
    // ========================
    // Texture formats
    // ========================
    //
    // Engine-neutral names. Each backend translates to its native format
    // enum. Names follow Vulkan/DX/Metal convention: channels + component
    // bits + numeric type.

    enum class TextureFormat
    {
        Unknown,
        R8Unorm,
        Rg8Unorm,
        Rgba8Unorm,
        Rgba8Srgb,
        Rgb10A2Unorm,
        R16Float,
        Rg16Float,
        Rgba16Float,
        R32Float,
        Rgba32Float,
        Bc1,
        Bc3,
        Bc7,
        Astc4x4,
        Astc8x8,
        Depth24Stencil8,
        Depth32Float,
        Count
    };

    // ========================
    // Buffer usage and memory
    // ========================

    enum class BufferUsage
    {
        Vertex,
        Index,
        Uniform,
        Storage,
        Indirect,
        Count
    };

    enum class BufferMemory
    {
        GpuOnly,
        CpuToGpu,
        GpuToCpu,
        Count
    };

    // ========================
    // Vertex input
    // ========================

    enum class VertexAttribType
    {
        Float32,
        Float16,
        Uint8Norm,
        Uint8,
        Uint16,
        Uint32,
        Int32,
        Count
    };

    enum class PrimitiveTopology
    {
        Points,
        Lines,
        LineStrip,
        Triangles,
        TriangleStrip,
        Count
    };

    enum class IndexType
    {
        Uint16,
        Uint32,
        Count
    };

    // ========================
    // Pipeline state
    // ========================

    enum class CompareOp
    {
        Never,
        Less,
        Equal,
        LessEqual,
        Greater,
        NotEqual,
        GreaterEqual,
        Always,
        Count
    };

    enum class BlendFactor
    {
        Zero,
        One,
        SrcColor,
        OneMinusSrcColor,
        DstColor,
        OneMinusDstColor,
        SrcAlpha,
        OneMinusSrcAlpha,
        DstAlpha,
        OneMinusDstAlpha,
        Count
    };

    enum class BlendOp
    {
        Add,
        Subtract,
        ReverseSubtract,
        Min,
        Max,
        Count
    };

    enum class CullMode
    {
        None,
        Front,
        Back,
        Count
    };

    enum class FrontFace
    {
        CounterClockwise,
        Clockwise,
        Count
    };

    // ========================
    // Sampler
    // ========================

    enum class FilterMode
    {
        Nearest,
        Linear,
        Count
    };

    enum class MipmapMode
    {
        Nearest,
        Linear,
        None,
        Count
    };

    enum class AddressMode
    {
        Repeat,
        MirroredRepeat,
        ClampToEdge,
        ClampToBorder,
        Count
    };

    // ========================
    // Shader
    // ========================

    enum class ShaderStage
    {
        Vertex,
        Fragment,
        Compute,
        Count
    };

    // ========================
    // Image access (for compute image bindings)
    // ========================

    enum class ImageAccess
    {
        ReadOnly,
        WriteOnly,
        ReadWrite,
        Count
    };

    // ========================
    // Load / store ops for render targets
    // ========================

    enum class LoadOp
    {
        Load,
        Clear,
        DontCare,
        Count
    };

    enum class StoreOp
    {
        Store,
        DontCare,
        Count
    };
}

#endif // GFXENUMS_H
