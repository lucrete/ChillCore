#include "Texture.h"
#include "PngLoader.h"
#include "CCAssert.h"
#include "PrintManager.h"
#include "GfxRenderApi.h"
#include "GfxDescriptions.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace CC
{
    // ========================
    // Helpers
    // ========================

    static Gfx::TextureFormat FormatForColorSpace(TextureColorSpace colorSpace)
    {
        return colorSpace == TextureColorSpace::Srgb
            ? Gfx::TextureFormat::Rgba8Srgb
            : Gfx::TextureFormat::Rgba8Unorm;
    }

    static int ComputeMipLevelCount(int width, int height)
    {
        int maxDimension = (width > height) ? width : height;
        int levels = 1;
        while (maxDimension > 1)
        {
            maxDimension /= 2;
            levels++;
        }
        return levels;
    }

    // ========================
    // Texture
    // ========================

    Texture::Texture(const std::string& filePath, bool generateMipmaps, TextureColorSpace colorSpace)
        : textureHandle(), filePath(filePath), width(0), height(0), bitsPerPixel(0)
    {
        try
        {
            PngLoader loader;
            PngData pngData = loader.Load(filePath);

            width = pngData.header.width;
            height = pngData.header.height;
            bitsPerPixel = 4; // PngData always gives RGBA format

            Gfx::TextureDescription description;
            description.width       = width;
            description.height      = height;
            description.mipLevels   = generateMipmaps ? ComputeMipLevelCount(width, height) : 1;
            description.format      = FormatForColorSpace(colorSpace);
            description.initialData = pngData.imageData.data();
            description.debugName   = filePath.c_str();

            textureHandle = Gfx::RenderApi::Get()->CreateTexture(description);

            CCPrint(PrintManager::CHANNEL_RENDER, "Texture loaded: %s (%dx%d, %d BPP)", filePath.c_str(), width, height, bitsPerPixel);
        }
        catch (const std::exception& e)
        {
            CCPrint(PrintManager::CHANNEL_WARN, "Failed to load texture: %s - %s", filePath.c_str(), e.what());
        }
    }

    Texture::Texture(const unsigned char* data, int dataSize, const std::string& debugName, TextureColorSpace colorSpace)
        : textureHandle(), filePath(debugName), width(0), height(0), bitsPerPixel(0)
    {
        int channels = 0;
        stbi_set_flip_vertically_on_load(true);
        unsigned char* imageData = stbi_load_from_memory(data, dataSize, &width, &height, &channels, 4);

        if (imageData == nullptr)
        {
            CCPrint(PrintManager::CHANNEL_WARN, "Failed to decode texture from memory: %s - %s",
                debugName.c_str(), stbi_failure_reason());
        }
        else
        {
            bitsPerPixel = 4;

            Gfx::TextureDescription description;
            description.width       = width;
            description.height      = height;
            description.mipLevels   = ComputeMipLevelCount(width, height);
            description.format      = FormatForColorSpace(colorSpace);
            description.initialData = imageData;
            description.debugName   = debugName.c_str();

            textureHandle = Gfx::RenderApi::Get()->CreateTexture(description);

            stbi_image_free(imageData);

            CCPrint(PrintManager::CHANNEL_RENDER, "Texture loaded from memory: %s (%dx%d)", debugName.c_str(), width, height);
        }
    }

    Texture::~Texture()
    {
        if (textureHandle.IsValid())
        {
            Gfx::RenderApi::Get()->DestroyTexture(textureHandle);
        }
    }

    Gfx::TextureHandle Texture::GetTextureHandle() const
    {
        return textureHandle;
    }

    int Texture::GetWidth() const
    {
        return width;
    }

    int Texture::GetHeight() const
    {
        return height;
    }

    std::string Texture::GetFilePath() const
    {
        return filePath;
    }
}
