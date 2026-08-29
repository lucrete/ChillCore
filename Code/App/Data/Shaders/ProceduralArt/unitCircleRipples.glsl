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
#include "Include/frameUniforms.glinc"

out vec4 FragColor;

in vec2 uv;
// Custom per-material parameters. Written by Material::SetUniform,
// packed by std140 offset from this declaration.
// Slot must match MATERIAL_PARAMS_BINDING_SLOT in Code/Core/Rendering/Uniforms/UniformBindings.h.
// Unnamed block: members stay in global scope and read as plain uniforms.
layout(std140, binding = 5) uniform MaterialParams
{
    float aspectRatio;
};

vec3 palette (float t) {

    vec3 a = vec3(0.5, 0.5, 0.5);
    vec3 b = vec3(0.5, 0.5, 0.5);
    vec3 c = vec3(1.0, 1.0, 1.0);
    vec3 d = vec3(0.02, 0.29, 0.45);
    
    return a + b*cos(6.28318*(c*t+d));
}

vec2 aspectAdjustUvCentered(vec2 uv)
{
    float xUnit = (-1.0 + (uv.x * 2.0)) * aspectRatio;
    float yUnit = -1.0 + (uv.y * 2.0);
    return vec2(xUnit, yUnit);
}

vec2 aspectAdjustUv(vec2 uv)
{
    float xUnit = uv.x * aspectRatio;
    float yUnit = uv.y;
    return vec2(xUnit, yUnit);
}

vec3 gradientTrig(float t, vec3 a, vec3 b, vec3 c, vec3 d)
{
    return a + b * cos(6.28318 * (c * t + d));
}

vec4 circle(vec2 uv, vec2 pos, float radius, float radiusOffset) 
{
    float distanceToCenter = length(pos - uv);
    float d = distanceToCenter - radius;
    float t = clamp(d/0.005, 0.0, 1.0);

    float percentRadius = clamp(distanceToCenter / radius, 0.0, 1.0);
    vec3 col = palette(percentRadius + radiusOffset);
    col *= smoothstep(0.97, 0.95, percentRadius);
    return vec4(col, 1.0 - t);
}

float ring(vec2 uv, vec2 pos, float radius, float radiusWidth, float paletteOffset) 
{
    float distanceToCenter = length(pos - uv);
    
    float percentRadius = clamp((distanceToCenter - radius) / radiusWidth, 0.0, 1.0);
    percentRadius *= smoothstep(1.0, 0.96, percentRadius);
    percentRadius *= smoothstep(0.00, 0.02, percentRadius);
    return percentRadius ;
}

float percentRange(float value, float min, float max)
{
    return clamp((value - min) / (max - min), 0.0, 1.0);
}

float rings02(vec2 uv, vec2 pos)
{
    float distanceToCenter = length(pos - uv);
    float level = (1.0 + sin(distanceToCenter*10.0))/2.0;
    //float level = sin(distanceToCenter*10);
    //level = percentRange(level, 0.8, 1);
    //level = (1 + sin(2*level*3.14))/2.0;
    return level ;
}

float sinNormalized(float value)
{
    return 1.0 - (cos(value) + 1.0) / 2.0;
}

void main()
{
    vec2 uvAdjust = aspectAdjustUvCentered(uv);
    float timeAbsolute = frameUniforms.cameraPositionAndTime.w;
    FragColor = circle(uvAdjust, vec2(0.0, 0.0), 0.5 + 0.45*sin(.5*timeAbsolute), timeAbsolute*0.1 + -.3*cos(timeAbsolute));
}

