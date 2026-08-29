#shader vertex
#version 430 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoords;

out vec2 TexCoords;

void main()
{
    gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);
    TexCoords = aTexCoords;
}

#shader fragment
#version 430 core
out vec4 FragColor;

in vec2 TexCoords;

layout(binding = 0) uniform sampler2D sceneTexture;

// Custom per-material parameters, packed by std140 offset from this
// declaration. Slot must match CUSTOM_PARAMS_BINDING_SLOT in
// Code/Core/Rendering/Uniforms/UniformBindings.h.
layout(std140, binding = 5) uniform CustomParams
{
    float effectAmount;
};

void main()
{
    vec3 sceneColor = texture(sceneTexture, TexCoords).rgb;
    vec3 inverted   = vec3(1.0) - sceneColor;
    // effectAmount 0 returns the scene unmodified, so toggling the effect
    // off is the manual regression check against the direct render path.
    FragColor = vec4(mix(sceneColor, inverted, clamp(effectAmount, 0.0, 1.0)), 1.0);
}
