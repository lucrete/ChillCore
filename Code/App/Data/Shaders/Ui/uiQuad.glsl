#shader vertex
#version 430 core
#include "Include/uiUniforms.glinc"

layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in vec4 aColor;

out vec2 TexCoord;
out vec4 Color;

void main()
{
    gl_Position = uiUniforms.projection * vec4(aPos, 0.0, 1.0);
    TexCoord = aTexCoord;
    Color = aColor;
}

#shader fragment
#version 430 core
in vec2 TexCoord;
in vec4 Color;

out vec4 FragColor;

layout(binding = 0) uniform sampler2D mainTex;

void main()
{
    FragColor = texture(mainTex, TexCoord) * Color;
}
