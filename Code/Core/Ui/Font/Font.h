#ifndef FONT_H
#define FONT_H

#include <string>
#include <unordered_map>

namespace CC
{
    class Texture;

    struct GlyphMetrics
    {
        int unicode = 0;
        float advance = 0.0f;
        float planeBoundsLeft = 0.0f;
        float planeBoundsBottom = 0.0f;
        float planeBoundsRight = 0.0f;
        float planeBoundsTop = 0.0f;
        float atlasBoundsLeft = 0.0f;
        float atlasBoundsBottom = 0.0f;
        float atlasBoundsRight = 0.0f;
        float atlasBoundsTop = 0.0f;
    };

    class Font
    {
    public:
        Font(const std::string& jsonPath);
        ~Font();

        const GlyphMetrics* GetGlyph(int unicode) const;
        Texture* GetAtlasTexture() const;
        float GetEmSize() const;
        float GetLineHeight() const;
        float GetAscender() const;
        float GetDescender() const;
        float GetPixelRange() const;
        float GetAtlasSize() const;
        int GetAtlasWidth() const;
        int GetAtlasHeight() const;
        bool IsLoaded() const;

    private:
        void LoadFromJson(const std::string& jsonPath);

        static constexpr int ASCII_CACHE_SIZE = 128;

        Texture* atlasTexture = nullptr;
        std::unordered_map<int, GlyphMetrics> glyphs;
        const GlyphMetrics* asciiCache[ASCII_CACHE_SIZE] = {};

        float emSize = 1.0f;
        float lineHeight = 1.0f;
        float ascender = 0.0f;
        float descender = 0.0f;
        float pixelRange = 0.0f;
        float atlasSize = 0.0f;
        int atlasWidth = 0;
        int atlasHeight = 0;
        bool isLoaded = false;
    };
}

#endif // FONT_H
