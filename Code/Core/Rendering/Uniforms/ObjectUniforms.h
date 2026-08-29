#ifndef OBJECTUNIFORMS_H
#define OBJECTUNIFORMS_H

#include "UniformBindings.h"

namespace CC
{
    // Per-draw uniform buffer contents. Bound at OBJECT_UNIFORMS_BINDING_SLOT.
    //
    // UpdateFrequency::Draw — values that change with every draw call.
    // Sized at 128 bytes, the guaranteed minimum maxPushConstantsSize in
    // Vulkan 1.0-1.3 (1.4 raised it to 256), so a Vulkan backend can map
    // this to native push constants without restructuring the shader
    // interface. D3D12's analogue is root constants, drawn from the same
    // 256-byte root signature budget as everything else in the signature.
    //
    // Nothing else belongs here: a third mat4 exceeds the floor this is
    // sized against and forfeits that mapping.
    //
    // std140 layout. Matches shader-side declaration in
    // Include/objectUniforms.glinc.
    struct ObjectUniforms
    {
        float mvp[16];
        float model[16];
    };

    static constexpr int OBJECT_UNIFORMS_SIZE_BYTES = 128;
}

#endif // OBJECTUNIFORMS_H
