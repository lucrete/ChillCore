#include "Font.h"
#include "Texture.h"
#include "PrintManager.h"
#include "PlatformFileSystem.h"

#include <json.hpp>

namespace CC
{
    Font::Font(const std::string& jsonPath)
    {
        LoadFromJson(jsonPath);
    }

    Font::~Font()
    {
        delete atlasTexture;
    }

    const GlyphMetrics* Font::GetGlyph(int unicode) const
    {
        if (unicode >= 0 && unicode < ASCII_CACHE_SIZE)
        {
            return asciiCache[unicode];
        }
        auto it = glyphs.find(unicode);
        if (it != glyphs.end())
        {
            return &it->second;
        }
        return nullptr;
    }

    Texture* Font::GetAtlasTexture() const
    {
        return atlasTexture;
    }

    float Font::GetEmSize() const
    {
        return emSize;
    }

    float Font::GetLineHeight() const
    {
        return lineHeight;
    }

    float Font::GetAscender() const
    {
        return ascender;
    }

    float Font::GetDescender() const
    {
        return descender;
    }

    float Font::GetPixelRange() const
    {
        return pixelRange;
    }

    int Font::GetAtlasWidth() const
    {
        return atlasWidth;
    }

    float Font::GetAtlasSize() const
    {
        return atlasSize;
    }

    int Font::GetAtlasHeight() const
    {
        return atlasHeight;
    }

    bool Font::IsLoaded() const
    {
        return isLoaded;
    }

    void Font::LoadFromJson(const std::string& jsonPath)
    {
        std::string jsonContents;
        if (!PlatformFileSystem::Get()->ReadFileText(jsonPath.c_str(), jsonContents))
        {
            CCPrint(PrintManager::CHANNEL_WARN, "Font: Failed to open JSON: %s", jsonPath.c_str());
        }
        else
        {
            nlohmann::json root;
            bool parseOk = true;
            try
            {
                root = nlohmann::json::parse(jsonContents);
            }
            catch (const std::exception& e)
            {
                CCPrint(PrintManager::CHANNEL_WARN, "Font: Failed to parse JSON: %s - %s", jsonPath.c_str(), e.what());
                parseOk = false;
            }

            if (parseOk)
            {
                // ========================
                // Atlas metadata
                // ========================
                const auto& atlas = root["atlas"];
                pixelRange = atlas.value("distanceRange", 4.0f);
                atlasSize = atlas.value("size", 48.0f);
                atlasWidth = atlas.value("width", 0);
                atlasHeight = atlas.value("height", 0);

                // ========================
                // Font metrics
                // ========================
                const auto& metrics = root["metrics"];
                emSize = metrics.value("emSize", 1.0f);
                lineHeight = metrics.value("lineHeight", 1.0f);
                ascender = metrics.value("ascender", 0.0f);
                descender = metrics.value("descender", 0.0f);

                // ========================
                // Glyph metrics
                // ========================
                const auto& glyphArray = root["glyphs"];
                for (const auto& g : glyphArray)
                {
                    GlyphMetrics gm;
                    gm.unicode = g.value("unicode", 0);
                    gm.advance = g.value("advance", 0.0f);

                    if (g.contains("planeBounds"))
                    {
                        const auto& pb = g["planeBounds"];
                        gm.planeBoundsLeft = pb.value("left", 0.0f);
                        gm.planeBoundsBottom = pb.value("bottom", 0.0f);
                        gm.planeBoundsRight = pb.value("right", 0.0f);
                        gm.planeBoundsTop = pb.value("top", 0.0f);
                    }

                    if (g.contains("atlasBounds"))
                    {
                        const auto& ab = g["atlasBounds"];
                        gm.atlasBoundsLeft = ab.value("left", 0.0f);
                        gm.atlasBoundsBottom = ab.value("bottom", 0.0f);
                        gm.atlasBoundsRight = ab.value("right", 0.0f);
                        gm.atlasBoundsTop = ab.value("top", 0.0f);
                    }

                    glyphs[gm.unicode] = gm;
                }

                // Build ASCII fast-path lookup
                for (auto& pair : glyphs)
                {
                    if (pair.first >= 0 && pair.first < ASCII_CACHE_SIZE)
                    {
                        asciiCache[pair.first] = &pair.second;
                    }
                }

                // ========================
                // Load atlas texture
                // ========================
                // Derive .png path from .json path
                std::string pngPath = jsonPath.substr(0, jsonPath.length() - 5) + ".png";
                bool noMipmaps = false;
                atlasTexture = new Texture(pngPath, noMipmaps);

                isLoaded = true;
                CCPrint(PrintManager::CHANNEL_RENDER, "Font loaded: %s (%d glyphs, %dx%d atlas)",
                    jsonPath.c_str(), (int)glyphs.size(), atlasWidth, atlasHeight);
            }
        }
    }
}
