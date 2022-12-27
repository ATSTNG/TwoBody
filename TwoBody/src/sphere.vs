#version 330 core
layout(location = 0) in vec3 aPos; // position
layout(location = 1) in vec2 aTexPos; // texture position

out vec2 texPos;
out vec4 sphereCenter;
out vec4 fragPos;

uniform mat4 vertexTransform;
uniform mat4 modelTransform;
uniform float r;

void main() {
    vec3 pos = aPos * r;

    gl_Position = vertexTransform * vec4(pos, 1.0);
    sphereCenter = modelTransform * vec4(0, 0, 0, 1);
    fragPos = modelTransform * vec4(pos, 1.0);
    texPos = aTexPos;
}
