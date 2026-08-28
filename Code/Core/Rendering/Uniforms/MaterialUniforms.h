#ifndef MATERIALUNIFORMS_H
#define MATERIALUNIFORMS_H

#include "UniformBindings.h"

namespace CC
{
    // Per-material uniform buffer contents. Bound at MATERIAL_UNIFORMS_BINDING_SLOT.
    //
    // std140 layout. Total size 96 bytes. Mixed scalar/vec3 fields packed into
    // vec4s with explicit padding. The two ivec4s carry alpha/debug modes and
    // PBR-texture-presence flags that the PBR shader branches on.
    //
    // Matches shader-side declaration in Include/materialUniforms.glinc.
    struct MaterialUniforms
    {
        float baseColorAndOpacity[4];         // xyz = baseColor,      w = opacity
        float textureTilingAndAlphaCutoff[4]; // xy  = textureTiling,  z = alphaCutoff, w = unused
        float pbrFactors[4];                  // x   = metallicFactor, y = roughnessFactor, zw = unused
        float emissiveFactor[4];              // xyz = emissiveFactor, w = unused
        int   alphaAndDebugMode[4];           // x   = alphaMode,      y = debugMode,   zw = unused
        int   textureFlags[4];                // x   = hasNormal,      y = hasMetallicRoughness, z = hasOcclusion, w = hasEmissive
    };
}

#endif // MATERIALUNIFORMS_H
