#version 450

// G-Buffer vertex shader for deferred rendering

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_texcoord;
layout(location = 3) in vec3 in_tangent;

// G-Buffer outputs
layout(location = 0) out vec3 out_world_pos;
layout(location = 1) out vec3 out_normal;
layout(location = 2) out vec2 out_texcoord;

layout(push_constant) uniform PushConstants {
    mat4 model;
    mat4 view;
    mat4 projection;
} push;

void main() {
    vec4 world_pos = push.model * vec4(in_position, 1.0);
    out_world_pos = world_pos.xyz;
    out_normal = mat3(push.model) * in_normal;
    out_texcoord = in_texcoord;
    gl_Position = push.projection * push.view * world_pos;
}
