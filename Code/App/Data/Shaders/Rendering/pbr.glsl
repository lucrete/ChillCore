#shader vertex
#version 430 core
#include "Include/materialUniforms.glinc"
#include "Include/objectUniforms.glinc"

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in vec3 aNormal;
layout (location = 3) in vec4 aTangent;

out vec3 fragPos;
out vec3 normal;
out vec2 texCoord;
out mat3 TBN;

void main()
{
    vec2 textureTiling = material.textureTilingAndAlphaCutoff.xy;
    gl_Position = objectUniforms.mvp * vec4(aPos, 1.0);
    fragPos = vec3(objectUniforms.model * vec4(aPos, 1.0));
    texCoord = aTexCoord * textureTiling;

    // Calculate TBN matrix for normal mapping
    mat3 normalMatrix = mat3(transpose(inverse(objectUniforms.model)));
    vec3 N = normalize(normalMatrix * aNormal);
    vec3 T = normalize(normalMatrix * aTangent.xyz);
    T = normalize(T - dot(T, N) * N); // Re-orthogonalize
    vec3 B = cross(N, T) * aTangent.w; // Bitangent with handedness

    TBN = mat3(T, B, N);
    normal = N;
}

#shader fragment
#version 430 core
#include "Include/frameUniforms.glinc"
#include "Include/materialUniforms.glinc"

out vec4 FragColor;

in vec3 fragPos;
in vec3 normal;
in vec2 texCoord;
in mat3 TBN;

// PBR textures
layout(binding = 0) uniform sampler2D mainTex;              // Base color
layout(binding = 1) uniform sampler2D metallicRoughnessTex; // Metallic (B), Roughness (G)
layout(binding = 2) uniform sampler2D normalTex;            // Normal map
layout(binding = 3) uniform sampler2D occlusionTex;         // Ambient occlusion
layout(binding = 4) uniform sampler2D emissiveTex;          // Emissive

// UV flip for glTF compatibility (flips V coordinate in fragment shader).
const bool FLIP_UV_Y = true;

// Hardcoded debug mode for quick iteration via shader hotload.
// Set to -1 to use uniform, or 0-5 to override.
const int DEBUG_MODE_OVERRIDE = 0;

// Performance note on const vs uniform branching:
// - const values are evaluated at compile time. The compiler performs constant
//   propagation through local variables (e.g., activeDebugMode), so branches
//   using const-derived values have dead code completely eliminated.
// - uniform values are evaluated at draw time. All pixels in a draw call take
//   the same branch path, so the GPU can skip unused code (very efficient).
// - Per-pixel branches (e.g., if texCoord.x > 0.5) can cause warp divergence
//   where the GPU must execute both paths - avoid these in performance code.
//
// Example: When DEBUG_MODE_OVERRIDE = 3, the compiler simplifies
//   "activeDebugMode = DEBUG_MODE_OVERRIDE >= 0 ? DEBUG_MODE_OVERRIDE : debugMode"
// to "activeDebugMode = 3", making all if(activeDebugMode == X) checks become
// compile-time constants. Only mode 3 code remains in the compiled shader.

const float PI = 3.14159265359;

// ========================
// PBR Functions
// ========================

// Fresnel-Schlick approximation
vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// GGX/Trowbridge-Reitz normal distribution function
float distributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / denom;
}

// Smith's Schlick-GGX geometry function
float geometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return num / denom;
}

// Smith's geometry function
float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = geometrySchlickGGX(NdotV, roughness);
    float ggx1 = geometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

void main()
{
    vec3  cameraPos             = frameUniforms.cameraPositionAndTime.xyz;
    vec3  ambientLightColor     = frameUniforms.ambientLightColorAndIntensity.xyz;
    float ambientLightIntensity = frameUniforms.ambientLightColorAndIntensity.w;
    vec3  lightDir              = frameUniforms.directionalLightDirAndIntensity.xyz;
    float lightIntensity        = frameUniforms.directionalLightDirAndIntensity.w;
    vec3  lightColor            = frameUniforms.directionalLightColor.xyz;

    vec3  baseColor                  = material.baseColorAndOpacity.xyz;
    float opacity                    = material.baseColorAndOpacity.w;
    float alphaCutoff                = material.textureTilingAndAlphaCutoff.z;
    float metallicFactor             = material.pbrFactors.x;
    float roughnessFactor            = material.pbrFactors.y;
    vec3  emissiveFactor             = material.emissiveFactorPadded.xyz;
    int   alphaMode                  = material.alphaAndDebugMode.x;
    int   debugMode                  = material.alphaAndDebugMode.y;
    int   hasNormalTex               = material.textureFlags.x;
    int   hasMetallicRoughnessTex    = material.textureFlags.y;
    int   hasOcclusionTex            = material.textureFlags.z;
    int   hasEmissiveTex             = material.textureFlags.w;

    // Apply UV flip if enabled (for glTF compatibility)
    vec2 uv = FLIP_UV_Y ? vec2(texCoord.x, 1.0 - texCoord.y) : texCoord;

    // Debug visualization modes
    // Use hardcoded override if set, otherwise use uniform
    int activeDebugMode = DEBUG_MODE_OVERRIDE >= 0 ? DEBUG_MODE_OVERRIDE : debugMode;

    if (activeDebugMode == 1)
    {
        // UV coordinates as RG colors (show flipped UVs)
        FragColor = vec4(uv, 0.0, 1.0);
        return;
    }
    if (activeDebugMode == 2)
    {
        // Vertex normals as RGB (before normal map)
        FragColor = vec4(normalize(normal) * 0.5 + 0.5, 1.0);
        return;
    }
    if (activeDebugMode == 3)
    {
        // Base color texture only
        FragColor = texture(mainTex, uv);
        return;
    }
    if (activeDebugMode == 4)
    {
        // Raw metallic-roughness texture
        FragColor = texture(metallicRoughnessTex, uv);
        return;
    }
    if (activeDebugMode == 5)
    {
        // Raw normal map texture
        FragColor = texture(normalTex, uv);
        return;
    }

    // Sample textures
    vec4 albedoSample = texture(mainTex, uv);
    vec3 albedo = albedoSample.rgb * baseColor;

    // Metallic-roughness: G = roughness, B = metallic (glTF spec)
    float metallic = metallicFactor;
    float roughness = roughnessFactor;
    if (hasMetallicRoughnessTex != 0)
    {
        vec4 mrSample = texture(metallicRoughnessTex, uv);
        metallic = mrSample.b * metallicFactor;
        roughness = mrSample.g * roughnessFactor;
    }
    roughness = clamp(roughness, 0.04, 1.0); // Prevent division issues

    // Normal mapping
    vec3 N;
    if (hasNormalTex != 0)
    {
        vec3 normalMapSample = texture(normalTex, uv).rgb;
        normalMapSample = normalMapSample * 2.0 - 1.0;
        N = normalize(TBN * normalMapSample);
    }
    else
    {
        N = normalize(normal);
    }

    // Ambient occlusion
    float ao = 1.0;
    if (hasOcclusionTex != 0)
    {
        ao = texture(occlusionTex, uv).r;
    }

    // Emissive
    vec3 emissive = emissiveFactor;
    if (hasEmissiveTex != 0)
    {
        emissive = texture(emissiveTex, uv).rgb * emissiveFactor;
    }

    // Calculate lighting vectors
    vec3 V = normalize(cameraPos - fragPos);
    vec3 L = normalize(-lightDir);
    vec3 H = normalize(V + L);

    // Calculate reflectance at normal incidence
    // Dielectrics use 0.04, metals use albedo color
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    // Cook-Torrance BRDF
    float NDF = distributionGGX(N, H, roughness);
    float G = geometrySmith(N, V, L, roughness);
    vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

    // Specular reflection
    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3 specular = numerator / denominator;

    // Energy conservation: diffuse + specular <= 1
    vec3 kS = F; // Specular reflection coefficient
    vec3 kD = vec3(1.0) - kS; // Diffuse coefficient
    kD *= 1.0 - metallic; // Metals have no diffuse

    // Lambertian diffuse
    float NdotL = max(dot(N, L), 0.0);
    vec3 diffuse = kD * albedo / PI;

    // Direct lighting contribution
    vec3 Lo = (diffuse + specular) * lightColor * lightIntensity * NdotL;

    // Ambient lighting (simplified IBL approximation)
    vec3 ambient = ambientLightColor * ambientLightIntensity * albedo * ao;

    // Final color
    vec3 color = ambient + Lo + emissive;

    // Tone mapping (Reinhard)
    color = color / (color + vec3(1.0));

    // Gamma correction
    color = pow(color, vec3(1.0 / 2.2));

    // Alpha handling based on material alpha mode
    float alpha = 1.0;
    if (alphaMode == 1) // MASK
    {
        if (albedoSample.a < alphaCutoff)
        {
            discard;
        }
    }
    else if (alphaMode == 2) // BLEND
    {
        alpha = albedoSample.a * opacity;
    }

    FragColor = vec4(color, alpha);
}
