#ifndef UNIFORMBINDINGS_H
#define UNIFORMBINDINGS_H

namespace CC
{
    // ========================
    // Update Frequency
    // ========================
    //
    // Shader inputs are grouped by how often they change. Each frequency
    // owns one uniform buffer and one binding slot; the shader-side glinc
    // files hardcode the same integer literal in their
    // layout(std140, binding = N) blocks and carry a comment pointing here.
    //
    // This is the partition Vulkan descriptor sets and D3D12 root signatures
    // are meant to be organised along, so the grouping carries forward to
    // those backends rather than needing to be redone.
    //
    // Baked state — cull mode, depth compare, blend — is deliberately not
    // here. It is never sent: it lives in the pipeline object.

    enum class UpdateFrequency
    {
        Frame = 0,  // Once per frame  — viewProj, camera, time, lights
        Material,   // On value change — base colour, PBR factors, alpha mode
        Custom,     // On value change — shader-declared CustomParams block
        Draw,       // Every draw      — mvp, model (sized for push constants)
        Ui,         // Once per frame  — orthographic projection
        Text,       // Mixed, see note — projection plus per-batch colour
        Max
    };

    // Text is the one buffer holding two frequencies: its projection is
    // written once per frame, its colour and pixel range per batch through
    // an offset update. The enumerator names the buffer, not every field.

    // ========================
    // Uniform Buffer Binding Slots
    // ========================
    //
    // To add a frequency: add an enumerator above, its slot here, its entry
    // in UNIFORM_BINDING_SLOTS, and the corresponding header and glinc.

    static constexpr int FRAME_UNIFORMS_BINDING_SLOT    = 0;  // UpdateFrequency::Frame
    static constexpr int MATERIAL_UNIFORMS_BINDING_SLOT = 1;  // UpdateFrequency::Material
    static constexpr int OBJECT_UNIFORMS_BINDING_SLOT   = 2;  // UpdateFrequency::Draw
    static constexpr int UI_UNIFORMS_BINDING_SLOT       = 3;  // UpdateFrequency::Ui
    static constexpr int TEXT_UNIFORMS_BINDING_SLOT     = 4;  // UpdateFrequency::Text
    static constexpr int CUSTOM_PARAMS_BINDING_SLOT     = 5;  // UpdateFrequency::Custom

    // Indexed by UpdateFrequency. A Vulkan or D3D12 backend maps this to a
    // descriptor set index or root parameter slot without restructuring the
    // shader interface.
    static constexpr int UNIFORM_BINDING_SLOTS[static_cast<int>(UpdateFrequency::Max)] =
    {
        FRAME_UNIFORMS_BINDING_SLOT,
        MATERIAL_UNIFORMS_BINDING_SLOT,
        CUSTOM_PARAMS_BINDING_SLOT,
        OBJECT_UNIFORMS_BINDING_SLOT,
        UI_UNIFORMS_BINDING_SLOT,
        TEXT_UNIFORMS_BINDING_SLOT
    };
}

#endif // UNIFORMBINDINGS_H