#shader vertex
#version 430 core
#include "Include/objectUniforms.glinc"

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 uv;

void main() 
{
    // Identity on the fullscreen quad, whose vertices are already in clip
    // space; a real transform when the same shader is used in-world.
    gl_Position = objectUniforms.mvp * vec4(aPos, 1.0);
    uv = aTexCoord;
}

#shader fragment
#version 430 core
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
    FragColor = vec4(color, 1.0);
}