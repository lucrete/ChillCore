#ifndef UIRENDERER_H
#define UIRENDERER_H

#include <vector>
#include "CCMat4x4.h"
#include "CCColour.h"
#include "GfxHandles.h"

namespace CC
{
    class UiRenderer
    {
    public:
        UiRenderer();
        ~UiRenderer();
        static UiRenderer* Get();

        void BeginFrame(int screenWidth, int screenHeight);
        void SetGlobalAlpha(float alpha);
        void DrawQuad(float x, float y, float width, float height, const Colour& colour);
        void DrawTexturedQuad(float x, float y, float width, float height,
            float uvLeft, float uvTop, float uvRight, float uvBottom,
            const Colour& tint, Gfx::TextureHandle texture);
        void DrawSlice9(float x, float y, float width, float height,
            Gfx::TextureHandle texture, float sliceSize, int textureWidth, int textureHeight, const Colour& tint);
        void EndFrame();

    private:
        struct UiVertex
        {
            float x, y;
            float u, v;
            float r, g, b, a;
        };

        struct UiQuadBatch
        {
            Gfx::TextureHandle texture;
            std::vector<UiVertex> vertices;
            std::vector<unsigned int> indices;
        };

        void AddQuadToBatch(Gfx::TextureHandle texture, float x, float y, float width, float height,
            float uvLeft, float uvTop, float uvRight, float uvBottom, const Colour& colour);
        UiQuadBatch& GetBatchForTexture(Gfx::TextureHandle texture);
        void FlushBatch(UiQuadBatch& batch);

        static constexpr int MAX_VERTEX_BUFFER_BYTES = 256 * 1024;
        static constexpr int MAX_INDEX_BUFFER_BYTES  = 64 * 1024;

        static UiRenderer* instance;

        Gfx::PipelineHandle pipelineHandle;
        Gfx::BufferHandle   vertexBuffer;
        Gfx::BufferHandle   indexBuffer;
        Gfx::BufferHandle   uniformBuffer;
        Gfx::TextureHandle  whiteTexture;

        Mat4x4 projection;
        int currentScreenWidth = 0;
        int currentScreenHeight = 0;

        float globalAlpha = 1.0f;

        std::vector<UiQuadBatch> batches;
        int activeBatchCount = 0;

        // Cached shader handle (pipeline rebuilt on hot-reload)
        Gfx::ShaderHandle cachedShaderHandle;
    };
}

#endif // UIRENDERER_H
