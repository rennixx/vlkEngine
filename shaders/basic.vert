#version 450

// Vertex shader for basic triangle rendering

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_color;

layout(location = 0) out vec3 frag_color;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
} push;

void main() {
    gl_Position = push.mvp * vec4(in_position, 1.0);
    frag_color = in_color;
}
