#shader vertex
#version 430 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 uv;

void main() {
    gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0); 
    uv = aTexCoord;
}

#shader fragment
#version 430 core
out vec4 FragColor;

in vec2 uv;
// Custom per-material parameters. Written by Material::SetUniform,
// packed by std140 offset from this declaration.
// Slot must match CUSTOM_PARAMS_BINDING_SLOT in Code/Core/Rendering/Uniforms/UniformBindings.h.
// Unnamed block: members stay in global scope and read as plain uniforms.
layout(std140, binding = 5) uniform CustomParams
{
    vec2  center;
    float aspectRatio;
    float scale;
};

#include "Include/gradients.glinc"

vec3 palette (float t) {

    vec3 a = vec3(0.5, 0.5, 0.5);
    vec3 b = vec3(0.5, 0.5, 0.5);
    vec3 c = vec3(1.0, 1.0, 1.0);
    vec3 d = vec3(0.02, 0.29, 0.45);
    
    return a + b*cos(6.28318*(c*t+d));
}

vec2 aspectAdjustUv(vec2 uv)
{
    float xUnit = uv.x * aspectRatio;
    float yUnit = uv.y;
    return vec2(xUnit, yUnit);
}

float sinNormalized(float value)
{
    return 1.0 - (cos(value) + 1.0) / 2.0;
}

void main()
{
    vec2 uvAdjust = aspectAdjustUv(uv - 0.5);

    float TwoPi = 6.2832;

    vec2 c = center + vec2((uvAdjust)*scale);
    vec2 z = vec2(0.0);
    float iter = 0.0;
    float maxIter = 255.0;
    for(iter = 0.0; iter < maxIter; iter++)
    {
        z = vec2(z.x * z.x - z.y * z.y, 2.0 * z.x * z.y) + c;
        if (length(z) > 2.0) 
        {
            break;
        }        
    }

    vec3 col = palette(iter/maxIter);
    
    FragColor = vec4(col , 1.0);
}




