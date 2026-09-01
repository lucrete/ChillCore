#include "TextureManager.h"
#include "CCAssert.h"
#include "PrintManager.h"

namespace CC
{
    TextureManager* TextureManager::instance = NULL;
    const char* TextureManager::defaultTexturePath = "Data\\Textures\\";

    TextureManager::TextureManager()
        : texturePath(defaultTexturePath)
    {
        CC_ASSERT(instance == NULL, "TextureManager already created");
        instance = this;

        // Both are base-colour textures, so they are decoded on sample.
        AddTexture("TestPattern", "TestPattern.png", TextureColorSpace::Srgb);
        AddTexture("DefaultBase", "DefaultBase.png", TextureColorSpace::Srgb);
    }

    TextureManager::~TextureManager()
    {
        ClearTextures();
        instance = NULL;
    }

    TextureManager* TextureManager::Get()
    {
        CC_ASSERT(instance != NULL, "TextureManager not created yet");
        return instance;
    }

    std::string TextureManager::MakeKey(const std::string& name, TextureColorSpace colorSpace)
    {
        return colorSpace == TextureColorSpace::Srgb ? name + "#srgb" : name;
    }

    Texture* TextureManager::GetTexture(const std::string& name, TextureColorSpace colorSpace)
    {
        std::string key = MakeKey(name, colorSpace);

        // Check if texture exists
        if (textureMap.find(key) != textureMap.end())
        {
            return textureMap[key].get();
        }

        // If not found, try to load it assuming the name is also the filename
        std::string filePath = texturePath + name;
        if (name.find(".") == std::string::npos)
        {
            // Add .png extension if no extension is provided
            filePath += ".png";
        }

        try
        {
            textureMap[key] = std::make_unique<Texture>(filePath, true, colorSpace);
            return textureMap[key].get();
        }
        catch (const std::exception& e)
        {
            CCPrint(PrintManager::CHANNEL_WARN, "Failed to load texture: %s - %s", name.c_str(), e.what());

            // Return the default error texture
            return textureMap["default_error"].get();
        }
    }

    void TextureManager::AddTexture(const std::string& name, const std::string& filePath, TextureColorSpace colorSpace)
    {
        // Construct the full file path
        std::string fullPath = texturePath + filePath;
        AddTextureWithFullPath(name, fullPath, colorSpace);
    }

    void TextureManager::AddTextureWithFullPath(const std::string& name, const std::string& fullPath, TextureColorSpace colorSpace)
    {
        std::string key = MakeKey(name, colorSpace);

        // Check if texture already exists
        if (textureMap.find(key) != textureMap.end())
        {
            CCPrint(PrintManager::CHANNEL_WARN, "Texture already exists: %s. Overwriting.", name.c_str());
            textureMap.erase(key);
        }

        // Create and store the texture
        try
        {
            textureMap[key] = std::make_unique<Texture>(fullPath, true, colorSpace);
            CCPrint(PrintManager::CHANNEL_RENDER, "Added texture: %s -> %s", name.c_str(), fullPath.c_str());
        }
        catch (const std::exception& e)
        {
            CCPrint(PrintManager::CHANNEL_WARN, "Failed to add texture: %s - %s", name.c_str(), e.what());
        }
    }

    void TextureManager::AddTextureFromMemory(const std::string& name, const unsigned char* data, int dataSize, TextureColorSpace colorSpace)
    {
        std::string key = MakeKey(name, colorSpace);

        // Check if texture already exists
        if (textureMap.find(key) != textureMap.end())
        {
            CCPrint(PrintManager::CHANNEL_WARN, "Texture already exists: %s. Overwriting.", name.c_str());
            textureMap.erase(key);
        }

        // Create and store the texture from memory
        try
        {
            textureMap[key] = std::make_unique<Texture>(data, dataSize, name, colorSpace);
        }
        catch (const std::exception& e)
        {
            CCPrint(PrintManager::CHANNEL_WARN, "Failed to add texture from memory: %s - %s", name.c_str(), e.what());
        }
    }

    bool TextureManager::HasTexture(const std::string& name, TextureColorSpace colorSpace) const
    {
        return textureMap.find(MakeKey(name, colorSpace)) != textureMap.end();
    }

    void TextureManager::RemoveTexture(const std::string& name, TextureColorSpace colorSpace)
    {
        // Do not allow removing the default error texture
        if (name == "default_error")
        {
            CCPrint(PrintManager::CHANNEL_WARN, "Cannot remove default_error texture");
            return;
        }

        auto it = textureMap.find(MakeKey(name, colorSpace));
        if (it != textureMap.end())
        {
            textureMap.erase(it);
            CCPrint(PrintManager::CHANNEL_RENDER, "Removed texture: %s", name.c_str());
        }
        else
        {
            CCPrint(PrintManager::CHANNEL_WARN, "Texture not found: %s", name.c_str());
        }
    }

    void TextureManager::ClearTextures()
    {
        // Store the default error texture if it exists
        std::unique_ptr<Texture> defaultErrorTexture = nullptr;
        auto it = textureMap.find("default_error");
        if (it != textureMap.end())
        {
            defaultErrorTexture = std::move(it->second);
        }

        // Clear all textures
        textureMap.clear();

        // Restore the default error texture
        if (defaultErrorTexture)
        {
            textureMap["default_error"] = std::move(defaultErrorTexture);
        }

        CCPrint(PrintManager::CHANNEL_RENDER, "Cleared all textures (except default_error)");
    }

    void TextureManager::SetTexturePath(const std::string& path)
    {
        texturePath = path;

        // Ensure path ends with a separator
        if (!texturePath.empty() &&
            texturePath[texturePath.length() - 1] != '\\' &&
            texturePath[texturePath.length() - 1] != '/')
        {
            texturePath += '\\';
        }

        CCPrint(PrintManager::CHANNEL_RENDER, "Texture path set to: %s", texturePath.c_str());
    }

    std::string TextureManager::GetTexturePath() const
    {
        return texturePath;
    }
}