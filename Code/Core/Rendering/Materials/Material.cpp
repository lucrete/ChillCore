#include "Material.h"

#include <cstring>

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
        if (materialParamsBuffer.IsValid())
        {
            Gfx::RenderApi::Get()->DestroyBuffer(materialParamsBuffer);
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

            // Custom Params: the shader-declared MaterialParams block.
            // Absent for every shader that uses only the standard params.
            if (materialParamsDirty)
            {
                PackAndUploadParams();
            }
            if (materialParamsBuffer.IsValid())
            {
                gfxApi->BindUniformBuffer(MATERIAL_PARAMS_BINDING_SLOT, materialParamsBuffer, 0, materialParamsBufferSizeBytes);
            }

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

    void Material::SetUniform(const std::string& name, float value)
    {
        SetParamValue(name, ShaderParamType::Float, &value, static_cast<int>(sizeof(value)));
    }

    void Material::SetUniform(const std::string& name, int value)
    {
        SetParamValue(name, ShaderParamType::Int, &value, static_cast<int>(sizeof(value)));
    }

    void Material::SetUniform(const std::string& name, const Vector2& value)
    {
        float components[2] = { value.x, value.y };
        SetParamValue(name, ShaderParamType::Vector2, components, static_cast<int>(sizeof(components)));
    }

    void Material::SetUniform(const std::string& name, const Vector3& value)
    {
        float components[3] = { value.x, value.y, value.z };
        SetParamValue(name, ShaderParamType::Vector3, components, static_cast<int>(sizeof(components)));
    }

    void Material::SetUniform(const std::string& name, const Vector4& value)
    {
        float components[4] = { value.x, value.y, value.z, value.w };
        SetParamValue(name, ShaderParamType::Vector4, components, static_cast<int>(sizeof(components)));
    }

    void Material::ReconnectShader()
    {
        // A recompile can move, add, or drop MaterialParams members. The
        // values are held by name, so re-packing against the new layout is
        // all that is needed.
        materialUniformsDirty = true;
        materialParamsDirty   = true;
    }

    bool Material::CheckUniformExists(const std::string& name) const
    {
        const ShaderParamLayout* layout = GetParamLayout();
        return layout != nullptr && layout->FindParam(name) != nullptr;
    }

    const ShaderParamLayout* Material::GetParamLayout() const
    {
        return ShaderManager::Get()->GetParamLayout(shaderName);
    }

    void Material::SetParamValue(const std::string& name, ShaderParamType type, const void* data, int sizeBytes)
    {
        CC_ASSERT(sizeBytes <= static_cast<int>(sizeof(ParamValue::data)), "Material parameter is larger than the value store: " + name);

        unsigned char incoming[sizeof(ParamValue::data)] = {};
        memcpy(incoming, data, static_cast<size_t>(sizeBytes));

        int index = -1;
        for (int i = 0; i < paramValueCount && index == -1; i++)
        {
            if (paramValues[i].name == name)
            {
                index = i;
            }
        }

        if (index == -1)
        {
            CC_ASSERT(paramValueCount < MAX_PARAM_VALUES, "Material holds at most MAX_PARAM_VALUES parameters: " + name);
            if (paramValueCount < MAX_PARAM_VALUES)
            {
                index = paramValueCount;
                paramValues[index].name = name;
                paramValues[index].type = type;
                paramValueCount++;
                materialParamsDirty = true;
            }
        }

        if (index != -1)
        {
            if (paramValues[index].type != type || memcmp(paramValues[index].data, incoming, sizeof(incoming)) != 0)
            {
                memcpy(paramValues[index].data, incoming, sizeof(incoming));
                paramValues[index].type = type;
                materialParamsDirty = true;
            }

            // Uploaded on the spot rather than deferred to the next
            // SetStandardUniforms, because RenderManager's fade overlay sets
            // its colour after the material is bound for this frame's draw.
            // An unchanged value — the fullscreen quad re-sets its aspect
            // ratio every frame — costs the compare above and nothing more.
            if (materialParamsDirty)
            {
                PackAndUploadParams();
            }
        }
    }

    void Material::PackAndUploadParams()
    {
        const ShaderParamLayout* layout = GetParamLayout();
        if (layout != nullptr && layout->HasParams())
        {
            unsigned char blockData[ShaderParamLayout::MAX_BLOCK_SIZE_BYTES] = {};
            int blockSizeBytes = layout->GetBlockSizeBytes();

            for (int i = 0; i < paramValueCount; i++)
            {
                const ShaderParam* param = layout->FindParam(paramValues[i].name);
                if (param != nullptr)
                {
                    // A value set as one type against a block member of
                    // another would write the wrong bytes at the right
                    // offset, which reads as a corrupt value rather than a
                    // missing one. Leave the member at its default instead.
                    CC_ASSERT(param->type == paramValues[i].type, "MaterialParams type mismatch for: " + paramValues[i].name);
                    if (param->type == paramValues[i].type)
                    {
                        memcpy(blockData + param->offsetBytes, paramValues[i].data, static_cast<size_t>(param->sizeBytes));
                    }
                }
            }

            Gfx::RenderApi* gfxApi = Gfx::RenderApi::Get();

            if (materialParamsBuffer.IsValid() && materialParamsBufferSizeBytes != blockSizeBytes)
            {
                gfxApi->DestroyBuffer(materialParamsBuffer);
                materialParamsBuffer = Gfx::BufferHandle();
            }

            if (!materialParamsBuffer.IsValid())
            {
                Gfx::BufferDescription description;
                description.sizeBytes = blockSizeBytes;
                description.usage     = Gfx::BufferUsage::Uniform;
                description.memory    = Gfx::BufferMemory::CpuToGpu;
                description.debugName = "MaterialParams";
                materialParamsBuffer          = gfxApi->CreateBuffer(description);
                materialParamsBufferSizeBytes = blockSizeBytes;
            }

            gfxApi->UpdateBuffer(materialParamsBuffer, 0, blockSizeBytes, blockData);
        }

        materialParamsDirty = false;
    }
}