#ifndef GFXCAPABILITIES_H
#define GFXCAPABILITIES_H

namespace CC::Gfx
{
    // ========================
    // Backend capabilities
    // ========================
    //
    // Populated by the backend during RenderApi::Init. Queried via
    // RenderApi::GetCapabilities(). Application code branches on these
    // fields rather than on preprocessor defines so the same source
    // compiles and runs on every target.
    //
    // Feature gaps that differ meaningfully between targets:
    // - Compute shaders: unavailable on WebGL 2 and macOS GL.
    // - Storage buffers: unavailable on WebGL 2.
    // - Indirect draw: unavailable on WebGL 2.
    // - Geometry / tessellation: unavailable on WebGL 2 and GLES 3.1.
    // - BC texture compression: unavailable on mobile; ASTC instead.

    struct GfxCapabilities
    {
        // Features
        bool supportsComputeShaders    = false;
        bool supportsStorageBuffers    = false;
        bool supportsIndirectDraw      = false;
        bool supportsGeometryShader    = false;
        bool supportsTessellation      = false;
        bool supportsBcTextureFormats  = false;
        bool supportsAstcTextureFormats = false;
        bool supportsAnisotropicFiltering = false;
        // Half-float colour attachments. Core on desktop GL; on GLES 3.1 it
        // needs EXT_color_buffer_half_float, so an HDR scene target cannot be
        // assumed. Without it, tone mapping has no range to compress.
        bool supportsHalfFloatRenderTargets = false;
        bool supportsDebugMarkers      = false;
        bool supportsGpuTimestamps     = false;

        // Limits
        int maxTextureSize             = 0;
        int maxTextureArrayLayers      = 0;
        int maxVertexAttributes        = 0;
        int maxUniformBufferSizeBytes  = 0;
        int maxStorageBufferSizeBytes  = 0;
        int maxColorAttachments        = 0;
        int maxMsaaSamples             = 0;
        int maxComputeWorkgroupSizeX   = 0;
        int maxComputeWorkgroupSizeY   = 0;
        int maxComputeWorkgroupSizeZ   = 0;
        int maxComputeWorkgroupInvocations = 0;

        // Backend identity (for logs and dev UI, never for behavioural branching)
        const char* backendName        = "Unknown";
        const char* deviceName         = "Unknown";
        const char* driverVersion      = "Unknown";
    };
}

#endif // GFXCAPABILITIES_H
