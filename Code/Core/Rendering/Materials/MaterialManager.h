#ifndef MATERIALMANAGER_H
#define MATERIALMANAGER_H

#pragma once

#include "Material.h"
#include "ShaderManager.h"
#include <unordered_map>
#include <memory>
#include <string>

namespace CC
{
    class MaterialManager
    {
    public:
        MaterialManager();
        ~MaterialManager();
        static MaterialManager* Get();

        void CreateMaterial(const std::string& name, const std::string& shaderName,
            const std::string& mainTex = "",
            const Vector3& baseColor = Vector3(1.0f, 1.0f, 1.0f),
            const Vector2& textureTiling = Vector2(1.0f, 1.0f),
            float opacity = 1.0f);

        void CreatePbrMaterial(const std::string& name,
            const std::string& baseColorTex,
            const std::string& metallicRoughnessTex,
            const std::string& normalTex,
            const std::string& occlusionTex,
            const std::string& emissiveTex,
            const Vector3& baseColorFactor,
            float metallicFactor,
            float roughnessFactor,
            const Vector3& emissiveFactor,
            float opacity = 1.0f);

        Material* GetMaterial(const std::string& name);
        bool HasMaterial(const std::string& name) const;
        void RemoveMaterial(const std::string& name);

        void Clear();
        void ReconnectShader(const std::string& shaderName);

    private:
        static MaterialManager* instance;
        std::unordered_map<std::string, std::unique_ptr<Material>> materials;
    };
}

#endif // MATERIALMANAGER_H