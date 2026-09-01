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
#include "Include/colorSpace.glinc"

out vec4 FragColor;

in vec2 TexCoords;

layout(binding = 0) uniform sampler2D sceneTexture;
layout(binding = 1) uniform sampler2D bloomTexture;

// Custom per-material parameters, packed by std140 offset from this
// declaration. Slot must match CUSTOM_PARAMS_BINDING_SLOT in
// Code/Core/Rendering/Uniforms/UniformBindings.h.
//
// Packed into vec4s because std140 gives a lone float a full 16-byte slot
// and the block has a fixed capacity.
layout(std140, binding = 5) uniform CustomParams
{
    vec4 bloomAndExposure;  // x: bloom on, y: bloom intensity, z: exposure, w: tonemap on
    vec4 vignetteParams;    // x: intensity (0 disables), y: smoothness, z: roundness
    vec4 gradeParamsA;      // x: grade on, y: contrast, z: saturation, w: temperature
    vec4 gradeParamsB;      // x: tint, y: blend mode
    vec4 gradeLift;         // xyz: per-channel lift, moves the shadows
    vec4 gradeGamma;        // xyz: per-channel gamma, moves the mid-tones
    vec4 gradeGain;         // xyz: per-channel gain, moves the highlights
    vec4 gradeBlend;        // xyz: blend colour, w: blend strength
};

// ========================
// Blend modes
// ========================
//
// The photographic filter looks are built by compositing a flat colour over
// the image, not by curves alone, so the grade needs the same tool.
//
// These run in log space, alongside contrast, for two reasons: the operations
// are defined on a bounded 0..1 signal and log is where the scene is bounded,
// and the log excursion already exists so no extra transfer function is paid.
// The consequence is that results are not identical to the same mode in an
// image editor working on display-referred pixels — looks are authored against
// this implementation rather than ported by their numbers.

const int BLEND_MULTIPLY  = 0;
const int BLEND_SCREEN    = 1;
const int BLEND_OVERLAY   = 2;
const int BLEND_SOFT_LIGHT = 3;

vec3 ApplyBlend(vec3 base, vec3 blend, int mode)
{
    vec3 result = base * blend;

    if (mode == BLEND_SCREEN)
    {
        result = vec3(1.0) - (vec3(1.0) - base) * (vec3(1.0) - blend);
    }
    else if (mode == BLEND_OVERLAY)
    {
        result = mix(2.0 * base * blend,
                     vec3(1.0) - 2.0 * (vec3(1.0) - base) * (vec3(1.0) - blend),
                     step(vec3(0.5), base));
    }
    else if (mode == BLEND_SOFT_LIGHT)
    {
        result = mix(2.0 * base * blend + base * base * (vec3(1.0) - 2.0 * blend),
                     sqrt(max(base, vec3(0.0))) * (2.0 * blend - vec3(1.0)) + 2.0 * base * (vec3(1.0) - blend),
                     step(vec3(0.5), blend));
    }

    return result;
}

// ========================
// Arri LogC v3
// ========================
//
// Grading runs in log space because that is what makes a preset portable:
// a contrast or lift value lands the same way regardless of the exposure the
// shot is viewed at, which is not true of the same operation in linear.

const float LOGC_CUT = 0.011361;
const float LOGC_A   = 5.555556;
const float LOGC_B   = 0.047996;
const float LOGC_C   = 0.244161;
const float LOGC_D   = 0.386036;
const float LOGC_E   = 5.301883;
const float LOGC_F   = 0.092819;

// Mid grey in LogC. Contrast pivots here so it darkens shadows and lifts
// highlights rather than dimming the whole frame.
const float LOGC_MID_GREY = 0.4135884;

float LinearToLogC(float value)
{
    return value > LOGC_CUT
        ? LOGC_C * (log(LOGC_A * value + LOGC_B) / log(10.0)) + LOGC_D
        : LOGC_E * value + LOGC_F;
}

float LogCToLinear(float value)
{
    return value > LOGC_E * LOGC_CUT + LOGC_F
        ? (pow(10.0, (value - LOGC_D) / LOGC_C) - LOGC_B) / LOGC_A
        : (value - LOGC_F) / LOGC_E;
}

vec3 LinearToLogC(vec3 color)
{
    return vec3(LinearToLogC(color.r), LinearToLogC(color.g), LinearToLogC(color.b));
}

vec3 LogCToLinear(vec3 color)
{
    return vec3(LogCToLinear(color.r), LogCToLinear(color.g), LogCToLinear(color.b));
}

// ========================
// ACES
// ========================
//
// Narkowicz's curve fit to the ACES RRT and ODT combined. Cheap enough for a
// mobile fragment shader and visually close to the reference over the range
// a real-time scene produces.

vec3 AcesTonemap(vec3 color)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
}

// Approximate white balance. Temperature runs cool to warm, tint green to
// magenta; both are scaled well below a full colour-temperature conversion so
// the slider range stays usable rather than immediately clipping a channel.
vec3 ApplyWhiteBalance(vec3 color, float temperature, float tint)
{
    vec3 balance = vec3(1.0 + temperature * 0.25,
                        1.0 + tint * 0.15,
                        1.0 - temperature * 0.25);
    return color * balance;
}

void main()
{
    vec3 color = texture(sceneTexture, TexCoords).rgb;

    // ---- Bloom composite, in linear, before anything compresses range.
    if (bloomAndExposure.x > 0.5)
    {
        color += texture(bloomTexture, TexCoords).rgb * bloomAndExposure.y;
    }

    // ---- Exposure.
    color *= bloomAndExposure.z;

    // ---- Vignette. A lens effect on incoming light, so it multiplies linear
    // colour before the tone curve rather than darkening a finished image.
    if (vignetteParams.x > 0.0)
    {
        vec2 offset = TexCoords - vec2(0.5);
        // Roundness 1 gives a circular falloff; 0 follows the frame's aspect.
        offset.x *= mix(1.0, 1.7778, vignetteParams.z);
        float distance = length(offset);
        float vignette = smoothstep(0.8, 0.8 - vignetteParams.y, distance);
        color *= mix(1.0, vignette, vignetteParams.x);
    }

    // ---- Colour grade, in log space, before the tone curve.
    if (gradeParamsA.x > 0.5)
    {
        float contrast    = gradeParamsA.y;
        float saturation  = gradeParamsA.z;
        float temperature = gradeParamsA.w;
        float tint        = gradeParamsB.x;
        int   blendMode   = int(gradeParamsB.y + 0.5);
        vec3  blendColor  = gradeBlend.xyz;
        float blendAmount = gradeBlend.w;
        vec3  lift        = gradeLift.xyz;
        vec3  gammaValue  = gradeGamma.xyz;
        vec3  gain        = gradeGain.xyz;

        color = ApplyWhiteBalance(color, temperature, tint);

        vec3 logColor = LinearToLogC(max(color, vec3(0.0)));
        logColor = (logColor - LOGC_MID_GREY) * contrast + LOGC_MID_GREY;

        if (blendAmount > 0.0)
        {
            logColor = mix(logColor, ApplyBlend(logColor, blendColor, blendMode), blendAmount);
        }

        color = LogCToLinear(logColor);

        color = color * gain + lift;
        color = pow(max(color, vec3(0.0)), vec3(1.0) / max(gammaValue, vec3(0.001)));

        float luminance = Luminance(color);
        color = mix(vec3(luminance), color, saturation);
    }

    // ---- Output transform. With the tone map off the frame is clamped
    // instead, which hard-clips anything above white rather than rolling it
    // off — visible wherever an emissive surface or a specular highlight
    // exceeds 1.0.
    color = bloomAndExposure.w > 0.5 ? AcesTonemap(color) : clamp(color, 0.0, 1.0);

    // ---- Encode to display space. This pass is the single point where the
    // frame stops being scene-linear, which is why it runs every frame even
    // with every effect switched off.
    FragColor = vec4(LinearToSrgb(color), 1.0);
}
