#ifndef OBJECTUNIFORMS_H
#define OBJECTUNIFORMS_H

#include "UniformBindings.h"

namespace CC
{
    // Per-draw uniform buffer contents. Bound at OBJECT_UNIFORMS_BINDING_SLOT.
    //
    // The "push tier" — values that change with every draw call. Sized at
    // 128 bytes so future Vulkan / D3D12 backends can map this to native
    // push constants (the common minimum guarantee) without restructuring
    // the shader interface.
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
