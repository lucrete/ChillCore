#shader vertex
#version 430 core
#include "Include/materialUniforms.glinc"
#include "Include/objectUniforms.glinc"

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in vec3 aNormal;

out vec3 fragPos;
out vec3 normal;
out vec2 texCoord;

void main()
{
    vec2 textureTiling = material.textureTilingAndAlphaCutoff.xy;
    gl_Position = objectUniforms.mvp * vec4(aPos, 1.0);
    fragPos = vec3(objectUniforms.model * vec4(aPos, 1.0));
    normal = mat3(transpose(inverse(objectUniforms.model))) * aNormal;
    texCoord = aTexCoord * textureTiling; // Apply tiling to texture coordinates
}

#shader fragment
#version 430 core
#include "Include/frameUniforms.glinc"
#include "Include/materialUniforms.glinc"

out vec4 FragColor;

in vec3 fragPos;
in vec3 normal;
in vec2 texCoord;

const float shininess = 16.0;
const float specularStrength = 3.0;
layout(binding = 0) uniform sampler2D mainTex;
layout(binding = 4) uniform sampler2D emissiveTex;

void main()
{
    vec3  cameraPos             = frameUniforms.cameraPositionAndTime.xyz;
    vec3  ambientLightColor     = frameUniforms.ambientLightColorAndIntensity.xyz;
    float ambientLightIntensity = frameUniforms.ambientLightColorAndIntensity.w;
    vec3  lightDir              = frameUniforms.directionalLightDirAndIntensity.xyz;
    float lightIntensity        = frameUniforms.directionalLightDirAndIntensity.w;
    vec3  lightColor            = frameUniforms.directionalLightColor.xyz;
    vec3  baseColor             = material.baseColorAndOpacity.xyz;
    float opacity               = material.baseColorAndOpacity.w;
    vec3  emissiveFactor        = material.emissiveFactorPadded.xyz;
    int   hasEmissiveTex        = material.textureFlags.w;

    vec3 norm = normalize(normal);
    vec3 lightDirection = normalize(-lightDir);

    // Calculate ambient lighting
    vec3 ambient = ambientLightIntensity * ambientLightColor;

    // Calculate diffuse lighting
    float diff = max(dot(norm, lightDirection), 0.0);
    vec3 diffuse = diff * lightColor * lightIntensity;

    // Calculate specular lighting
    vec3 viewDir = normalize(cameraPos - fragPos);
    vec3 reflectDir = reflect(-lightDirection, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
    vec3 specular = specularStrength * spec * lightColor * lightIntensity;
    
    // Get color from texture and modulate with base color
    vec4 texColor = texture(mainTex, texCoord);
    vec3 objectColor = texColor.rgb * baseColor;
    
    // If texture has alpha < 0.1, discard the fragment
    if (texColor.a < 0.1)
    {
        discard;
    }
    
    // Emissive map modulated by the factor. A material without a map behaves
    // as though the map were white, so the factor alone drives the emission.
    vec3 emissive = emissiveFactor;
    if (hasEmissiveTex != 0)
    {
        emissive = texture(emissiveTex, texCoord).rgb * emissiveFactor;
    }

    // Combine all lighting components. Emissive is added after the lit
    // terms because it is light the surface produces, not light it reflects,
    // so it is unaffected by the scene's lighting.
    vec3 result = (ambient + diffuse + specular) * objectColor + emissive;

    // Scene-linear output. The post-process pass owns the tone curve and the
    // encode to display space; applying either here would do it twice.
    FragColor = vec4(result, texColor.a * opacity);
}