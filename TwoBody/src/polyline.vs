#version 330 core
layout(location = 0) in vec3 aPos; // position

uniform mat4 vertexTransform;

void main() {
    gl_Position = vertexTransform * vec4(aPos, 1.0);
}
