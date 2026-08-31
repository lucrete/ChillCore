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

layout(std140, binding = 5) uniform CustomParams
{
    vec4 prefilterParams;  // x: threshold, y: knee
};

// Soft-knee bright pass. A hard threshold makes bloom pop in and out as a
// highlight crosses it, which reads as flicker in motion; the knee ramps the
// contribution in over a range around the threshold instead.
void main()
{
    vec3 color = texture(sceneTexture, TexCoords).rgb;

    float threshold = prefilterParams.x;
    float knee = max(prefilterParams.y * threshold, 0.0001);
    float brightness = max(color.r, max(color.g, color.b));

    float soft = clamp(brightness - threshold + knee, 0.0, 2.0 * knee);
    soft = soft * soft / (4.0 * knee);
    float contribution = max(soft, brightness - threshold) / max(brightness, 0.0001);

    FragColor = vec4(color * contribution, 1.0);
}
