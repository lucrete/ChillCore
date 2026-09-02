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
// Scalars first so the offsets need no reasoning about vec3 padding.
layout(std140, binding = 5) uniform CustomParams
{
    float aspectRatio;
    float patternScale;
    float pulseSpeed;
    float veinSharpness;
    vec3  glowColour;
};

const int OCTAVES = 5;

// Every surface this lands on samples the same field, so the pattern is a
// function of the domain alone. Nothing here knows whether the domain came
// from a screen or from a mesh's UVs.

float hash(vec2 p)
{
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

float valueNoise(vec2 p)
{
    vec2 cell = floor(p);
    vec2 f = fract(p);

    // Smoothstep the cell fraction so the lattice does not show as creases.
    vec2 blend = f * f * (3.0 - 2.0 * f);

    float a = hash(cell);
    float b = hash(cell + vec2(1.0, 0.0));
    float c = hash(cell + vec2(0.0, 1.0));
    float d = hash(cell + vec2(1.0, 1.0));

    return mix(mix(a, b, blend.x), mix(c, d, blend.x), blend.y);
}

float fbm(vec2 p)
{
    float total = 0.0;
    float amplitude = 1.0;
    float weight = 0.0;

    for (int i = 0; i < OCTAVES; i++)
    {
        total += amplitude * valueNoise(p);
        weight += amplitude;
        amplitude *= 0.5;
        p *= 2.0;
    }

    return total / weight;
}

void main()
{
    float time = frameUniforms.cameraPositionAndTime.w;

    // Aspect correction belongs to the domain, not the pattern. The
    // fullscreen quad sets a real ratio; an in-world surface leaves it at 1
    // so the pattern follows the UVs rather than the window.
    vec2 domain = vec2((uv.x - 0.5) * aspectRatio, uv.y - 0.5) * patternScale;

    // Drifting the sample point animates the field without re-deriving it.
    float field = fbm(domain + vec2(0.0, time * pulseSpeed * 0.1));

    // The zero crossings become thin bright lines; sharpness sets how thin.
    float vein = pow(max(0.0, 1.0 - abs(2.0 * field - 1.0)), veinSharpness);

    // A slow breath over the whole field, so the veins are never fully static.
    float pulse = 0.75 + 0.25 * sin(time * pulseSpeed);

    // Scene-linear, deliberately above white at the vein cores so bloom has
    // something to find and the tone curve something to roll off.
    vec3 colour = glowColour * vein * pulse;

    FragColor = vec4(colour, 1.0);
}
