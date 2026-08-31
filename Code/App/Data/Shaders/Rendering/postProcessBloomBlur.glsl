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

layout(binding = 0) uniform sampler2D sourceTexture;

layout(std140, binding = 5) uniform CustomParams
{
    vec4 blurParams;  // xy: texel size, zw: blur direction
};

// Separable nine-tap Gaussian. Run once horizontally and once vertically, it
// costs eighteen taps instead of the eighty-one a single 2D kernel of the
// same width would need.
const float WEIGHTS[5] = float[](0.227027, 0.194594, 0.121621, 0.054054, 0.016216);

void main()
{
    vec2 step = blurParams.xy * blurParams.zw;
    vec3 result = texture(sourceTexture, TexCoords).rgb * WEIGHTS[0];

    for (int i = 1; i < 5; i++)
    {
        vec2 offset = step * float(i);
        result += texture(sourceTexture, TexCoords + offset).rgb * WEIGHTS[i];
        result += texture(sourceTexture, TexCoords - offset).rgb * WEIGHTS[i];
    }

    FragColor = vec4(result, 1.0);
}
