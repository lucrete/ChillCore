#ifndef GFXHANDLES_H
#define GFXHANDLES_H

#include <cstdint>

namespace CC::Gfx
{
    // ========================
    // Opaque resource handles
    // ========================
    //
    // A handle is a small value type that identifies a GPU resource owned by
    // the active RenderApi backend. Frontend and application code never see
    // the native API type (GLuint, VkBuffer, ID3D12Resource*); the backend
    // maps handle ids to its own pool entries.
    //
    // Default-constructed handle holds UINT32_MAX, which IsValid() rejects.
    // Valid handles are direct indices into the backend pool (no off-by-one).
    // Destroyed handles may be recycled by the backend pool.

    struct BufferHandle
    {
        uint32_t id = UINT32_MAX;
        bool IsValid() const
        {
            return id != UINT32_MAX;
        }
    };

    struct TextureHandle
    {
        uint32_t id = UINT32_MAX;
        bool IsValid() const
        {
            return id != UINT32_MAX;
        }
    };

    struct SamplerHandle
    {
        uint32_t id = UINT32_MAX;
        bool IsValid() const
        {
            return id != UINT32_MAX;
        }
    };

    struct ShaderHandle
    {
        uint32_t id = UINT32_MAX;
        bool IsValid() const
        {
            return id != UINT32_MAX;
        }
    };

    struct PipelineHandle
    {
        uint32_t id = UINT32_MAX;
        bool IsValid() const
        {
            return id != UINT32_MAX;
        }
    };

    struct RenderTargetHandle
    {
        uint32_t id = UINT32_MAX;
        bool IsValid() const
        {
            return id != UINT32_MAX;
        }
    };

    // ========================
    // Handle equality
    // ========================

    inline bool operator==(BufferHandle a, BufferHandle b)       { return a.id == b.id; }
    inline bool operator!=(BufferHandle a, BufferHandle b)       { return a.id != b.id; }
    inline bool operator==(TextureHandle a, TextureHandle b)     { return a.id == b.id; }
    inline bool operator!=(TextureHandle a, TextureHandle b)     { return a.id != b.id; }
    inline bool operator==(SamplerHandle a, SamplerHandle b)     { return a.id == b.id; }
    inline bool operator!=(SamplerHandle a, SamplerHandle b)     { return a.id != b.id; }
    inline bool operator==(ShaderHandle a, ShaderHandle b)       { return a.id == b.id; }
    inline bool operator!=(ShaderHandle a, ShaderHandle b)       { return a.id != b.id; }
    inline bool operator==(PipelineHandle a, PipelineHandle b)   { return a.id == b.id; }
    inline bool operator!=(PipelineHandle a, PipelineHandle b)   { return a.id != b.id; }
    inline bool operator==(RenderTargetHandle a, RenderTargetHandle b) { return a.id == b.id; }
    inline bool operator!=(RenderTargetHandle a, RenderTargetHandle b) { return a.id != b.id; }
}

#endif // GFXHANDLES_H
