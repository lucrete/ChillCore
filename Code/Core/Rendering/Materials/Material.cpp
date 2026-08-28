#include "Material.h"
#include "ShaderManager.h"
#include "TextureManager.h"
#include "CameraManager.h"
#include "PrintManager.h"
#include "GfxRenderApi.h"
#include "GfxDescriptions.h"
#include "MaterialUniforms.h"
#include "ObjectUniforms.h"

namespace CC
{
    static Gfx::BufferHandle CreateMaterialUniformBuffer()
    {
        Gfx::BufferDescription description;
        description.sizeBytes = static_cast<int>(sizeof(MaterialUniforms));
        description.usage     = Gfx::BufferUsage::Uniform;
        description.memory    = Gfx::BufferMemory::CpuToGpu;
        description.debugName = "MaterialUniforms";
        return Gfx::RenderApi::Get()->CreateBuffer(description);
    }

    static Gfx::SamplerHandle CreateRepeatLinearSampler()
    {
        Gfx::SamplerDescription samplerDesc;
        samplerDesc.minFilter     = Gfx::FilterMode::Linear;
        samplerDesc.magFilter     = Gfx::FilterMode::Linear;
        samplerDesc.mipmapMode    = Gfx::MipmapMode::Linear;
        samplerDesc.addressU      = Gfx::AddressMode::Repeat;
        samplerDesc.addressV      = Gfx::AddressMode::Repeat;
        samplerDesc.addressW      = Gfx::AddressMode::Repeat;
        samplerDesc.maxAnisotropy = 1.0f;
        samplerDesc.debugName     = "Material::RepeatLinearSampler";
        return Gfx::RenderApi::Get()->CreateSampler(samplerDesc);
    }

    Material::~Material()
    {
        if (samplerHandle.IsValid())
        {
            Gfx::RenderApi::Get()->DestroySampler(samplerHandle);
        }
        if (materialUniformBuffer.IsValid())
        {
            Gfx::RenderApi::Get()->DestroyBuffer(materialUniformBuffer);
        }
    }

    Material::Material(const std::string& _shaderName, const std::string& mainTex,
        const Vector3& _baseColor, const Vector2& _textureTiling)
        : shaderName(_shaderName)
        , texture(nullptr)
        , baseColour(_baseColor)
        , textureTiling(_textureTiling)
        , metallicRoughnessTexture(nullptr)
        , normalTexture(nullptr)
        , occlusionTexture(nullptr)
        , emissiveTexture(nullptr)
        , metallicFactor(1.0f)
        , roughnessFactor(1.0f)
        , emissiveFactor(0.0f, 0.0f, 0.0f)
        , isPbrMaterial(false)
    {
        texture = TextureManager::Get()->GetTexture(mainTex.empty() ? "DefaultBase" : mainTex);

        samplerHandle = CreateRepeatLinearSampler();
        materialUniformBuffer = CreateMaterialUniformBuffer();
    }

    Material::Material(const std::string& _shaderName,
        const std::string& baseColorTex,
        const std::string& metallicRoughnessTex,
        const std::string& normalTex,
        const std::string& occlusionTex,
        const std::string& emissiveTex,
        const Vector3& baseColorFactor,
        float _metallicFactor,
        float _roughnessFactor,
        const Vector3& _emissiveFactor)
        : shaderName(_shaderName)
        , texture(nullptr)
        , baseColour(baseColorFactor)
        , textureTiling(1.0f, 1.0f)
        , metallicRoughnessTexture(nullptr)
        , normalTexture(nullptr)
        , occlusionTexture(nullptr)
        , emissiveTexture(nullptr)
        , metallicFactor(_metallicFactor)
        , roughnessFactor(_roughnessFactor)
        , emissiveFactor(_emissiveFactor)
        , isPbrMaterial(true)
    {
        // Load base color texture
        texture = TextureManager::Get()->GetTexture(baseColorTex.empty() ? "DefaultBase" : baseColorTex);

        // Load PBR textures (use DefaultBase as fallback for missing textures)
        if (!metallicRoughnessTex.empty() && TextureManager::Get()->HasTexture(metallicRoughnessTex))
        {
            metallicRoughnessTexture = TextureManager::Get()->GetTexture(metallicRoughnessTex);
        }

        if (!normalTex.empty() && TextureManager::Get()->HasTexture(normalTex))
        {
            normalTexture = TextureManager::Get()->GetTexture(normalTex);
        }

        if (!occlusionTex.empty() && TextureManager::Get()->HasTexture(occlusionTex))
        {
            occlusionTexture = TextureManager::Get()->GetTexture(occlusionTex);
        }

        if (!emissiveTex.empty() && TextureManager::Get()->HasTexture(emissiveTex))
        {
            emissiveTexture = TextureManager::Get()->GetTexture(emissiveTex);
        }

        samplerHandle = CreateRepeatLinearSampler();
        materialUniformBuffer = CreateMaterialUniformBuffer();
    }

    void Material::Bind()
    {
        Gfx::ShaderHandle handle;
        if (ShaderManager::Get()->IsShaderCompiled(shaderName))
        {
            handle = ShaderManager::Get()->GetShaderHandle(shaderName);
        }
        else
        {
            handle = ShaderManager::Get()->GetShaderHandle("DefaultError");
        }
        Gfx::RenderApi::Get()->BindShaderProgram(handle);
    }

    int Material::GetUniformLocation(const std::string& name) const
    {
        Gfx::ShaderHandle handle = ShaderManager::Get()->GetShaderHandle(shaderName);
        return Gfx::RenderApi::Get()->GetUniformLocation(handle, name.c_str());
    }

    void Material::UploadMaterialUniforms()
    {
        MaterialUniforms data = {};

        data.baseColorAndOpacity[0] = baseColour.x;
        data.baseColorAndOpacity[1] = baseColour.y;
        data.baseColorAndOpacity[2] = baseColour.z;
        data.baseColorAndOpacity[3] = opacity;

        data.textureTilingAndAlphaCutoff[0] = textureTiling.x;
        data.textureTilingAndAlphaCutoff[1] = textureTiling.y;
        data.textureTilingAndAlphaCutoff[2] = alphaCutoff;
        data.textureTilingAndAlphaCutoff[3] = 0.0f;

        data.pbrFactors[0] = metallicFactor;
        data.pbrFactors[1] = roughnessFactor;
        data.pbrFactors[2] = 0.0f;
        data.pbrFactors[3] = 0.0f;

        data.emissiveFactor[0] = emissiveFactor.x;
        data.emissiveFactor[1] = emissiveFactor.y;
        data.emissiveFactor[2] = emissiveFactor.z;
        data.emissiveFactor[3] = 0.0f;

        data.alphaAndDebugMode[0] = static_cast<int>(alphaMode);
        data.alphaAndDebugMode[1] = debugMode;
        data.alphaAndDebugMode[2] = 0;
        data.alphaAndDebugMode[3] = 0;

        data.textureFlags[0] = normalTexture            != nullptr ? 1 : 0;
        data.textureFlags[1] = metallicRoughnessTexture != nullptr ? 1 : 0;
        data.textureFlags[2] = occlusionTexture         != nullptr ? 1 : 0;
        data.textureFlags[3] = emissiveTexture          != nullptr ? 1 : 0;

        Gfx::RenderApi::Get()->UpdateBuffer(materialUniformBuffer, 0, static_cast<int>(sizeof(MaterialUniforms)), &data);
    }

    void Material::SetStandardUniforms(const Mat4x4& model)
    {
        if (ShaderManager::Get()->IsShaderCompiled(shaderName))
        {
            Gfx::RenderApi* gfxApi = Gfx::RenderApi::Get();

            if (materialUniformsDirty && materialUniformBuffer.IsValid())
            {
                UploadMaterialUniforms();
                materialUniformsDirty = false;
            }
            gfxApi->BindUniformBuffer(MATERIAL_UNIFORMS_BINDING_SLOT, materialUniformBuffer, 0, static_cast<int>(sizeof(MaterialUniforms)));

            // Push tier: per-draw object uniforms (mvp + model). One UBO
            // update per draw, bound at OBJECT_UNIFORMS_BINDING_SLOT.
            Mat4x4 viewProj = CameraManager::Get()->GetViewProjectionMatrix();
            Mat4x4 mvp = viewProj * model;
            ObjectUniforms objectUniforms;
            const float* mvpData   = mvp;
            const float* modelData = model;
            for (int i = 0; i < 16; i++)
            {
                objectUniforms.mvp[i]   = mvpData[i];
                objectUniforms.model[i] = modelData[i];
            }
            gfxApi->SetPushConstants(&objectUniforms, OBJECT_UNIFORMS_SIZE_BYTES);

            // Bind base color texture (unit 0). Shader declares layout(binding=0) so no sampler uniform upload needed.
            if (texture != nullptr && texture->GetTextureHandle().IsValid())
            {
                gfxApi->BindTexture(0, texture->GetTextureHandle(), samplerHandle);
            }

            // PBR material bindings. Shader declares layout(binding=1..4).
            if (isPbrMaterial)
            {
                // Default fallback texture for missing PBR maps
                Texture* defaultTex = TextureManager::Get()->GetTexture("DefaultBase");
                Gfx::TextureHandle defaultHandle = defaultTex ? defaultTex->GetTextureHandle() : Gfx::TextureHandle();

                Gfx::TextureHandle mrHandle       = metallicRoughnessTexture ? metallicRoughnessTexture->GetTextureHandle() : defaultHandle;
                Gfx::TextureHandle normalHandle   = normalTexture            ? normalTexture->GetTextureHandle()            : defaultHandle;
                Gfx::TextureHandle occlusionHandle= occlusionTexture         ? occlusionTexture->GetTextureHandle()         : defaultHandle;
                Gfx::TextureHandle emissiveHandle = emissiveTexture          ? emissiveTexture->GetTextureHandle()          : defaultHandle;

                if (mrHandle.IsValid())        { gfxApi->BindTexture(1, mrHandle,        samplerHandle); }
                if (normalHandle.IsValid())    { gfxApi->BindTexture(2, normalHandle,    samplerHandle); }
                if (occlusionHandle.IsValid()) { gfxApi->BindTexture(3, occlusionHandle, samplerHandle); }
                if (emissiveHandle.IsValid())  { gfxApi->BindTexture(4, emissiveHandle,  samplerHandle); }
            }
        }
    }

    void Material::AddUniform(const std::string& name)
    {
        Gfx::ShaderHandle handle = ShaderManager::Get()->GetShaderHandle(shaderName);
        int location = Gfx::RenderApi::Get()->GetUniformLocation(handle, name.c_str());
        uniformLocations[name] = location;
    }

    void Material::ReconnectShader()
    {
        Gfx::ShaderHandle handle = ShaderManager::Get()->GetShaderHandle(shaderName);
        for (auto i = uniformLocations.begin(); i != uniformLocations.end(); i++)
        {
            i->second = Gfx::RenderApi::Get()->GetUniformLocation(handle, i->first.c_str());
        }
    }

    bool Material::CheckUniformExists(const std::string& name)
    {
        Gfx::ShaderHandle handle = ShaderManager::Get()->GetShaderHandle(shaderName);
        int location = Gfx::RenderApi::Get()->GetUniformLocation(handle, name.c_str());
        return location != -1;
    }

    void Material::SetUniformInternal(int location, int value)
    {
        Gfx::RenderApi::Get()->SetUniformInt(location, value);
    }

    void Material::SetUniformInternal(int location, float value)
    {
        Gfx::RenderApi::Get()->SetUniformFloat(location, value);
    }

    void Material::SetUniformInternal(int location, const Vector4& value)
    {
        Gfx::RenderApi::Get()->SetUniformVec4(location, value);
    }

    void Material::SetUniformInternal(int location, const Vector3& value)
    {
        Gfx::RenderApi::Get()->SetUniformVec3(location, value);
    }

    void Material::SetUniformInternal(int location, const Vector2& value)
    {
        Gfx::RenderApi::Get()->SetUniformVec2(location, value);
    }
}