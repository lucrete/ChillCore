#shader vertex
#version 430 core

layout(location = 0) in vec3 vPos;
void main()
{
    gl_Position = vec4(vPos, 1.0);
}

#shader fragment
#version 430 core

out vec4 FragColor;
void main()
{
    FragColor = vec4(1.0, 0.0, 1.0, 1.0);
}
