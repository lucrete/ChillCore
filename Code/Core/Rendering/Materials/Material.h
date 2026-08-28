#ifndef MATERIAL_H
#define MATERIAL_H

#include <unordered_map>
#include <string>
#include "ShaderManager.h"
#include "CCAssert.h"
#include "CCMat4x4.h"
#include "Texture.h"
#include "LightDirectional.h"
#include "CCVector2.h"
#include "CCVector4.h"
#include "GfxHandles.h"

namespace CC
{
    enum class AlphaBlendMode
    {
        Opaque = 0,
        Mask = 1,
        Blend = 2
    };

    class Material
    {
    public:
        Material() = delete;
        virtual ~Material();

        // Standard material constructor
        Material(const std::string& shaderName, const std::string& mainTex = "",
            const Vector3& baseColor = Vector3(1.0f, 1.0f, 1.0f),
            const Vector2& textureTiling = Vector2(1.0f, 1.0f));

        // PBR material constructor
        Material(const std::string& shaderName,
            const std::string& baseColorTex,
            const std::string& metallicRoughnessTex,
            const std::string& normalTex,
            const std::string& occlusionTex,
            const std::string& emissiveTex,
            const Vector3& baseColorFactor,
            float metallicFactor,
            float roughnessFactor,
            const Vector3& emissiveFactor);

        void SetStandardUniforms(const Mat4x4& value);

        void AddUniform(const std::string& name);

        template<typename T>
        void AddUniform(const std::string& name, const T& defaultValue)
        {
            Bind();
            AddUniform(name);
            auto it = uniformLocations.find(name);
            if (it != uniformLocations.end() && it->second != -1)
            {
                SetUniformInternal(it->second, defaultValue);
            }
        }

        template<typename T>
        void SetUniform(const std::string& name, T value)
        {
            if (ShaderManager::Get()->IsShaderCompiled(shaderName))
            {
                auto it = uniformLocations.find(name);
                CC_ASSERT(it != uniformLocations.end(), "Uniform not found: " + name);
                if (it != uniformLocations.end())
                {
                    if (it->second != -1)
                    {
                        SetUniformInternal(it->second, value);
                    }
                }
            }
        }

        bool UsesShader(const std::string& shaderName) const { return shaderName == shaderName; }
        void ReconnectShader();

        bool CheckUniformExists(const std::string& name);

        Texture* GetTexture() const { return texture; }
        bool HasTexture() const { return texture != nullptr; }

        const std::string& GetShaderName() const { return shaderName; }

        void SetTextureTiling(const Vector2& tiling) { textureTiling = tiling; materialUniformsDirty = true; }
        Vector2 GetTextureTiling() const { return textureTiling; }

        void SetOpacity(float value) { opacity = value; materialUniformsDirty = true; }
        float GetOpacity() const { return opacity; }
        bool IsTransparent() const { return opacity < 1.0f || alphaMode == AlphaBlendMode::Blend; }

        void SetAlphaMode(AlphaBlendMode mode) { alphaMode = mode; materialUniformsDirty = true; }
        AlphaBlendMode GetAlphaMode() const { return alphaMode; }
        void SetAlphaCutoff(float value) { alphaCutoff = value; materialUniformsDirty = true; }
        float GetAlphaCutoff() const { return alphaCutoff; }

        // Screen-space overlays (fullscreen fade, future HUD) have no
        // spatial relationship to scene depth. They must opt out of
        // depth test or risk gating against undefined default-backbuffer
        // depth values.
        void SetDepthTestEnabled(bool enabled) { depthTestEnabled = enabled; }
        bool GetDepthTestEnabled() const { return depthTestEnabled; }

        // Debug visualization modes for PBR materials
        // 0=normal, 1=UVs, 2=normals, 3=baseColor only, 4=metallicRoughness, 5=normal map
        void SetDebugMode(int mode) { debugMode = mode; materialUniformsDirty = true; }
        int GetDebugMode() const { return debugMode; }

    private:
        void Bind();
        int GetUniformLocation(const std::string& name) const;
        void UploadMaterialUniforms();

        void SetUniformInternal(int location, int value);
        void SetUniformInternal(int location, float value);
        void SetUniformInternal(int location, const Vector4& value);
        void SetUniformInternal(int location, const Vector3& value);
        void SetUniformInternal(int location, const Vector2& value);

        std::string shaderName;
        std::unordered_map<std::string, int> uniformLocations;

        // Base texture (mainTex / baseColor)
        Texture* texture = nullptr;
        Vector3 baseColour = { 1.0f, 1.0f, 1.0f };
        Vector2 textureTiling = { 1.0f, 1.0f };

        // PBR textures
        Texture* metallicRoughnessTexture = nullptr;
        Texture* normalTexture = nullptr;
        Texture* occlusionTexture = nullptr;
        Texture* emissiveTexture = nullptr;

        // PBR factors
        float metallicFactor = 1.0f;
        float roughnessFactor = 1.0f;
        Vector3 emissiveFactor = { 0.0f, 0.0f, 0.0f };

        float opacity = 1.0f;
        AlphaBlendMode alphaMode = AlphaBlendMode::Opaque;
        float alphaCutoff = 0.5f;
        bool depthTestEnabled = true;

        bool isPbrMaterial = false;

        // Debug mode for PBR visualization
        int debugMode = 0;

        Gfx::SamplerHandle samplerHandle;

        Gfx::BufferHandle materialUniformBuffer;
        bool materialUniformsDirty = true;
    };
}

#endif // MATERIAL_H