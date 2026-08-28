#ifndef PIPELINECACHE_H
#define PIPELINECACHE_H

#include <unordered_map>
#include <cstdint>
#include "GfxHandles.h"
#include "GfxDescriptions.h"

namespace CC
{
    class Material;

    // Shares Gfx::PipelineHandle instances across Renderables that use the
    // same (material, vertex layout) pair. Owned by RenderManager. Destroys
    // every cached pipeline on shutdown.
    //
    // The cache key is a 64-bit mix of the Material pointer identity and a
    // hash of the VertexLayout bytes. Materials are managed by MaterialManager
    // so their pointer identity is stable for the lifetime of the cache.
    class PipelineCache
    {
    public:
        PipelineCache();
        ~PipelineCache();

        Gfx::PipelineHandle GetOrCreate(const Material* material, const Gfx::VertexLayout& layout);

        int GetCachedCount() const { return static_cast<int>(entries.size()); }

    private:
        static uint64_t HashKey(const Material* material, const Gfx::VertexLayout& layout);

        std::unordered_map<uint64_t, Gfx::PipelineHandle> entries;
    };
}

#endif // PIPELINECACHE_H
