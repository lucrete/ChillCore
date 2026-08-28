#shader vertex
#version 430 core
#include "Include/objectUniforms.glinc"

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 texCoord;

void main()
{
    gl_Position = objectUniforms.mvp * vec4(aPos, 1.0);
    texCoord = aTexCoord;
}

#shader fragment
#version 430 core
#include "Include/materialUniforms.glinc"

out vec4 FragColor;

in vec2 texCoord;

layout(binding = 0) uniform sampler2D mainTex;

void main()
{
    float opacity = material.baseColorAndOpacity.w;
    vec4 texColor = texture(mainTex, texCoord);
    FragColor = vec4(texColor.rgb, texColor.a * opacity);
}