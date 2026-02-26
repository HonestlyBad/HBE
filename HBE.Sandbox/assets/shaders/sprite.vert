#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;

out vec2 vUV;

uniform mat4 uMVP;
uniform vec4 uUVRect; // xy offset, zw scale

void main() {
    vUV = aUV * uUVRect.zw + uUVRect.xy;
    gl_Position = uMVP * vec4(aPos, 1.0);
}