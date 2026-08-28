#include "PipelineCache.h"
#include "Material.h"
#include "ShaderManager.h"
#include "GfxRenderApi.h"
#include "PrintManager.h"

namespace CC
{
    PipelineCache::PipelineCache()
    {
    }

    PipelineCache::~PipelineCache()
    {
        Gfx::RenderApi* gfxApi = Gfx::RenderApi::Get();
        for (auto& pair : entries)
        {
            if (pair.second.IsValid())
            {
                gfxApi->DestroyPipeline(pair.second);
            }
        }
        entries.clear();
    }

    uint64_t PipelineCache::HashKey(const Material* material, const Gfx::VertexLayout& layout)
    {
        // FNV-1a 64-bit over (material ptr, layout bytes). Stable across runs
        // within the same binary: pointer identity is derived from the process
        // allocator. The cache is process-local so that's sufficient.
        const uint64_t fnvOffsetBasis = 0xCBF29CE484222325ull;
        const uint64_t fnvPrime       = 0x100000001B3ull;

        uint64_t hash = fnvOffsetBasis;

        uintptr_t materialPtrValue = reinterpret_cast<uintptr_t>(material);
        const unsigned char* materialBytes = reinterpret_cast<const unsigned char*>(&materialPtrValue);
        for (size_t i = 0; i < sizeof(materialPtrValue); i++)
        {
            hash ^= static_cast<uint64_t>(materialBytes[i]);
            hash *= fnvPrime;
        }

        const unsigned char* layoutBytes = reinterpret_cast<const unsigned char*>(&layout);
        for (size_t i = 0; i < sizeof(Gfx::VertexLayout); i++)
        {
            hash ^= static_cast<uint64_t>(layoutBytes[i]);
            hash *= fnvPrime;
        }

        return hash;
    }

    Gfx::PipelineHandle PipelineCache::GetOrCreate(const Material* material, const Gfx::VertexLayout& layout)
    {
        uint64_t key = HashKey(material, layout);
        auto it = entries.find(key);

        Gfx::PipelineHandle result;
        if (it != entries.end())
        {
            result = it->second;
        }
        else
        {
            Gfx::ShaderHandle shader = ShaderManager::Get()->GetShaderHandle(material->GetShaderName());
            const bool isTransparent = material->IsTransparent();

            Gfx::PipelineDescription desc;
            desc.shader                         = shader;
            desc.vertexLayout                   = layout;
            desc.topology                       = Gfx::PrimitiveTopology::Triangles;
            // Legacy parity: GL_CULL_FACE was never enabled; keep culling off.
            desc.rasterizer.cullMode            = Gfx::CullMode::None;
            desc.rasterizer.frontFace           = Gfx::FrontFace::CounterClockwise;
            desc.depthStencil.depthTestEnabled  = material->GetDepthTestEnabled();
            desc.depthStencil.depthWriteEnabled = !isTransparent && material->GetDepthTestEnabled();
            desc.depthStencil.depthCompare      = Gfx::CompareOp::Less;
            desc.blend.enabled                  = isTransparent;
            desc.blend.srcColorFactor           = Gfx::BlendFactor::SrcAlpha;
            desc.blend.dstColorFactor           = Gfx::BlendFactor::OneMinusSrcAlpha;
            desc.blend.colorOp                  = Gfx::BlendOp::Add;
            desc.blend.srcAlphaFactor           = Gfx::BlendFactor::One;
            desc.blend.dstAlphaFactor           = Gfx::BlendFactor::OneMinusSrcAlpha;
            desc.blend.alphaOp                  = Gfx::BlendOp::Add;

            result = Gfx::RenderApi::Get()->CreatePipeline(desc);
            entries.emplace(key, result);
        }
        return result;
    }
}
