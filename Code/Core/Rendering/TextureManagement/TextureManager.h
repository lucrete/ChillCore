#ifndef TEXTUREMANAGER_H
#define TEXTUREMANAGER_H

#include "Texture.h"
#include <map>
#include <string>
#include <memory>

namespace CC
{
    class TextureManager
    {
    public:
        TextureManager();
        virtual ~TextureManager();

        static TextureManager* Get();

        // The colour space is part of a texture's identity, not a property of
        // the file: the same image is a decoded colour when it is albedo and
        // raw data when it is a mask. Each space gets its own cache entry, so
        // one file used in both roles yields two textures rather than
        // whichever role happened to load first.
        Texture* GetTexture(const std::string& name,
                            TextureColorSpace colorSpace = TextureColorSpace::Linear);
        void AddTexture(const std::string& name, const std::string& filePath,
                        TextureColorSpace colorSpace = TextureColorSpace::Linear);
        void AddTextureWithFullPath(const std::string& name, const std::string& fullPath,
                                    TextureColorSpace colorSpace = TextureColorSpace::Linear);
        void AddTextureFromMemory(const std::string& name, const unsigned char* data, int dataSize,
                                  TextureColorSpace colorSpace = TextureColorSpace::Linear);
        bool HasTexture(const std::string& name,
                        TextureColorSpace colorSpace = TextureColorSpace::Linear) const;
        void RemoveTexture(const std::string& name,
                           TextureColorSpace colorSpace = TextureColorSpace::Linear);
        void ClearTextures();
        void SetTexturePath(const std::string& path);
        std::string GetTexturePath() const;

    private:
        static TextureManager* instance;
        static const char* defaultTexturePath;

        // Cache key: the name for a linear texture, the name with a suffix for
        // an sRGB one. Linear keeps the bare name so the key is stable for
        // callers that never think about colour space.
        static std::string MakeKey(const std::string& name, TextureColorSpace colorSpace);

        std::map<std::string, std::unique_ptr<Texture>> textureMap;
        std::string texturePath;
    };
}

#endif // TEXTUREMANAGER_H