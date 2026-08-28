#include "LightManager.h"
#include "CCAssert.h"
#include "PrintManager.h"

namespace CC
{
    LightManager* LightManager::instance = NULL;

    LightManager::LightManager()
        : ambientLightInstensity(0.2f)
        , ambientLightColor(1.0f, 1.0f, 1.0f)
    {
        CC_ASSERT(instance == NULL, "LightManager already created");
        instance = this;

        // Create a default directional light pointing downward
        CreateLight("DefaultLight", Vector3(-0.5f, -1.0f, -0.3f), Vector3(1.0f, 1.0f, 1.0f));
    }

    LightManager::~LightManager()
    {
        instance = NULL;
    }

    LightManager* LightManager::Get()
    {
        CC_ASSERT(instance != NULL, "LightManager not created yet");
        return instance;
    }

    LightDirectional* LightManager::CreateLight(const std::string& name, const Vector3& direction, const Vector3& color)
    {
        auto it = lights.find(name);
        if (it != lights.end())
        {
            CCPrint(PrintManager::CHANNEL_WARN, "Light already exists: %s. Overwriting.", name.c_str());
            lights.erase(it);
        }

        auto light = std::make_unique<LightDirectional>(name, direction, color);
        LightDirectional* result = light.get();
        lights[name] = std::move(light);

        CCPrint(PrintManager::CHANNEL_RENDER, "Created directional light: %s", name.c_str());
        return result;
    }

    LightDirectional* LightManager::GetLight(const std::string& name)
    {
        auto it = lights.find(name);
        if (it != lights.end())
        {
            return it->second.get();
        }

        CCPrint(PrintManager::CHANNEL_WARN, "Light not found: %s, returning DefaultLight", name.c_str());
        return lights["DefaultLight"].get();
    }

    void LightManager::RemoveLight(const std::string& name)
    {
        if (name == "DefaultLight")
        {
            CCPrint(PrintManager::CHANNEL_WARN, "Cannot remove DefaultLight");
            return;
        }

        auto it = lights.find(name);
        if (it != lights.end())
        {
            lights.erase(it);
            CCPrint(PrintManager::CHANNEL_RENDER, "Removed light: %s", name.c_str());
        }
        else
        {
            CCPrint(PrintManager::CHANNEL_WARN, "Light not found: %s", name.c_str());
        }
    }

    void LightManager::Clear()
    {
        // Store the default light
        std::unique_ptr<LightDirectional> defaultLight = nullptr;
        auto it = lights.find("DefaultLight");
        if (it != lights.end())
        {
            defaultLight = std::move(it->second);
        }

        // Clear all lights
        lights.clear();

        // Restore the default light
        if (defaultLight)
        {
            lights["DefaultLight"] = std::move(defaultLight);
        }
        else
        {
            // Recreate default light if it was somehow missing
            CreateLight("DefaultLight", Vector3(0.5f, -1.0f, 0.3f), Vector3(1.0f, 1.0f, 1.0f));
        }

        CCPrint(PrintManager::CHANNEL_RENDER, "Cleared all lights (except DefaultLight)");
    }
}