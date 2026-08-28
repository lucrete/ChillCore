#ifndef FRAMEUNIFORMS_H
#define FRAMEUNIFORMS_H

#include "UniformBindings.h"

namespace CC
{
    // Per-frame uniform buffer contents. Bound at FRAME_UNIFORMS_BINDING_SLOT.
    //
    // std140 layout. Scalars are packed into the unused w component of
    // surrounding vec4s to avoid padding holes. Total size 128 bytes.
    //
    // Matches shader-side declaration in Include/frameUniforms.glinc.
    struct FrameUniforms
    {
        float viewProj[16];
        float cameraPositionAndTime[4];
        float ambientLightColorAndIntensity[4];
        float directionalLightDirAndIntensity[4];
        float directionalLightColor[4];
    };
}

#endif // FRAMEUNIFORMS_H
