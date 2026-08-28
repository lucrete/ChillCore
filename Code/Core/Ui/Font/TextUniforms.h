#ifndef TEXTUNIFORMS_H
#define TEXTUNIFORMS_H

#include "UniformBindings.h"

namespace CC
{
    // MSDF text uniform buffer contents. Bound at TEXT_UNIFORMS_BINDING_SLOT.
    //
    // std140 layout. Two cadences sharing one buffer:
    //   - projection: per-frame   (offset  0, 64 bytes)
    //   - textColor:  per-batch   (offset 64, 16 bytes)
    //   - pixelRange: per-batch   (offset 80,  4 bytes; padded to 16)
    //
    // Total size 96 bytes. Per-batch updates only touch the trailing 32
    // bytes via UpdateBuffer offset; projection is uploaded once per
    // BeginFrame.
    //
    // Matches shader-side declaration in Include/textUniforms.glinc.
    struct TextUniforms
    {
        float projection[16];   // offset  0
        float textColor[4];     // offset 64
        float pixelRange;       // offset 80
        float _pad[3];          // pad to 16-byte alignment / 96-byte size
    };

    static constexpr int TEXT_UNIFORMS_SIZE_BYTES        = 96;
    static constexpr int TEXT_UNIFORMS_PROJECTION_OFFSET = 0;
    static constexpr int TEXT_UNIFORMS_PROJECTION_SIZE   = 64;
    static constexpr int TEXT_UNIFORMS_BATCH_OFFSET      = 64;
    static constexpr int TEXT_UNIFORMS_BATCH_SIZE        = 32;
}

#endif // TEXTUNIFORMS_H
