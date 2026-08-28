#shader vertex
#version 430 core

in vec3 vPos;
out vec2 uv;

void main()
{
    gl_Position = vec4(vPos, 1.0);
    uv = vPos.xy * 0.5 + 0.5;
}

#shader fragment
#version 430 core

in vec2 uv;
out vec4 fragColor;

void main()
{
    vec3 color  = vec3(uv.x, uv.y, uv.x*uv.y);
    fragColor = vec4(color, 1.0);
}