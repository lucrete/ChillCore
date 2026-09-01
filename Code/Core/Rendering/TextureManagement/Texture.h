#ifndef TEXTURE_H
#define TEXTURE_H

#include <string>
#include "PngLoader.h"
#include "GfxHandles.h"

namespace CC
{
    // Whether a texture's bytes are sRGB-encoded colour or raw data.
    //
    // Colour textures are authored in sRGB and must be decoded before they
    // reach lighting maths. Sampling an sRGB format makes the hardware decode
    // on read, which happens before filtering — a shader-side pow() would
    // filter in the wrong space and give a different result.
    //
    // Normal maps, metallic-roughness and occlusion carry measurements, not
    // colour, and must not be decoded. UI and font atlases are drawn after the
    // post-process pass, straight into a display-referred backbuffer, so they
    // are not decoded either.
    enum class TextureColorSpace
    {
        Linear,
        Srgb
    };

    class Texture
    {
    public:
        // Load from file path. Mipmaps on by default — appropriate for 3D
        // scene content. UI / font callers that want crisp level-0 sampling
        // pass false explicitly (see Font::Font).
        Texture(const std::string& filePath,
                bool generateMipmaps = true,
                TextureColorSpace colorSpace = TextureColorSpace::Linear);

        // Load from memory buffer (PNG, JPEG, etc.)
        Texture(const unsigned char* data,
                int dataSize,
                const std::string& debugName = "",
                TextureColorSpace colorSpace = TextureColorSpace::Linear);

        virtual ~Texture();

        Gfx::TextureHandle GetTextureHandle() const;
        int GetWidth() const;
        int GetHeight() const;
        std::string GetFilePath() const;

    private:
        Gfx::TextureHandle textureHandle;
        std::string filePath;
        int width;
        int height;
        int bitsPerPixel;
    };
}

#endif // TEXTURE_H
