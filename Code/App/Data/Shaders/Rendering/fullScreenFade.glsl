#shader vertex
#version 430 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoords;

void main()
{
    gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);
}

#shader fragment
#version 430 core
out vec4 FragColor;

uniform vec4 fadeColor;

void main()
{
    FragColor = fadeColor;
}
