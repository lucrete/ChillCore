#ifndef UIUNIFORMS_H
#define UIUNIFORMS_H

#include "UniformBindings.h"

namespace CC
{
    // UI quad uniform buffer contents. Bound at UI_UNIFORMS_BINDING_SLOT.
    //
    // std140 layout. Total size 64 bytes — just the orthographic
    // screen-space projection.
    //
    // Matches shader-side declaration in Include/uiUniforms.glinc.
    struct UiUniforms
    {
        float projection[16];
    };

    static constexpr int UI_UNIFORMS_SIZE_BYTES = 64;
}

#endif // UIUNIFORMS_H
