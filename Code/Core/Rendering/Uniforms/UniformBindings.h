#ifndef UNIFORMBINDINGS_H
#define UNIFORMBINDINGS_H

namespace CC
{
    // ========================
    // Uniform Buffer Binding Slots
    // ========================
    //
    // Single source of truth for every UBO binding slot the engine uses.
    // Each tier owns one slot; the shader-side glinc files hardcode the
    // same integer literal in their layout(std140, binding = N) blocks
    // and carry a comment pointing back here.
    //
    // To add a new slot: declare it here, add the corresponding header
    // and glinc, and use the next free integer.

    static constexpr int FRAME_UNIFORMS_BINDING_SLOT    = 0;  // Per-frame    — viewProj, camera, lights
    static constexpr int MATERIAL_UNIFORMS_BINDING_SLOT = 1;  // Per-material — base colour, PBR factors, alpha mode
    static constexpr int OBJECT_UNIFORMS_BINDING_SLOT   = 2;  // Per-draw     — mvp, model (push tier)
    static constexpr int UI_UNIFORMS_BINDING_SLOT       = 3;  // UI quads     — orthographic projection
    static constexpr int TEXT_UNIFORMS_BINDING_SLOT     = 4;  // MSDF text    — projection + per-batch text colour / pixel range
    static constexpr int MATERIAL_PARAMS_BINDING_SLOT   = 5;  // Per-material — shader-declared custom parameters (see ShaderParamLayout)
}

#endif // UNIFORMBINDINGS_H
