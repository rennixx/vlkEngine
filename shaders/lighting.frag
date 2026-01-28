#version 450

// Lighting pass fragment shader for deferred rendering

layout(location = 0) in vec2 in_texcoord;
layout(location = 0) out vec4 out_color;

// G-Buffer inputs
layout(set = 0, binding = 0) uniform sampler2D g_position;
layout(set = 0, binding = 1) uniform sampler2D g_normal;
layout(set = 0, binding = 2) uniform sampler2D g_albedo;
layout(set = 0, binding = 3) uniform sampler2D g_material;

// IBL
layout(set = 0, binding = 4) uniform samplerCube irradiance_map;
layout(set = 0, binding = 5) uniform samplerCube prefilter_map;
layout(set = 0, binding = 6) uniform sampler2D brdf_lut;

// Camera
layout(push_constant) uniform PushConstants {
    vec3 camera_pos;
    int debug_mode;  // 0=final, 1=position, 2=normal, 3=albedo, 4=material
} push;

const float PI = 3.14159265359;

// PBR functions
vec3 fresnel_schlick(float cos_theta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cos_theta, 0.0, 1.0), 5.0);
}

vec3 fresnel_schlick_roughness(float cos_theta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cos_theta, 0.0, 1.0), 5.0);
}

float distribution_ggx(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / denom;
}

float geometry_schlick_ggx(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}

float geometry_smith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = geometry_schlick_ggx(NdotV, roughness);
    float ggx1 = geometry_schlick_ggx(NdotL, roughness);

    return ggx1 * ggx2;
}

void main() {
    // Sample G-Buffer
    vec3 world_pos = texture(g_position, in_texcoord).rgb;
    vec3 normal = normalize(texture(g_normal, in_texcoord).rgb);
    vec3 albedo = texture(g_albedo, in_texcoord).rgb;
    vec3 material = texture(g_material, in_texcoord).rgb;

    float roughness = material.r;
    float metallic = material.g;
    float ao = material.b;

    // Debug modes
    if (push.debug_mode == 1) {
        out_color = vec4(world_pos * 0.01, 1.0);
        return;
    }
    if (push.debug_mode == 2) {
        out_color = vec4(normal * 0.5 + 0.5, 1.0);
        return;
    }
    if (push.debug_mode == 3) {
        out_color = vec4(albedo, 1.0);
        return;
    }
    if (push.debug_mode == 4) {
        out_color = vec4(roughness, metallic, ao, 1.0);
        return;
    }

    vec3 N = normal;
    vec3 V = normalize(push.camera_pos - world_pos);

    // Calculate reflectance
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    // Lighting (simplified single directional light for now)
    vec3 L = normalize(vec3(1.0, 1.0, 1.0));
    vec3 H = normalize(V + L);
    vec3 radiance = vec3(1.0);  // White light

    // Cook-Torrance BRDF
    float NDF = distribution_ggx(N, H, roughness);
    float G = geometry_smith(N, V, L, roughness);
    vec3 F = fresnel_schlick(max(dot(H, V), 0.0), F0);

    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3 specular = numerator / denominator;

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;

    float NdotL = max(dot(N, L), 0.0);
    vec3 Lo = (kD * albedo / PI + specular) * radiance * NdotL;

    // Ambient with IBL (simplified)
    vec3 R = reflect(-V, N);
    vec3 F_ambient = fresnel_schlick_roughness(max(dot(N, V), 0.0), F0, roughness);

    vec3 kD_ambient = (1.0 - F_ambient) * (1.0 - metallic);
    vec3 irradiance = texture(irradiance_map, N).rgb;
    vec3 diffuse = irradiance * albedo;

    vec3 prefiltered = textureLod(prefilter_map, R, roughness * 4.0).rgb;
    vec2 brdf = texture(brdf_lut, vec2(max(dot(N, V), 0.0), roughness)).rg;
    vec3 specular_ambient = prefiltered * (F_ambient * brdf.x + brdf.y);

    vec3 ambient = (kD_ambient * diffuse + specular_ambient) * ao;
    vec3 color = ambient + Lo;

    // Tone mapping (ACES approx)
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));  // Gamma correction

    out_color = vec4(color, 1.0);
}
