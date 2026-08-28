#include "TextRenderer.h"
#include "Font.h"
#include "Texture.h"
#include "ShaderManager.h"
#include "CCAssert.h"
#include "PrintManager.h"
#include "GfxRenderApi.h"
#include "GfxDescriptions.h"
#include "TextUniforms.h"

namespace CC
{
    TextRenderer* TextRenderer::instance = nullptr;

    static Gfx::VertexLayout MakeTextVertexLayout()
    {
        Gfx::VertexLayout layout;
        layout.strideBytes    = 4 * sizeof(float); // pos2 + uv2
        layout.attributeCount = 2;
        layout.attributes[0]  = { 0, 0                 , Gfx::VertexAttribType::Float32, 2 };
        layout.attributes[1]  = { 1, 2 * sizeof(float) , Gfx::VertexAttribType::Float32, 2 };
        return layout;
    }

    TextRenderer::TextRenderer()
    {
        CC_ASSERT(instance == nullptr, "TextRenderer already created");
        instance = this;

        Gfx::RenderApi* gfxApi = Gfx::RenderApi::Get();

        // Pre-allocated dynamic vertex and index buffers. FlushBatch writes
        // each batch's contents into them via UpdateBuffer (subData).
        Gfx::BufferDescription vbDesc;
        vbDesc.sizeBytes = MAX_VERTEX_BUFFER_BYTES;
        vbDesc.usage     = Gfx::BufferUsage::Vertex;
        vbDesc.memory    = Gfx::BufferMemory::CpuToGpu;
        vbDesc.debugName = "TextRenderer::Vertices";
        vertexBuffer = gfxApi->CreateBuffer(vbDesc);

        Gfx::BufferDescription ibDesc;
        ibDesc.sizeBytes = MAX_INDEX_BUFFER_BYTES;
        ibDesc.usage     = Gfx::BufferUsage::Index;
        ibDesc.memory    = Gfx::BufferMemory::CpuToGpu;
        ibDesc.debugName = "TextRenderer::Indices";
        indexBuffer = gfxApi->CreateBuffer(ibDesc);

        Gfx::BufferDescription uboDesc;
        uboDesc.sizeBytes = TEXT_UNIFORMS_SIZE_BYTES;
        uboDesc.usage     = Gfx::BufferUsage::Uniform;
        uboDesc.memory    = Gfx::BufferMemory::CpuToGpu;
        uboDesc.debugName = "TextRenderer::Uniforms";
        uniformBuffer = gfxApi->CreateBuffer(uboDesc);

        // Pipeline is built lazily in EndFrame so it can rebuild on shader
        // hot-reload of the MsdfText shader.
    }

    TextRenderer::~TextRenderer()
    {
        Gfx::RenderApi* gfxApi = Gfx::RenderApi::Get();
        if (uniformBuffer.IsValid())  { gfxApi->DestroyBuffer(uniformBuffer); }
        if (indexBuffer.IsValid())    { gfxApi->DestroyBuffer(indexBuffer); }
        if (vertexBuffer.IsValid())   { gfxApi->DestroyBuffer(vertexBuffer); }
        if (pipelineHandle.IsValid()) { gfxApi->DestroyPipeline(pipelineHandle); }
        instance = nullptr;
    }

    TextRenderer* TextRenderer::Get()
    {
        CC_ASSERT(instance != nullptr, "TextRenderer not created yet");
        return instance;
    }

    void TextRenderer::SetGlobalAlpha(float alpha)
    {
        globalAlpha = alpha;
    }

    void TextRenderer::BeginFrame(int screenWidth, int screenHeight)
    {
        currentScreenWidth = screenWidth;
        currentScreenHeight = screenHeight;

        // Y=0 at top, Y increases downward
        projection.Orthographic(0.0f, (float)screenWidth, (float)screenHeight, 0.0f, -1.0f, 1.0f);

        // Upload only the projection slice of the UBO. textColor / pixelRange
        // are written per-batch in FlushBatch.
        Gfx::RenderApi::Get()->UpdateBuffer(uniformBuffer,
            TEXT_UNIFORMS_PROJECTION_OFFSET,
            TEXT_UNIFORMS_PROJECTION_SIZE,
            (const float*)projection);

        // Clear batch contents but preserve allocated capacity
        for (int i = 0; i < activeBatchCount; i++)
        {
            batches[i].vertices.clear();
            batches[i].indices.clear();
        }
        activeBatchCount = 0;
    }

    TextRenderer::TextBatch& TextRenderer::GetBatchForText(Font* font, float fontSize, const Colour& colour)
    {
        for (int i = 0; i < activeBatchCount; i++)
        {
            if (batches[i].font == font && batches[i].fontSize == fontSize
                && batches[i].colour == colour)
            {
                return batches[i];
            }
        }

        // Reuse an existing slot if available, otherwise grow
        if (activeBatchCount < (int)batches.size())
        {
            TextBatch& batch = batches[activeBatchCount];
            batch.font = font;
            batch.fontSize = fontSize;
            batch.colour = colour;
            batch.vertices.clear();
            batch.indices.clear();
            activeBatchCount++;
            return batch;
        }

        batches.push_back(TextBatch());
        TextBatch& newBatch = batches.back();
        newBatch.font = font;
        newBatch.fontSize = fontSize;
        newBatch.colour = colour;
        activeBatchCount++;
        return newBatch;
    }

    void TextRenderer::DrawText(const std::string& text, Font* font, float fontSize, float x, float y,
        const Colour& colour, TextAlign alignment, float maxWidth)
    {
        if (!font || !font->IsLoaded() || text.empty())
        {
            return;
        }

        TextBatch& batch = GetBatchForText(font, fontSize, colour);
        LayoutText(text, font, fontSize, x, y, alignment, maxWidth, &batch, nullptr);
    }

    void TextRenderer::DrawCachedText(const std::vector<TextVertex>& vertices,
        const std::vector<unsigned int>& indices,
        Font* font, float fontSize, const Colour& colour)
    {
        if (vertices.empty() || !font)
        {
            return;
        }

        TextBatch& batch = GetBatchForText(font, fontSize, colour);

        unsigned int baseIdx = (unsigned int)batch.vertices.size();
        batch.vertices.insert(batch.vertices.end(), vertices.begin(), vertices.end());

        for (unsigned int idx : indices)
        {
            batch.indices.push_back(baseIdx + idx);
        }
    }

    void TextRenderer::LayoutTextToCache(const std::string& text, Font* font, float fontSize,
        float x, float y, TextAlign alignment, float maxWidth,
        std::vector<TextVertex>& outVertices, std::vector<unsigned int>& outIndices)
    {
        outVertices.clear();
        outIndices.clear();

        if (!font || !font->IsLoaded() || text.empty())
        {
            return;
        }

        // Use a temporary batch to capture the output
        TextBatch tempBatch;
        tempBatch.font = font;
        tempBatch.fontSize = fontSize;

        LayoutText(text, font, fontSize, x, y, alignment, maxWidth, &tempBatch, nullptr);

        outVertices = std::move(tempBatch.vertices);
        outIndices = std::move(tempBatch.indices);
    }

    TextMetrics TextRenderer::MeasureText(const std::string& text, Font* font, float fontSize, float maxWidth)
    {
        TextMetrics metrics;
        if (!font || !font->IsLoaded() || text.empty())
        {
            return metrics;
        }

        LayoutText(text, font, fontSize, 0.0f, 0.0f, TextAlign::Left, maxWidth, nullptr, &metrics);
        return metrics;
    }

    void TextRenderer::LayoutText(const std::string& text, Font* font, float fontSize, float x, float y,
        TextAlign alignment, float maxWidth, TextBatch* batch, TextMetrics* metrics)
    {
        float scale = fontSize / font->GetEmSize();
        float atlasW = (float)font->GetAtlasWidth();
        float atlasH = (float)font->GetAtlasHeight();
        float scaledLineHeight = font->GetLineHeight() * scale;
        float scaledAscender = font->GetAscender() * scale;

        // ========================
        // Word-wrap: split into lines
        // ========================
        struct Word
        {
            int startIndex;
            int length;
            float width;
        };

        std::vector<Word> words;
        int i = 0;
        int textLen = (int)text.length();

        while (i < textLen)
        {
            // Skip leading whitespace (include as separate "word")
            if (text[i] == ' ')
            {
                Word spaceWord;
                spaceWord.startIndex = i;
                spaceWord.length = 1;
                const GlyphMetrics* spaceGlyph = font->GetGlyph(' ');
                spaceWord.width = spaceGlyph ? spaceGlyph->advance * scale : fontSize * 0.25f;
                words.push_back(spaceWord);
                i++;
                continue;
            }

            // Collect non-space characters into a word
            Word word;
            word.startIndex = i;
            word.width = 0.0f;
            while (i < textLen && text[i] != ' ' && text[i] != '\n')
            {
                const GlyphMetrics* glyph = font->GetGlyph(text[i]);
                if (glyph)
                {
                    word.width += glyph->advance * scale;
                }
                i++;
            }
            word.length = i - word.startIndex;
            words.push_back(word);

            // Handle explicit newlines
            if (i < textLen && text[i] == '\n')
            {
                Word newlineWord;
                newlineWord.startIndex = i;
                newlineWord.length = 1;
                newlineWord.width = 0.0f;
                words.push_back(newlineWord);
                i++;
            }
        }

        // ========================
        // Break words into lines
        // ========================
        struct LineDef
        {
            std::vector<int> wordIndices;
            float width;
        };

        std::vector<LineDef> lines;
        LineDef currentLine;
        currentLine.width = 0.0f;

        for (int w = 0; w < (int)words.size(); w++)
        {
            // Explicit newline
            if (words[w].length == 1 && text[words[w].startIndex] == '\n')
            {
                lines.push_back(currentLine);
                currentLine = LineDef();
                currentLine.width = 0.0f;
                continue;
            }

            // Skip leading spaces on a new line
            bool isSpace = (words[w].length == 1 && text[words[w].startIndex] == ' ');
            if (isSpace && currentLine.wordIndices.empty())
            {
                continue;
            }

            float newWidth = currentLine.width + words[w].width;

            // Word-wrap if exceeding maxWidth
            if (maxWidth > 0.0f && newWidth > maxWidth && !currentLine.wordIndices.empty() && !isSpace)
            {
                // Remove trailing spaces from line width
                while (!currentLine.wordIndices.empty())
                {
                    int lastIdx = currentLine.wordIndices.back();
                    if (words[lastIdx].length == 1 && text[words[lastIdx].startIndex] == ' ')
                    {
                        currentLine.width -= words[lastIdx].width;
                        currentLine.wordIndices.pop_back();
                    }
                    else
                    {
                        break;
                    }
                }
                lines.push_back(currentLine);
                currentLine = LineDef();
                currentLine.width = 0.0f;
                newWidth = words[w].width;
            }

            currentLine.wordIndices.push_back(w);
            currentLine.width = newWidth;
        }
        if (!currentLine.wordIndices.empty())
        {
            lines.push_back(currentLine);
        }

        // ========================
        // Metrics only - no geometry
        // ========================
        if (metrics)
        {
            metrics->lineCount = (int)lines.size();
            metrics->height = lines.size() * scaledLineHeight;
            metrics->width = 0.0f;
            for (const auto& line : lines)
            {
                if (line.width > metrics->width)
                {
                    metrics->width = line.width;
                }
            }
            return;
        }

        // ========================
        // Generate glyph quads
        // ========================
        if (!batch)
        {
            return;
        }

        for (int lineIdx = 0; lineIdx < (int)lines.size(); lineIdx++)
        {
            const LineDef& line = lines[lineIdx];

            float lineOffsetX = 0.0f;
            if (alignment == TextAlign::Center)
            {
                lineOffsetX = (maxWidth > 0.0f ? maxWidth - line.width : -line.width) * 0.5f;
            }
            else if (alignment == TextAlign::Right)
            {
                lineOffsetX = maxWidth > 0.0f ? maxWidth - line.width : -line.width;
            }

            float cursorX = x + lineOffsetX;
            float baselineY = y + scaledAscender + lineIdx * scaledLineHeight;

            for (int wi = 0; wi < (int)line.wordIndices.size(); wi++)
            {
                const Word& word = words[line.wordIndices[wi]];

                for (int ci = 0; ci < word.length; ci++)
                {
                    char ch = text[word.startIndex + ci];
                    const GlyphMetrics* glyph = font->GetGlyph(ch);
                    if (!glyph)
                    {
                        continue;
                    }

                    // Space character has no visible quad
                    if (glyph->atlasBoundsLeft == 0.0f && glyph->atlasBoundsRight == 0.0f &&
                        glyph->planeBoundsLeft == 0.0f && glyph->planeBoundsRight == 0.0f)
                    {
                        cursorX += glyph->advance * scale;
                        continue;
                    }

                    // Glyph quad in screen space
                    // planeBounds are in EM units, scale to pixels
                    float quadLeft = cursorX + glyph->planeBoundsLeft * scale;
                    float quadRight = cursorX + glyph->planeBoundsRight * scale;
                    // planeBounds Y: bottom is lower, top is upper (in font coords)
                    // Screen space: Y increases downward, so flip
                    float quadTop = baselineY - glyph->planeBoundsTop * scale;
                    float quadBottom = baselineY - glyph->planeBoundsBottom * scale;

                    // Atlas UVs: PngLoader flips Y for OpenGL, so atlas yOrigin="bottom"
                    // maps directly to OpenGL V coordinates (V=0 at bottom)
                    float uvLeft = glyph->atlasBoundsLeft / atlasW;
                    float uvRight = glyph->atlasBoundsRight / atlasW;
                    float uvBottom = glyph->atlasBoundsBottom / atlasH;
                    float uvTop = glyph->atlasBoundsTop / atlasH;

                    unsigned int baseIdx = (unsigned int)batch->vertices.size();

                    // Top-left (screen top = glyph visual top = higher V)
                    batch->vertices.push_back({quadLeft, quadTop, uvLeft, uvTop});
                    // Bottom-left (screen bottom = glyph visual bottom = lower V)
                    batch->vertices.push_back({quadLeft, quadBottom, uvLeft, uvBottom});
                    // Bottom-right
                    batch->vertices.push_back({quadRight, quadBottom, uvRight, uvBottom});
                    // Top-right
                    batch->vertices.push_back({quadRight, quadTop, uvRight, uvTop});

                    batch->indices.push_back(baseIdx + 0);
                    batch->indices.push_back(baseIdx + 1);
                    batch->indices.push_back(baseIdx + 2);
                    batch->indices.push_back(baseIdx + 0);
                    batch->indices.push_back(baseIdx + 2);
                    batch->indices.push_back(baseIdx + 3);

                    cursorX += glyph->advance * scale;
                }
            }
        }
    }

    void TextRenderer::EndFrame()
    {
        if (activeBatchCount == 0)
        {
            return;
        }

        Gfx::RenderApi* gfxApi = Gfx::RenderApi::Get();
        Gfx::ShaderHandle shaderHandle = ShaderManager::Get()->GetShaderHandle("MsdfText");

        // Build / rebuild pipeline on shader change (handles hot-reload of MsdfText)
        if (shaderHandle != cachedShaderHandle)
        {
            if (pipelineHandle.IsValid())
            {
                gfxApi->DestroyPipeline(pipelineHandle);
            }

            Gfx::PipelineDescription desc;
            desc.shader                         = shaderHandle;
            desc.vertexLayout                   = MakeTextVertexLayout();
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
        gfxApi->BindUniformBuffer(TEXT_UNIFORMS_BINDING_SLOT, uniformBuffer, 0, TEXT_UNIFORMS_SIZE_BYTES);

        for (int i = 0; i < activeBatchCount; i++)
        {
            FlushBatch(batches[i]);
        }
    }

    void TextRenderer::FlushBatch(TextBatch& batch)
    {
        if (batch.vertices.empty() || !batch.font)
        {
            return;
        }

        Gfx::RenderApi* gfxApi = Gfx::RenderApi::Get();

        Colour fadedColour = batch.colour;
        fadedColour.a *= globalAlpha;

        // Screen-space pixel range = atlas distanceRange * (rendered size / atlas generation size)
        float atlasSize = batch.font->GetAtlasSize();
        float screenPxRange = batch.font->GetPixelRange() * (batch.fontSize / atlasSize);
        if (screenPxRange < 1.0f)
        {
            screenPxRange = 1.0f;
        }

        // Per-batch slice of the UBO: textColor + pixelRange (with std140 padding).
        struct
        {
            float textColor[4];
            float pixelRange;
            float pad[3];
        } batchData;
        batchData.textColor[0] = fadedColour.r;
        batchData.textColor[1] = fadedColour.g;
        batchData.textColor[2] = fadedColour.b;
        batchData.textColor[3] = fadedColour.a;
        batchData.pixelRange   = screenPxRange;
        gfxApi->UpdateBuffer(uniformBuffer,
            TEXT_UNIFORMS_BATCH_OFFSET,
            TEXT_UNIFORMS_BATCH_SIZE,
            &batchData);

        const int vbBytes = (int)(batch.vertices.size() * sizeof(TextVertex));
        const int ibBytes = (int)(batch.indices.size()  * sizeof(unsigned int));
        CC_ASSERT(vbBytes <= MAX_VERTEX_BUFFER_BYTES, "TextRenderer vertex batch exceeds buffer size");
        CC_ASSERT(ibBytes <= MAX_INDEX_BUFFER_BYTES,  "TextRenderer index batch exceeds buffer size");

        gfxApi->UpdateBuffer(vertexBuffer, 0, vbBytes, batch.vertices.data());
        gfxApi->UpdateBuffer(indexBuffer,  0, ibBytes, batch.indices.data());

        gfxApi->BindTexture(0, batch.font->GetAtlasTexture()->GetTextureHandle(), Gfx::SamplerHandle());
        gfxApi->BindVertexBuffer(0, vertexBuffer, 0, sizeof(TextVertex));
        gfxApi->BindIndexBuffer(indexBuffer, Gfx::IndexType::Uint32);
        gfxApi->DrawIndexed((int)batch.indices.size(), 1, 0, 0);
    }
}
