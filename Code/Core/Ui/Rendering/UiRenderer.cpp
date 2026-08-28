#include "UiRenderer.h"

#include "ShaderManager.h"
#include "CCAssert.h"
#include "GfxRenderApi.h"
#include "GfxDescriptions.h"
#include "UiUniforms.h"

namespace CC
{
    UiRenderer* UiRenderer::instance = nullptr;

    static Gfx::VertexLayout MakeUiVertexLayout()
    {
        Gfx::VertexLayout layout;
        layout.strideBytes    = 8 * sizeof(float); // pos2 + uv2 + rgba
        layout.attributeCount = 3;
        layout.attributes[0]  = { 0, 0                 , Gfx::VertexAttribType::Float32, 2 };
        layout.attributes[1]  = { 1, 2 * sizeof(float) , Gfx::VertexAttribType::Float32, 2 };
        layout.attributes[2]  = { 2, 4 * sizeof(float) , Gfx::VertexAttribType::Float32, 4 };
        return layout;
    }

    UiRenderer::UiRenderer()
    {
        CC_ASSERT(instance == nullptr, "UiRenderer already created");
        instance = this;

        Gfx::RenderApi* gfxApi = Gfx::RenderApi::Get();

        // Pre-allocated dynamic vertex and index buffers. Each FlushBatch
        // uploads the current batch's contents via UpdateBuffer (subData)
        // within these fixed-size allocations.
        Gfx::BufferDescription vbDesc;
        vbDesc.sizeBytes = MAX_VERTEX_BUFFER_BYTES;
        vbDesc.usage     = Gfx::BufferUsage::Vertex;
        vbDesc.memory    = Gfx::BufferMemory::CpuToGpu;
        vbDesc.debugName = "UiRenderer::Vertices";
        vertexBuffer = gfxApi->CreateBuffer(vbDesc);

        Gfx::BufferDescription ibDesc;
        ibDesc.sizeBytes = MAX_INDEX_BUFFER_BYTES;
        ibDesc.usage     = Gfx::BufferUsage::Index;
        ibDesc.memory    = Gfx::BufferMemory::CpuToGpu;
        ibDesc.debugName = "UiRenderer::Indices";
        indexBuffer = gfxApi->CreateBuffer(ibDesc);

        Gfx::BufferDescription uboDesc;
        uboDesc.sizeBytes = UI_UNIFORMS_SIZE_BYTES;
        uboDesc.usage     = Gfx::BufferUsage::Uniform;
        uboDesc.memory    = Gfx::BufferMemory::CpuToGpu;
        uboDesc.debugName = "UiRenderer::Uniforms";
        uniformBuffer = gfxApi->CreateBuffer(uboDesc);

        // Pipeline is built lazily in EndFrame so it can rebuild on shader
        // hot-reload of the UiQuad shader.

        // 1x1 white pixel texture for solid-color quads
        const unsigned char whitePixel[4] = { 255, 255, 255, 255 };
        Gfx::TextureDescription whiteDesc;
        whiteDesc.width       = 1;
        whiteDesc.height      = 1;
        whiteDesc.mipLevels   = 1;
        whiteDesc.format      = Gfx::TextureFormat::Rgba8Unorm;
        whiteDesc.initialData = whitePixel;
        whiteDesc.debugName   = "UiRendererWhitePixel";
        whiteTexture = gfxApi->CreateTexture(whiteDesc);
    }

    UiRenderer::~UiRenderer()
    {
        Gfx::RenderApi* gfxApi = Gfx::RenderApi::Get();
        if (whiteTexture.IsValid())   { gfxApi->DestroyTexture(whiteTexture); }
        if (uniformBuffer.IsValid())  { gfxApi->DestroyBuffer(uniformBuffer); }
        if (indexBuffer.IsValid())    { gfxApi->DestroyBuffer(indexBuffer); }
        if (vertexBuffer.IsValid())   { gfxApi->DestroyBuffer(vertexBuffer); }
        if (pipelineHandle.IsValid()) { gfxApi->DestroyPipeline(pipelineHandle); }
        instance = nullptr;
    }

    UiRenderer* UiRenderer::Get()
    {
        CC_ASSERT(instance != nullptr, "UiRenderer not created yet");
        return instance;
    }

    void UiRenderer::BeginFrame(int screenWidth, int screenHeight)
    {
        currentScreenWidth = screenWidth;
        currentScreenHeight = screenHeight;

        // Y=0 at top, Y increases downward
        projection.Orthographic(0.0f, (float)screenWidth, (float)screenHeight, 0.0f, -1.0f, 1.0f);

        UiUniforms uiUniforms;
        const float* projectionData = projection;
        for (int i = 0; i < 16; i++)
        {
            uiUniforms.projection[i] = projectionData[i];
        }
        Gfx::RenderApi::Get()->UpdateBuffer(uniformBuffer, 0, UI_UNIFORMS_SIZE_BYTES, &uiUniforms);

        // Clear batch contents but preserve allocated capacity
        for (int i = 0; i < activeBatchCount; i++)
        {
            batches[i].vertices.clear();
            batches[i].indices.clear();
        }
        activeBatchCount = 0;
    }

    void UiRenderer::SetGlobalAlpha(float alpha)
    {
        globalAlpha = alpha;
    }

    void UiRenderer::DrawQuad(float x, float y, float width, float height, const Colour& colour)
    {
        AddQuadToBatch(whiteTexture, x, y, width, height, 0.0f, 0.0f, 1.0f, 1.0f, colour);
    }

    void UiRenderer::DrawTexturedQuad(float x, float y, float width, float height,
        float uvLeft, float uvTop, float uvRight, float uvBottom,
        const Colour& tint, Gfx::TextureHandle texture)
    {
        AddQuadToBatch(texture, x, y, width, height, uvLeft, uvTop, uvRight, uvBottom, tint);
    }

    void UiRenderer::DrawSlice9(float x, float y, float width, float height,
        Gfx::TextureHandle texture, float sliceSize, int textureWidth, int textureHeight, const Colour& tint)
    {
        float uSlice = sliceSize / (float)textureWidth;
        float vSlice = sliceSize / (float)textureHeight;
        float innerWidth = width - sliceSize * 2.0f;
        float innerHeight = height - sliceSize * 2.0f;

        if (innerWidth < 0.0f) innerWidth = 0.0f;
        if (innerHeight < 0.0f) innerHeight = 0.0f;

        // UV V is flipped: screen Y=0 is top, but OpenGL V=0 is texture bottom.
        // So screen-top rows sample from V=1 (texture top) downward.
        float vTop = 1.0f;
        float vMidTop = 1.0f - vSlice;
        float vMidBot = vSlice;
        float vBot = 0.0f;

        // Top row
        AddQuadToBatch(texture, x, y, sliceSize, sliceSize,
            0.0f, vTop, uSlice, vMidTop, tint);
        AddQuadToBatch(texture, x + sliceSize, y, innerWidth, sliceSize,
            uSlice, vTop, 1.0f - uSlice, vMidTop, tint);
        AddQuadToBatch(texture, x + sliceSize + innerWidth, y, sliceSize, sliceSize,
            1.0f - uSlice, vTop, 1.0f, vMidTop, tint);

        // Middle row
        AddQuadToBatch(texture, x, y + sliceSize, sliceSize, innerHeight,
            0.0f, vMidTop, uSlice, vMidBot, tint);
        AddQuadToBatch(texture, x + sliceSize, y + sliceSize, innerWidth, innerHeight,
            uSlice, vMidTop, 1.0f - uSlice, vMidBot, tint);
        AddQuadToBatch(texture, x + sliceSize + innerWidth, y + sliceSize, sliceSize, innerHeight,
            1.0f - uSlice, vMidTop, 1.0f, vMidBot, tint);

        // Bottom row
        AddQuadToBatch(texture, x, y + sliceSize + innerHeight, sliceSize, sliceSize,
            0.0f, vMidBot, uSlice, vBot, tint);
        AddQuadToBatch(texture, x + sliceSize, y + sliceSize + innerHeight, innerWidth, sliceSize,
            uSlice, vMidBot, 1.0f - uSlice, vBot, tint);
        AddQuadToBatch(texture, x + sliceSize + innerWidth, y + sliceSize + innerHeight, sliceSize, sliceSize,
            1.0f - uSlice, vMidBot, 1.0f, vBot, tint);
    }

    void UiRenderer::AddQuadToBatch(Gfx::TextureHandle texture, float x, float y, float width, float height,
        float uvLeft, float uvTop, float uvRight, float uvBottom, const Colour& colour)
    {
        UiQuadBatch& batch = GetBatchForTexture(texture);

        unsigned int baseIdx = (unsigned int)batch.vertices.size();
        float fadedAlpha = colour.a * globalAlpha;

        // Top-left
        batch.vertices.push_back({x, y, uvLeft, uvTop, colour.r, colour.g, colour.b, fadedAlpha});
        // Bottom-left
        batch.vertices.push_back({x, y + height, uvLeft, uvBottom, colour.r, colour.g, colour.b, fadedAlpha});
        // Bottom-right
        batch.vertices.push_back({x + width, y + height, uvRight, uvBottom, colour.r, colour.g, colour.b, fadedAlpha});
        // Top-right
        batch.vertices.push_back({x + width, y, uvRight, uvTop, colour.r, colour.g, colour.b, fadedAlpha});

        batch.indices.push_back(baseIdx + 0);
        batch.indices.push_back(baseIdx + 1);
        batch.indices.push_back(baseIdx + 2);
        batch.indices.push_back(baseIdx + 0);
        batch.indices.push_back(baseIdx + 2);
        batch.indices.push_back(baseIdx + 3);
    }

    UiRenderer::UiQuadBatch& UiRenderer::GetBatchForTexture(Gfx::TextureHandle texture)
    {
        for (int i = 0; i < activeBatchCount; i++)
        {
            if (batches[i].texture == texture)
            {
                return batches[i];
            }
        }

        // Reuse an existing slot if available, otherwise grow
        if (activeBatchCount < (int)batches.size())
        {
            UiQuadBatch& batch = batches[activeBatchCount];
            batch.texture = texture;
            batch.vertices.clear();
            batch.indices.clear();
            activeBatchCount++;
            return batch;
        }

        batches.push_back(UiQuadBatch());
        batches.back().texture = texture;
        activeBatchCount++;
        return batches.back();
    }

    void UiRenderer::EndFrame()
    {
        if (activeBatchCount == 0)
        {
            return;
        }

        Gfx::RenderApi* gfxApi = Gfx::RenderApi::Get();
        Gfx::ShaderHandle shaderHandle = ShaderManager::Get()->GetShaderHandle("UiQuad");

        // Build / rebuild pipeline on shader change (handles hot-reload of UiQuad)
        if (shaderHandle != cachedShaderHandle)
        {
            if (pipelineHandle.IsValid())
            {
                gfxApi->DestroyPipeline(pipelineHandle);
            }

            Gfx::PipelineDescription desc;
            desc.shader                         = shaderHandle;
            desc.vertexLayout                   = MakeUiVertexLayout();
            desc.topology                       = Gfx::PrimitiveTopology::Triangles;
            desc.rasterizer.cullMode            = Gfx::CullMode::None;
            desc.rasterizer.frontFace           = Gfx::FrontFace::CounterClockwise;
            desc.depthStencil.depthTestEnabled  = false;
            desc.depthStencil.depthWriteEnabled = false;
            desc.blend.enabled                  = true;
            desc.blend.srcColorFactor           = Gfx::BlendFactor::SrcAlpha;
            desc.blend.dstColorFactor           = Gfx::BlendFactor::OneMinusSrcAlpha;
            desc.blend.colorOp                  = Gfx::BlendOp::Add;
            desc.blend.srcAlphaFactor           = Gfx::BlendFactor::One;
            desc.blend.dstAlphaFactor           = Gfx::BlendFactor::OneMinusSrcAlpha;
            desc.blend.alphaOp                  = Gfx::BlendOp::Add;
            pipelineHandle = gfxApi->CreatePipeline(desc);

            cachedShaderHandle = shaderHandle;
        }

        gfxApi->BindPipeline(pipelineHandle);
        gfxApi->BindUniformBuffer(UI_UNIFORMS_BINDING_SLOT, uniformBuffer, 0, UI_UNIFORMS_SIZE_BYTES);

        for (int i = 0; i < activeBatchCount; i++)
        {
            FlushBatch(batches[i]);
        }
    }

    void UiRenderer::FlushBatch(UiQuadBatch& batch)
    {
        if (batch.vertices.empty())
        {
            return;
        }

        Gfx::RenderApi* gfxApi = Gfx::RenderApi::Get();

        const int vbBytes = (int)(batch.vertices.size() * sizeof(UiVertex));
        const int ibBytes = (int)(batch.indices.size()  * sizeof(unsigned int));
        CC_ASSERT(vbBytes <= MAX_VERTEX_BUFFER_BYTES, "UiRenderer vertex batch exceeds buffer size");
        CC_ASSERT(ibBytes <= MAX_INDEX_BUFFER_BYTES,  "UiRenderer index batch exceeds buffer size");

        gfxApi->UpdateBuffer(vertexBuffer, 0, vbBytes, batch.vertices.data());
        gfxApi->UpdateBuffer(indexBuffer,  0, ibBytes, batch.indices.data());

        gfxApi->BindTexture(0, batch.texture, Gfx::SamplerHandle());
        gfxApi->BindVertexBuffer(0, vertexBuffer, 0, sizeof(UiVertex));
        gfxApi->BindIndexBuffer(indexBuffer, Gfx::IndexType::Uint32);
        gfxApi->DrawIndexed((int)batch.indices.size(), 1, 0, 0);
    }
}
