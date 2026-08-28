#ifndef TEXTURE_H
#define TEXTURE_H

#include <string>
#include "PngLoader.h"
#include "GfxHandles.h"

namespace CC
{

    class Texture
    {
    public:
        // Load from file path. Mipmaps on by default — appropriate for 3D
        // scene content. UI / font callers that want crisp level-0 sampling
        // pass false explicitly (see Font::Font).
        Texture(const std::string& filePath, bool generateMipmaps = true);

        // Load from memory buffer (PNG, JPEG, etc.)
        Texture(const unsigned char* data, int dataSize, const std::string& debugName = "");

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
