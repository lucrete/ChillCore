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

        AddTexture("TestPattern", "TestPattern.png");
        AddTexture("DefaultBase", "DefaultBase.png");
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

    Texture* TextureManager::GetTexture(const std::string& name)
    {
        // Check if texture exists
        if (textureMap.find(name) != textureMap.end())
        {
            return textureMap[name].get();
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
            textureMap[name] = std::make_unique<Texture>(filePath);
            return textureMap[name].get();
        }
        catch (const std::exception& e)
        {
            CCPrint(PrintManager::CHANNEL_WARN, "Failed to load texture: %s - %s", name.c_str(), e.what());

            // Return the default error texture
            return textureMap["default_error"].get();
        }
    }

    void TextureManager::AddTexture(const std::string& name, const std::string& filePath)
    {
        // Construct the full file path
        std::string fullPath = texturePath + filePath;
        AddTextureWithFullPath(name, fullPath);
    }

    void TextureManager::AddTextureWithFullPath(const std::string& name, const std::string& fullPath)
    {
        // Check if texture already exists
        if (textureMap.find(name) != textureMap.end())
        {
            CCPrint(PrintManager::CHANNEL_WARN, "Texture already exists: %s. Overwriting.", name.c_str());
            textureMap.erase(name);
        }

        // Create and store the texture
        try
        {
            textureMap[name] = std::make_unique<Texture>(fullPath);
            CCPrint(PrintManager::CHANNEL_RENDER, "Added texture: %s -> %s", name.c_str(), fullPath.c_str());
        }
        catch (const std::exception& e)
        {
            CCPrint(PrintManager::CHANNEL_WARN, "Failed to add texture: %s - %s", name.c_str(), e.what());
        }
    }

    void TextureManager::AddTextureFromMemory(const std::string& name, const unsigned char* data, int dataSize)
    {
        // Check if texture already exists
        if (textureMap.find(name) != textureMap.end())
        {
            CCPrint(PrintManager::CHANNEL_WARN, "Texture already exists: %s. Overwriting.", name.c_str());
            textureMap.erase(name);
        }

        // Create and store the texture from memory
        try
        {
            textureMap[name] = std::make_unique<Texture>(data, dataSize, name);
        }
        catch (const std::exception& e)
        {
            CCPrint(PrintManager::CHANNEL_WARN, "Failed to add texture from memory: %s - %s", name.c_str(), e.what());
        }
    }

    bool TextureManager::HasTexture(const std::string& name) const
    {
        return textureMap.find(name) != textureMap.end();
    }

    void TextureManager::RemoveTexture(const std::string& name)
    {
        // Do not allow removing the default error texture
        if (name == "default_error")
        {
            CCPrint(PrintManager::CHANNEL_WARN, "Cannot remove default_error texture");
            return;
        }

        auto it = textureMap.find(name);
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