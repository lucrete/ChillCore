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
    float iTime;
    float aspectRatio;
};

vec3 palette (float t) {

    vec3 a = vec3(0.5, 0.5, 0.5);
    vec3 b = vec3(0.5, 0.5, 0.5);
    vec3 c = vec3(1.0, 1.0, 1.0);
    vec3 d = vec3(0.02, 0.29, 0.45);
    
    return a + b*cos(6.28318*(c*t+d));
}

vec2 aspectAdjustUv(vec2 uv)
{
    float xUnit = (-1.0 + (uv.x * 2.0)) * aspectRatio;
    float yUnit = -1.0 + (uv.y * 2.0);
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
    vec3 col = gradientTrig(percentRadius + radiusOffset + 0.27 * iTime, vec3(0.5, 0.5, 0.5), vec3(0.5, 0.5, 0.5), vec3(1.0, 1.0, 1.0), vec3(0.0, 0.10, 0.20));
    col *= smoothstep(0.97, 0.95, percentRadius);
    return vec4(col, 1.0 - t);
}

void main() {
    vec2 uvAdjust = aspectAdjustUv(uv);

    FragColor = circle(uvAdjust, vec2(0.0, 0.0), 0.5 + 0.3*sin(iTime), 0.0);
    
    
    //FragColor = vec4(palette(cos(50*uvAdjust.x *uvAdjust.y + 6*sin(iTime*2))), 1.0);
}
