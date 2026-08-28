#ifndef TEXTRENDERER_H
#define TEXTRENDERER_H

#include <string>
#include <vector>
#include "CCMat4x4.h"
#include "CCColour.h"
#include "UiStyle.h"
#include "GfxHandles.h"

namespace CC
{
    class Font;

    struct TextMetrics
    {
        float width = 0.0f;
        float height = 0.0f;
        int lineCount = 0;
    };

    struct TextVertex
    {
        float x, y;
        float u, v;
    };

    class TextRenderer
    {
    public:
        TextRenderer();
        ~TextRenderer();
        static TextRenderer* Get();

        void BeginFrame(int screenWidth, int screenHeight);
        void SetGlobalAlpha(float alpha);
        void DrawText(const std::string& text, Font* font, float fontSize, float x, float y,
            const Colour& colour, TextAlign alignment = TextAlign::Left, float maxWidth = 0.0f);
        void DrawCachedText(const std::vector<TextVertex>& vertices,
            const std::vector<unsigned int>& indices,
            Font* font, float fontSize, const Colour& colour);
        void LayoutTextToCache(const std::string& text, Font* font, float fontSize,
            float x, float y, TextAlign alignment, float maxWidth,
            std::vector<TextVertex>& outVertices, std::vector<unsigned int>& outIndices);
        TextMetrics MeasureText(const std::string& text, Font* font, float fontSize, float maxWidth = 0.0f);
        void EndFrame();

    private:
        struct TextBatch
        {
            Font* font = nullptr;
            float fontSize = 0.0f;
            Colour colour;
            std::vector<TextVertex> vertices;
            std::vector<unsigned int> indices;
        };

        struct LineInfo
        {
            int startVertex = 0;
            int vertexCount = 0;
            float width = 0.0f;
        };

        TextBatch& GetBatchForText(Font* font, float fontSize, const Colour& colour);
        void FlushBatch(TextBatch& batch);
        void LayoutText(const std::string& text, Font* font, float fontSize, float x, float y,
            TextAlign alignment, float maxWidth, TextBatch* batch, TextMetrics* metrics);

        static constexpr int MAX_VERTEX_BUFFER_BYTES = 512 * 1024;
        static constexpr int MAX_INDEX_BUFFER_BYTES  = 128 * 1024;

        static TextRenderer* instance;

        Gfx::PipelineHandle pipelineHandle;
        Gfx::BufferHandle   vertexBuffer;
        Gfx::BufferHandle   indexBuffer;
        Gfx::BufferHandle   uniformBuffer;

        Mat4x4 projection;
        int currentScreenWidth = 0;
        int currentScreenHeight = 0;

        float globalAlpha = 1.0f;

        std::vector<TextBatch> batches;
        int activeBatchCount = 0;

        // Cached shader handle (pipeline rebuilt on hot-reload)
        Gfx::ShaderHandle cachedShaderHandle;
    };
}

#endif // TEXTRENDERER_H
