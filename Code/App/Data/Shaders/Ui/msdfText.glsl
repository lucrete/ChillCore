#shader vertex
#version 430 core
#include "Include/textUniforms.glinc"

layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoord;

void main()
{
    gl_Position = textUniforms.projection * vec4(aPos, 0.0, 1.0);
    TexCoord = aTexCoord;
}

#shader fragment
#version 430 core
#include "Include/textUniforms.glinc"

in vec2 TexCoord;

out vec4 FragColor;

layout(binding = 0) uniform sampler2D fontAtlas;

float median(float r, float g, float b)
{
    return max(min(r, g), min(max(r, g), b));
}

void main()
{
    vec3 msd = texture(fontAtlas, TexCoord).rgb;
    float dist = median(msd.r, msd.g, msd.b);
    float screenPxDist = textUniforms.pixelRange * (dist - 0.5);
    float opacity = clamp(screenPxDist + 0.5, 0.0, 1.0);
    FragColor = vec4(textUniforms.textColor.rgb, textUniforms.textColor.a * opacity);
}
