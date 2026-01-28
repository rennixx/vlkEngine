#version 450

// G-Buffer fragment shader for deferred rendering

layout(location = 0) in vec3 in_world_pos;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_texcoord;

// G-Buffer outputs
layout(location = 0) out vec4 out_position;   // RGB16F
layout(location = 1) out vec4 out_normal;      // RGB16F
layout(location = 2) out vec4 out_albedo;      // RGBA8
layout(location = 3) out vec4 out_material;    // RGBA8 (roughness, metallic, ao, unused)

// Material properties
layout(set = 0, binding = 0) uniform sampler2D albedo_map;
layout(set = 0, binding = 1) uniform sampler2D normal_map;
layout(set = 0, binding = 2) uniform sampler2D roughness_map;
layout(set = 0, binding = 3) uniform sampler2D metallic_map;
layout(set = 0, binding = 4) uniform sampler2D ao_map;

layout(push_constant) uniform PushConstants {
    vec4 base_color;
    float roughness;
    float metallic;
    float ao;
    int use_albedo_map;
    int use_normal_map;
    int use_roughness_map;
    int use_metallic_map;
    int use_ao_map;
} push;

void main() {
    // Store world position
    out_position = vec4(in_world_pos, 1.0);

    // Store normal (normalized)
    vec3 normal = normalize(in_normal);
    out_normal = vec4(normal, 1.0);

    // Store albedo
    vec4 albedo = push.base_color;
    if (push.use_albedo_map != 0) {
        albedo *= texture(albedo_map, in_texcoord);
    }
    out_albedo = albedo;

    // Store material properties
    float roughness = push.roughness;
    float metallic = push.metallic;
    float ao = push.ao;

    if (push.use_roughness_map != 0) {
        roughness *= texture(roughness_map, in_texcoord).r;
    }
    if (push.use_metallic_map != 0) {
        metallic *= texture(metallic_map, in_texcoord).r;
    }
    if (push.use_ao_map != 0) {
        ao *= texture(ao_map, in_texcoord).r;
    }

    out_material = vec4(roughness, metallic, ao, 1.0);
}
