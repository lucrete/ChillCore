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

// Custom per-material parameters. Written by Material::SetUniform,
// packed by std140 offset from this declaration.
// Slot must match MATERIAL_PARAMS_BINDING_SLOT in Code/Core/Rendering/Uniforms/UniformBindings.h.
// Unnamed block: members stay in global scope and read as plain uniforms.
layout(std140, binding = 5) uniform MaterialParams
{
    vec4 fadeColor;
};

void main()
{
    FragColor = fadeColor;
}
