#include "MaterialManager.h"
#include "CCAssert.h"
#include "TextureManager.h"

namespace CC
{
    MaterialManager* MaterialManager::instance = NULL;

    MaterialManager::MaterialManager()
    {
        CC_ASSERT(instance == NULL, "MaterialManager already created");
        instance = this;

        CreateMaterial("DefaultBasic", "DefaultBasic");

        // Fullscreen fade overlay. Alpha-blended so the underlying frame
        // shows through; the fadeAlpha uniform drives the 0→1 cover.
        // Depth test off because the overlay is screen-space and the
        // default backbuffer's depth values are not maintained.
        CreateMaterial("FullScreenFade", "FullScreenFade");
        GetMaterial("FullScreenFade")->SetAlphaMode(AlphaBlendMode::Blend);
        GetMaterial("FullScreenFade")->SetDepthTestEnabled(false);

        // Post-process pass for an offscreen scene render (AppStateRenderTarget).
        // Screen-space: depth test off, like the fade overlay.
        CreateMaterial("PostProcessInvert", "PostProcess");
        GetMaterial("PostProcessInvert")->SetDepthTestEnabled(false);

        CreateMaterial("ProcArt_Fractal2d", "Fractal2d");
        CreateMaterial("ProcArt_GradientViewer", "GradientViewer");
        CreateMaterial("ProcArt_UnitCircleRipples", "UnitCircleRipples");
        CreateMaterial("ProcArt_RadialWaves", "RadialWaves");
        CreateMaterial("ProcArt_TrigWaves", "TrigWaves");
        CreateMaterial("InWorldQuad", "TextureShader", "TestPattern");
        CreateMaterial("BlueTestPattern", "LitColour", "TestPattern", Vector3(1.0f, 1.0f, 1.0f), Vector2(5, 5));
        CreateMaterial("DevYellow", "LitColour", "", Vector3(1.0f, 1.0f, 0.0f));
        CreateMaterial("Concrete", "LitColour", "TestPattern", Vector3(0.93f, 0.92f, 0.90f));
    }

    MaterialManager::~MaterialManager()
    {
        instance = NULL;
    }

    MaterialManager* MaterialManager::Get()
    {
        CC_ASSERT(instance != NULL, "MaterialManager not created yet");
        return instance;
    }

    void MaterialManager::CreateMaterial(const std::string& name, const std::string& shaderName,
        const std::string& mainTex, const Vector3& baseColor, const Vector2& textureTiling, float opacity)
    {
        auto it = materials.find(name);
        CC_ASSERT(it == materials.end(), "Material already exists: " + name);
        if (it == materials.end())
        {
            auto material = std::make_unique<Material>(shaderName, mainTex, baseColor, textureTiling);
            material->SetOpacity(opacity);
            materials[name] = std::move(material);
        }
    }

    void MaterialManager::CreatePbrMaterial(const std::string& name,
        const std::string& baseColorTex,
        const std::string& metallicRoughnessTex,
        const std::string& normalTex,
        const std::string& occlusionTex,
        const std::string& emissiveTex,
        const Vector3& baseColorFactor,
        float metallicFactor,
        float roughnessFactor,
        const Vector3& emissiveFactor,
        float opacity)
    {
        auto it = materials.find(name);
        CC_ASSERT(it == materials.end(), "Material already exists: " + name);
        if (it == materials.end())
        {
            auto material = std::make_unique<Material>(
                "Pbr",
                baseColorTex,
                metallicRoughnessTex,
                normalTex,
                occlusionTex,
                emissiveTex,
                baseColorFactor,
                metallicFactor,
                roughnessFactor,
                emissiveFactor
            );
            material->SetOpacity(opacity);
            materials[name] = std::move(material);
        }
    }

    Material* MaterialManager::GetMaterial(const std::string& name)
    {
        auto it = materials.find(name);
        if (it != materials.end())
        {
            return it->second.get();
        }
        CC_ASSERT(false, "Material not found: " + name);
        return nullptr;
    }

    bool MaterialManager::HasMaterial(const std::string& name) const
    {
        return materials.find(name) != materials.end();
    }

    void MaterialManager::RemoveMaterial(const std::string& name)
    {
        auto it = materials.find(name);
        if (it != materials.end())
        {
            materials.erase(it);
        }
    }

    void MaterialManager::Clear()
    {
        materials.clear();
    }

    void MaterialManager::ReconnectShader(const std::string& shaderName)
    {
        for (auto i = materials.begin(); i != materials.end(); i++)
        {
            if (i->second->UsesShader(shaderName))
            {
                i->second->ReconnectShader();
            }
        }
    }
}