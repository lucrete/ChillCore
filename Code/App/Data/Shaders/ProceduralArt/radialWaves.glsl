#shader vertex
#version 430 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoord;

void main() {
    gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);
    TexCoord = aTexCoord;
}

#shader fragment
#version 430 core
#include "Include/colorSpace.glinc"
#include "Include/frameUniforms.glinc"

out vec4 FragColor;

in vec2 TexCoord;

vec3 palette (float t) {

    vec3 a = vec3(0.5, 0.5, 0.5);
    vec3 b = vec3(0.5, 0.5, 0.5);
    vec3 c = vec3(1.0, 1.0, 1.0);
    vec3 d = vec3(0.02, 0.29, 0.45);
    
    return a + b*cos(6.28318*(c*t+d));

}

void main() {
    float timeAbsolute = frameUniforms.cameraPositionAndTime.w;
    vec2 uv = TexCoord;//(TexCoord * 2.0 - 640) / 480;
    vec2 uv0 = TexCoord;
    vec3 finalColor = vec3(0.0);
    
    for (float i = 0.0; i < 3.0; i++) {
        uv = fract(uv * 1.5) - 0.5;

        float d = length(uv) * exp(-length(uv0));

        vec3 col = palette(length(uv0) - timeAbsolute*.25);

        d = sin(d*6.+ timeAbsolute*.5) * 0.3;
        d = abs(d);
        
        d = pow(0.03/ d, 1.5);
        
        finalColor += col * d;
    }   
    
    // Authored in display space, but the scene target holds scene-linear
    // light. Converting on output keeps the picked colours looking as
    // they were chosen once the post-process pass encodes back to sRGB.
    FragColor = vec4(SrgbToLinear(finalColor), 1.0);
}
