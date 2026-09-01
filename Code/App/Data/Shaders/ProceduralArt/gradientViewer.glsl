#shader vertex
#version 430 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 uv;

void main() 
{
    gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0); 
    uv = aTexCoord;
}

#shader fragment
#version 430 core
#include "Include/colorSpace.glinc"
#include "Include/frameUniforms.glinc"

out vec4 FragColor;

in vec2 uv;
// Custom per-material parameters. Written by Material::SetUniform,
// packed by std140 offset from this declaration.
// Slot must match CUSTOM_PARAMS_BINDING_SLOT in Code/Core/Rendering/Uniforms/UniformBindings.h.
// Unnamed block: members stay in global scope and read as plain uniforms.
layout(std140, binding = 5) uniform CustomParams
{
    float aspectRatio;
};

#include "Include/gradients.glinc"

vec2 aspectAdjustUv(vec2 uv)
{
    float xUnit = uv.x * aspectRatio;
    float yUnit = uv.y;
    return vec2(xUnit, yUnit);
}

void main()
{
    float timeAbsolute = frameUniforms.cameraPositionAndTime.w;
    vec2 uvAdjust = aspectAdjustUv(uv);

    // Select different gradients based on y position
    float gradientSelection = floor(uv.y * float(GRADIENT_COUNT));
    int gradientIndex = int(min(gradientSelection, float(GRADIENT_COUNT) - 1.0));

    // x position as gradient level with subtle animation
    float animatedLevel = uvAdjust.x + timeAbsolute * 0.1;
    
    vec3 color = gradient(gradientIndex, animatedLevel);
    // Authored in display space, but the scene target holds scene-linear
    // light. Converting on output keeps the picked colours looking as
    // they were chosen once the post-process pass encodes back to sRGB.
    FragColor = vec4(SrgbToLinear(color), 1.0);
}