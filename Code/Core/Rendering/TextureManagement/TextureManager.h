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

        Texture* GetTexture(const std::string& name);
        void AddTexture(const std::string& name, const std::string& filePath);
        void AddTextureWithFullPath(const std::string& name, const std::string& fullPath);
        void AddTextureFromMemory(const std::string& name, const unsigned char* data, int dataSize);
        bool HasTexture(const std::string& name) const;
        void RemoveTexture(const std::string& name);
        void ClearTextures();
        void SetTexturePath(const std::string& path);
        std::string GetTexturePath() const;

    private:
        static TextureManager* instance;
        static const char* defaultTexturePath;

        std::map<std::string, std::unique_ptr<Texture>> textureMap;
        std::string texturePath;
    };
}

#endif // TEXTUREMANAGER_H