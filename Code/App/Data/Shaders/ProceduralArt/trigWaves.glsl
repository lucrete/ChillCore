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
// Slot must match CUSTOM_PARAMS_BINDING_SLOT in Code/Core/Rendering/Uniforms/UniformBindings.h.
// Unnamed block: members stay in global scope and read as plain uniforms.
layout(std140, binding = 5) uniform CustomParams
{
    float aspectRatio;
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
    float timeAbsolute = frameUniforms.cameraPositionAndTime.w;
    vec2 uvAdjust = aspectAdjustUv(uv);

    float TwoPi = 6.2832;

    // High-level pattern
    float level1  = sinNormalized(uvAdjust.y * 7.0 - timeAbsolute) * 0.5 +
           sinNormalized(uvAdjust.y * 13.0 * 0.5 + timeAbsolute * 0.7) * 0.3 +
           sinNormalized(uvAdjust.y * 21.1 * 0.7 - timeAbsolute * 0.5) * 0.2;

     float level2  = sinNormalized(uvAdjust.x * 7.0 + timeAbsolute) * 0.5 +
           sinNormalized(uvAdjust.x * 13.0 * 0.5 - timeAbsolute * 0.7) * 0.3 +
           sinNormalized(uvAdjust.x * 21.1 * 0.7 + timeAbsolute * 0.5) * 0.2;

     float level3  = sinNormalized(uvAdjust.x * 3.0 - timeAbsolute*1.9) * 0.5 +
           sinNormalized(uvAdjust.x * 7.0 * 0.5 + timeAbsolute * 0.9) * 0.3 +
           sinNormalized(uvAdjust.x * 8.3 * 0.7 - timeAbsolute * 0.7) * 0.2;

    float level01 = sinNormalized(TwoPi*(level1+(level2 + level3)/2.0));

    // Mid-Detail Pattern
    float timeScale02 = 3.0;
    float harmonic1 = sinNormalized(uv.x*2.1*TwoPi + timeAbsolute*timeScale02*0.37)*0.55 +
                      sinNormalized(uv.x*3.9*TwoPi - timeAbsolute*timeScale02*0.27)*0.55 +
                      sinNormalized(uv.x*5.9*TwoPi - timeAbsolute*timeScale02*0.7)*0.15 +
                      sinNormalized(uv.x*7.9*TwoPi + timeAbsolute*timeScale02*0.6)*0.15 +
                      sinNormalized(uv.x*15.9*TwoPi - timeAbsolute*timeScale02*1.7)*0.1 +
                      sinNormalized(uv.x*17.9*TwoPi + timeAbsolute*timeScale02*2.6)*0.1;
    
    float harmonic2 = sinNormalized(uv.y*3.1*TwoPi - timeAbsolute*timeScale02*0.8)*0.7 +
                      sinNormalized(uv.y*5.9*TwoPi + timeAbsolute*timeScale02*0.3)*0.2 +
                      sinNormalized(uv.y*7.9*TwoPi - timeAbsolute*timeScale02*0.1)*0.1;
    
    float level02  = sinNormalized((uv.y + 0.1*harmonic1 + 0.05*harmonic2)*4.0*TwoPi);

    // Combine levels    
    float transitionStart = 0.75;
    float transitionRange = 0.2;
    float leveltransition = smoothstep(transitionStart, transitionStart + transitionRange, level01);
    
    float blendVariation = sinNormalized(timeAbsolute*0.37)*0.55 +
                      sinNormalized(timeAbsolute*0.7)*0.3 +
                      sinNormalized(timeAbsolute*2.6)*0.2;

    float level02Blend = 0.4*blendVariation;
    level02 = mix(level01, level02, level02Blend);
    
    float level = mix(level01, level02, leveltransition);

    ///////////////////
    // Greyscale
    //vec3 col = vec3(level);

    ///////////////////
    // Trig palette
    //vec3 col = palette(level +  timeAbsolute*0.05);
    
    ///////////////////
    // Gradient Interpolation
    // Calculate two adjacent gradient indices
    float scaledTime = timeAbsolute * 0.05;
    int gradientIndex1 = int(floor(scaledTime)) % GRADIENT_COUNT;
    int gradientIndex2 = (gradientIndex1 + 1) % GRADIENT_COUNT;

    // Get blend factor between the two gradients (0.0 to 1.0)
    float blendFactor = fract(scaledTime);

    // sine wave for even smoother transitions:
     float smoothBlend = (sin(blendFactor * 3.14159 - 1.57079) + 1.0) * 0.5;

    // Get colors from both gradients at the same position
    vec3 color1 = gradient(gradientIndex1, level);
    vec3 color2 = gradient(gradientIndex2, level);

    // Blend between the two gradient colors
    vec3 col = mix(color1, color2, smoothBlend);
    
    FragColor = vec4(col , 1.0);
}




