#version 100
precision highp float;
precision highp int;

/* clang-format off */
/* modes GLES2 doesn't support preprocessor directives like this, but we'll keep them for reference */
#define mode_color 0
#define mode_color_instancing 1
#define mode_depth 2
#define mode_depth_instancing 3

/* specializations */
#define DISABLE_LIGHTMAP false
#define DISABLE_LIGHT_DIRECTIONAL false
#define DISABLE_LIGHT_OMNI false
#define DISABLE_LIGHT_SPOT false
#define DISABLE_REFLECTION_PROBE true
#define DISABLE_FOG false
#define USE_DEPTH_FOG false
#define USE_RADIANCE_MAP true
#define USE_LIGHTMAP false
#define USE_SH_LIGHTMAP false
#define USE_LIGHTMAP_CAPTURE false
#define USE_MULTIVIEW false
#define RENDER_SHADOWS false
#define RENDER_SHADOWS_LINEAR false
#define SHADOW_MODE_PCF_5 false
#define SHADOW_MODE_PCF_13 false
#define LIGHT_USE_PSSM2 false
#define LIGHT_USE_PSSM4 false
#define LIGHT_USE_PSSM_BLEND false
#define BASE_PASS true
#define USE_ADDITIVE_LIGHTING false
#define APPLY_TONEMAPPING true
#define ADDITIVE_OMNI false
#define ADDITIVE_SPOT false
#define RENDER_MATERIAL false
#define SECOND_REFLECTION_PROBE false
#define LIGHTMAP_BICUBIC_FILTER false

#[vertex]

#define M_PI 3.14159265359

#define SHADER_IS_SRGB true
#define SHADER_SPACE_FAR -1.0

#include "stdlib_inc.glsl"

#if !defined(MODE_RENDER_DEPTH) || defined(TANGENT_USED) || defined(NORMAL_MAP_USED) || defined(LIGHT_ANISOTROPY_USED) ||defined(LIGHT_CLEARCOAT_USED)
#ifndef NORMAL_USED
#define NORMAL_USED
#endif
#endif

#ifdef MODE_UNSHADED
#ifdef USE_ADDITIVE_LIGHTING
#undef USE_ADDITIVE_LIGHTING
#endif
#endif // MODE_UNSHADED

/* INPUT ATTRIBS */

attribute highp vec4 vertex_angle_attrib;

#ifdef NORMAL_USED
attribute vec4 axis_tangent_attrib;
#endif

attribute vec4 color_attrib;

#ifdef UV_USED
attribute vec2 uv_attrib;
#endif

#if defined(UV2_USED) || defined(USE_LIGHTMAP) || defined(RENDER_MATERIAL)
attribute vec2 uv2_attrib;
#endif

#if defined(CUSTOM0_USED)
attribute vec4 custom0_attrib;
#endif

#if defined(CUSTOM1_USED)
attribute vec4 custom1_attrib;
#endif

#if defined(CUSTOM2_USED)
attribute vec4 custom2_attrib;
#endif

#if defined(CUSTOM3_USED)
attribute vec4 custom3_attrib;
#endif

#if defined(BONES_USED)
attribute vec4 bone_attrib;
#endif

#if defined(WEIGHTS_USED)
attribute vec4 weight_attrib;
#endif

vec3 oct_to_vec3(vec2 e) {
    vec3 v = vec3(e.xy, 1.0 - abs(e.x) - abs(e.y));
    float t = max(-v.z, 0.0);
    v.xy += t * -sign(v.xy);
    return normalize(v);
}

void axis_angle_to_tbn(vec3 axis, float angle, out vec3 tangent, out vec3 binormal, out vec3 normal) {
    float c = cos(angle);
    float s = sin(angle);
    vec3 omc_axis = (1.0 - c) * axis;
    vec3 s_axis = s * axis;
    tangent = omc_axis.xxx * axis + vec3(c, -s_axis.z, s_axis.y);
    binormal = omc_axis.yyy * axis + vec3(s_axis.z, c, -s_axis.x);
    normal = omc_axis.zzz * axis + vec3(-s_axis.y, s_axis.x, c);
}

/* GLES2 doesn't support instancing, so we'll comment out this section */
/*
#ifdef USE_INSTANCING
attribute highp vec4 instance_xform0;
attribute highp vec4 instance_xform1;
attribute highp vec4 instance_xform2;
attribute highp vec4 instance_color_custom_data;
#endif
*/

#define FLAGS_NON_UNIFORM_SCALE 16

/* GLES2 doesn't support uniform blocks, so we'll need to declare individual uniforms */
uniform vec4 global_shader_uniforms[MAX_GLOBAL_SHADER_UNIFORMS];

/* Scene data uniforms */
uniform highp mat4 projection_matrix;
uniform highp mat4 inv_projection_matrix;
uniform highp mat4 inv_view_matrix;
uniform highp mat4 view_matrix;

uniform highp mat4 main_cam_inv_view_matrix;

uniform vec2 viewport_size;
uniform vec2 screen_pixel_size;

uniform mediump vec4 ambient_light_color_energy;

uniform mediump float ambient_color_sky_mix;
uniform float emissive_exposure_normalization;
uniform bool use_ambient_light;

uniform bool use_ambient_cubemap;
uniform bool use_reflection_cubemap;
uniform float fog_aerial_perspective;
uniform float time;

uniform mat3 radiance_inverse_xform;

uniform int directional_light_count;
uniform float z_far;
uniform float z_near;
uniform float IBL_exposure_normalization;

uniform bool fog_enabled;
uniform int fog_mode;
uniform float fog_density;
uniform float fog_height;

uniform float fog_height_density;
uniform float fog_depth_curve;
uniform float fog_sun_scatter;
uniform float fog_depth_begin;

uniform vec3 fog_light_color;
uniform float fog_depth_end;

uniform float shadow_bias;
uniform float luminance_multiplier;
uniform int camera_visible_layers;
uniform bool pancake_shadows;

/* GLES2 doesn't support these features, so we'll comment them out */
/*
#ifdef USE_MULTIVIEW
uniform mat4 projection_matrix_view[MAX_VIEWS];
uniform mat4 inv_projection_matrix_view[MAX_VIEWS];
uniform vec4 eye_offset[MAX_VIEWS];
#endif
*/

uniform highp mat4 world_transform;
uniform highp vec3 compressed_aabb_position;
uniform highp vec3 compressed_aabb_size;
uniform highp vec4 uv_scale;

uniform int model_flags;

#ifdef RENDER_MATERIAL
uniform mediump vec2 uv_offset;
#endif

/* Varyings */

varying highp vec3 vertex_interp;
#ifdef NORMAL_USED
varying vec3 normal_interp;
#endif

#if defined(COLOR_USED)
varying vec4 color_interp;
#endif

#if defined(UV_USED)
varying vec2 uv_interp;
#endif

#if defined(UV2_USED) || defined(USE_LIGHTMAP)
varying vec2 uv2_interp;
#endif

#if defined(TANGENT_USED) || defined(NORMAL_MAP_USED) || defined(LIGHT_ANISOTROPY_USED)
varying vec3 tangent_interp;
varying vec3 binormal_interp;
#endif

#ifdef USE_ADDITIVE_LIGHTING
varying highp vec4 shadow_coord;

#if defined(LIGHT_USE_PSSM2) || defined(LIGHT_USE_PSSM4)
varying highp vec4 shadow_coord2;
#endif

#ifdef LIGHT_USE_PSSM4
varying highp vec4 shadow_coord3;
varying highp vec4 shadow_coord4;
#endif //LIGHT_USE_PSSM4
#endif

/* clang-format off */

/* GLOBALS will be inserted here in C++ */

/* clang-format on */

void main() {
    highp vec3 vertex = vertex_angle_attrib.xyz * compressed_aabb_size + compressed_aabb_position;

    highp mat4 model_matrix = world_transform;
    /* GLES2 doesn't support instancing, so we'll comment out this section */
    /*
    #ifdef USE_INSTANCING
    highp mat4 m = mat4(instance_xform0, instance_xform1, instance_xform2, vec4(0.0, 0.0, 0.0, 1.0));
    model_matrix = model_matrix * transpose(m);
    #endif
    */

#ifdef NORMAL_USED
    vec3 normal = oct_to_vec3(axis_tangent_attrib.xy * 2.0 - 1.0);
#endif

    highp mat3 model_normal_matrix;

    if (model_flags == FLAGS_NON_UNIFORM_SCALE) {
        /* GLES2 doesn't have inverse(), so we'll need to implement it */
        /* model_normal_matrix = transpose(inverse(mat3(model_matrix))); */
        model_normal_matrix = mat3(model_matrix); /* Placeholder, needs proper implementation */
    } else {
        model_normal_matrix = mat3(model_matrix);
    }

#if defined(NORMAL_USED) || defined(TANGENT_USED) || defined(NORMAL_MAP_USED) || defined(LIGHT_ANISOTROPY_USED)

    vec3 binormal;
    float binormal_sign;
    vec3 tangent;
    if (axis_tangent_attrib.z > 0.0 || axis_tangent_attrib.w < 1.0) {
        vec2 signed_tangent_attrib = axis_tangent_attrib.zw * 2.0 - 1.0;
        tangent = oct_to_vec3(vec2(signed_tangent_attrib.x, abs(signed_tangent_attrib.y) * 2.0 - 1.0));
        binormal_sign = sign(signed_tangent_attrib.y);
        binormal = normalize(cross(normal, tangent) * binormal_sign);
    } else {
        float angle = vertex_angle_attrib.w;
        binormal_sign = angle > 0.5 ? 1.0 : -1.0;
        angle = abs(angle * 2.0 - 1.0) * M_PI;
        vec3 axis = normal;
        axis_angle_to_tbn(axis, angle, tangent, binormal, normal);
        binormal *= binormal_sign;
    }
#endif

#if defined(COLOR_USED)
    color_interp = color_attrib;
    /* GLES2 doesn't support instancing, so we'll comment out this section */
    /*
    #ifdef USE_INSTANCING
    vec4 instance_color;
    instance_color.xy = unpackHalf2x16(instance_color_custom_data.x);
    instance_color.zw = unpackHalf2x16(instance_color_custom_data.y);
    color_interp *= instance_color;
    #endif
    */
#endif

#if defined(UV_USED)
    uv_interp = uv_attrib;
#endif

#if defined(UV2_USED) || defined(USE_LIGHTMAP)
    uv2_interp = uv2_attrib;
#endif

    if (uv_scale != vec4(0.0)) {
#ifdef UV_USED
        uv_interp = (uv_interp - 0.5) * uv_scale.xy;
#endif
#if defined(UV2_USED) || defined(USE_LIGHTMAP)
        uv2_interp = (uv2_interp - 0.5) * uv_scale.zw;
#endif
    }

#if defined(OVERRIDE_POSITION)
    highp vec4 position;
#endif

    /* GLES2 doesn't support multiview, so we'll use regular matrices */
    mat4 projection_matrix = projection_matrix;
    mat4 inv_projection_matrix = inv_projection_matrix;
    vec3 eye_offset = vec3(0.0, 0.0, 0.0);

    /* GLES2 doesn't support instancing, so we'll comment out this section */
    /*
    #ifdef USE_INSTANCING
    vec4 instance_custom;
    instance_custom.xy = unpackHalf2x16(instance_color_custom_data.z);
    instance_custom.zw = unpackHalf2x16(instance_color_custom_data.w);
    #else
    vec4 instance_custom = vec4(0.0);
    #endif
    */
    vec4 instance_custom = vec4(0.0);

    float roughness = 1.0;

    highp mat4 modelview = view_matrix * model_matrix;
    highp mat3 modelview_normal = mat3(view_matrix) * model_normal_matrix;

    float point_size = 1.0;

    {
#CODE : VERTEX
    }

    gl_PointSize = point_size;

    // Using local coordinates (default)
#if !defined(SKIP_TRANSFORM_USED) && !defined(VERTEX_WORLD_COORDS_USED)

    vertex = (modelview * vec4(vertex, 1.0)).xyz;
#ifdef NORMAL_USED
    normal = modelview_normal * normal;
#endif

#if defined(TANGENT_USED) || defined(NORMAL_MAP_USED) || defined(LIGHT_ANISOTROPY_USED)

    binormal = modelview_normal * binormal;
    tangent = modelview_normal * tangent;
#endif
#endif // !defined(SKIP_TRANSFORM_USED) && !defined(VERTEX_WORLD_COORDS_USED)

    // Using world coordinates
#if !defined(SKIP_TRANSFORM_USED) && defined(VERTEX_WORLD_COORDS_USED)

    vertex = (view_matrix * vec4(vertex, 1.0)).xyz;
#ifdef NORMAL_USED
    normal = (view_matrix * vec4(normal, 0.0)).xyz;
#endif

#if defined(TANGENT_USED) || defined(NORMAL_MAP_USED) || defined(LIGHT_ANISOTROPY_USED)
    binormal = (view_matrix * vec4(binormal, 0.0)).xyz;
    tangent = (view_matrix * vec4(tangent, 0.0)).xyz;
#endif
#endif

    vertex_interp = vertex;
#ifdef NORMAL_USED
    normal_interp = normal;
#endif

#if defined(TANGENT_USED) || defined(NORMAL_MAP_USED) || defined(LIGHT_ANISOTROPY_USED)
    tangent_interp = tangent;
    binormal_interp = binormal;
#endif

    // Calculate shadows.
#ifdef USE_ADDITIVE_LIGHTING
#if defined(ADDITIVE_OMNI) || defined(ADDITIVE_SPOT)
    // Apply normal bias at draw time to avoid issues with scaling non-fused geometry.
    vec3 light_rel_vec = positional_shadows[positional_shadow_index].light_position - vertex_interp;
    float light_length = length(light_rel_vec);
    float aNdotL = abs(dot(normalize(normal_interp), normalize(light_rel_vec)));
    vec3 normal_offset = (1.0 - aNdotL) * positional_shadows[positional_shadow_index].shadow_normal_bias * light_length * normal_interp;

#ifdef ADDITIVE_SPOT
    // Calculate coord here so we can take advantage of prefetch.
    shadow_coord = positional_shadows[positional_shadow_index].shadow_matrix * vec4(vertex_interp + normal_offset, 1.0);
#endif

#ifdef ADDITIVE_OMNI
    // Can't interpolate unit direction nicely, so forget about prefetch.
    shadow_coord = vec4(vertex_interp + normal_offset, 1.0);
#endif
#else // ADDITIVE_DIRECTIONAL
    vec3 base_normal_bias = normalize(normal_interp) * (1.0 - max(0.0, dot(directional_shadows[directional_shadow_index].direction, -normalize(normal_interp))));
    vec3 normal_offset = base_normal_bias * directional_shadows[directional_shadow_index].shadow_normal_bias.x;
    shadow_coord = directional_shadows[directional_shadow_index].shadow_matrix1 * vec4(vertex_interp + normal_offset, 1.0);

#if defined(LIGHT_USE_PSSM2) || defined(LIGHT_USE_PSSM4)
    normal_offset = base_normal_bias * directional_shadows[directional_shadow_index].shadow_normal_bias.y;
    shadow_coord2 = directional_shadows[directional_shadow_index].shadow_matrix2 * vec4(vertex_interp + normal_offset, 1.0);
#endif

#ifdef LIGHT_USE_PSSM4
    normal_offset = base_normal_bias * directional_shadows[directional_shadow_index].shadow_normal_bias.z;
    shadow_coord3 = directional_shadows[directional_shadow_index].shadow_matrix3 * vec4(vertex_interp + normal_offset, 1.0);
    normal_offset = base_normal_bias * directional_shadows[directional_shadow_index].shadow_normal_bias.w;
    shadow_coord4 = directional_shadows[directional_shadow_index].shadow_matrix4 * vec4(vertex_interp + normal_offset, 1.0);
#endif //LIGHT_USE_PSSM4

#endif // !(defined(ADDITIVE_OMNI) || defined(ADDITIVE_SPOT))
#endif // USE_ADDITIVE_LIGHTING

#if defined(RENDER_SHADOWS) && !defined(RENDER_SHADOWS_LINEAR)
    // This is an optimized version of normalize(vertex_interp) * scene_data.shadow_bias / length(vertex_interp).
    float light_length_sq = dot(vertex_interp, vertex_interp);
    vertex_interp += vertex_interp * shadow_bias / light_length_sq;
#endif

#if defined(OVERRIDE_POSITION)
    gl_Position = position;
#else
    gl_Position = projection_matrix * vec4(vertex_interp, 1.0);
#endif

#ifdef RENDER_MATERIAL
    gl_Position.xy = (uv2_attrib.xy + uv_offset) * 2.0 - 1.0;
    gl_Position.z = 0.00001;
    gl_Position.w = 1.0;
#endif
}

#[fragment]

#if !defined(MODE_RENDER_DEPTH) || defined(TANGENT_USED) || defined(NORMAL_MAP_USED) || defined(LIGHT_ANISOTROPY_USED) ||defined(LIGHT_CLEARCOAT_USED)
#ifndef NORMAL_USED
#define NORMAL_USED
#endif
#endif

#ifdef MODE_UNSHADED
#ifdef USE_ADDITIVE_LIGHTING
#undef USE_ADDITIVE_LIGHTING
#endif
#endif // MODE_UNSHADED

#ifndef MODE_RENDER_DEPTH
/* Include tonemap_inc.glsl contents here */
#endif
/* Include stdlib_inc.glsl contents here */

#define M_PI 3.14159265359

#define SHADER_IS_SRGB true
#define SHADER_SPACE_FAR -1.0

#define FLAGS_NON_UNIFORM_SCALE 16

/* Varyings */

#if defined(COLOR_USED)
varying vec4 color_interp;
#endif

#if defined(UV_USED)
varying vec2 uv_interp;
#endif

#if defined(UV2_USED)
varying vec2 uv2_interp;
#else
#ifdef USE_LIGHTMAP
varying vec2 uv2_interp;
#endif
#endif

#if defined(TANGENT_USED) || defined(NORMAL_MAP_USED) || defined(LIGHT_ANISOTROPY_USED)
varying vec3 tangent_interp;
varying vec3 binormal_interp;
#endif

#ifdef NORMAL_USED
varying vec3 normal_interp;
#endif

varying highp vec3 vertex_interp;

#ifdef USE_ADDITIVE_LIGHTING
varying highp vec4 shadow_coord;

#if defined(LIGHT_USE_PSSM2) || defined(LIGHT_USE_PSSM4)
varying highp vec4 shadow_coord2;
#endif

#ifdef LIGHT_USE_PSSM4
varying highp vec4 shadow_coord3;
varying highp vec4 shadow_coord4;
#endif //LIGHT_USE_PSSM4
#endif

#ifdef USE_RADIANCE_MAP
#define RADIANCE_MAX_LOD 5.0
uniform samplerCube radiance_map;
#endif // USE_RADIANCE_MAP

#ifndef DISABLE_REFLECTION_PROBE
#define REFLECTION_PROBE_MAX_LOD 8.0

uniform bool refprobe1_use_box_project;
uniform highp vec3 refprobe1_box_extents;
uniform vec3 refprobe1_box_offset;
uniform highp mat4 refprobe1_local_matrix;
uniform bool refprobe1_exterior;
uniform float refprobe1_intensity;
uniform int refprobe1_ambient_mode;
uniform vec4 refprobe1_ambient_color;

uniform samplerCube refprobe1_texture;

#ifdef SECOND_REFLECTION_PROBE
uniform bool refprobe2_use_box_project;
uniform highp vec3 refprobe2_box_extents;
uniform vec3 refprobe2_box_offset;
uniform highp mat4 refprobe2_local_matrix;
uniform bool refprobe2_exterior;
uniform float refprobe2_intensity;
uniform int refprobe2_ambient_mode;
uniform vec4 refprobe2_ambient_color;

uniform samplerCube refprobe2_texture;
#endif // SECOND_REFLECTION_PROBE
#endif // DISABLE_REFLECTION_PROBE

/* GLES2 doesn't support uniform blocks, so we'll declare individual uniforms */
uniform vec4 global_shader_uniforms[MAX_GLOBAL_SHADER_UNIFORMS];

/* Material Uniforms */
#ifdef MATERIAL_UNIFORMS_USED
/* Insert material uniforms here */
#endif

/* Scene data uniforms */
uniform highp mat4 projection_matrix;
uniform highp mat4 inv_projection_matrix;
uniform highp mat4 inv_view_matrix;
uniform highp mat4 view_matrix;

uniform highp mat4 main_cam_inv_view_matrix;

uniform vec2 viewport_size;
uniform vec2 screen_pixel_size;

uniform mediump vec4 ambient_light_color_energy;

uniform mediump float ambient_color_sky_mix;
uniform float emissive_exposure_normalization;
uniform bool use_ambient_light;

uniform bool use_ambient_cubemap;
uniform bool use_reflection_cubemap;
uniform float fog_aerial_perspective;
uniform float time;

uniform mat3 radiance_inverse_xform;

uniform int directional_light_count;
uniform float z_far;
uniform float z_near;
uniform float IBL_exposure_normalization;

uniform bool fog_enabled;
uniform int fog_mode;
uniform float fog_density;
uniform float fog_height;

uniform float fog_height_density;
uniform float fog_depth_curve;
uniform float fog_sun_scatter;
uniform float fog_depth_begin;

uniform vec3 fog_light_color;
uniform float fog_depth_end;

uniform float shadow_bias;
uniform float luminance_multiplier;
uniform int camera_visible_layers;
uniform bool pancake_shadows;

/* GLES2 doesn't support these features, so we'll comment them out */
/*
#ifdef USE_MULTIVIEW
uniform mat4 projection_matrix_view[MAX_VIEWS];
uniform mat4 inv_projection_matrix_view[MAX_VIEWS];
uniform vec4 eye_offset[MAX_VIEWS];
#endif
*/

#define LIGHT_BAKE_DISABLED 0
#define LIGHT_BAKE_STATIC 1
#define LIGHT_BAKE_DYNAMIC 2

#ifndef MODE_RENDER_DEPTH
/* Directional light data */
#if !defined(DISABLE_LIGHT_DIRECTIONAL) || (!defined(ADDITIVE_OMNI) && !defined(ADDITIVE_SPOT))
struct DirectionalLightData {
    mediump vec3 direction;
    mediump float energy;
    mediump vec3 color;
    mediump float size;
    lowp int unused;
    lowp int bake_mode;
    mediump float shadow_opacity;
    mediump float specular;
};

uniform DirectionalLightData directional_lights[MAX_DIRECTIONAL_LIGHT_DATA_STRUCTS];

#if defined(USE_ADDITIVE_LIGHTING) && (!defined(ADDITIVE_OMNI) && !defined(ADDITIVE_SPOT))
uniform highp sampler2D directional_shadow_atlas;
#endif
#endif

/* Omni and spot light data */
#if !defined(DISABLE_LIGHT_OMNI) || !defined(DISABLE_LIGHT_SPOT) || defined(ADDITIVE_OMNI) || defined(ADDITIVE_SPOT)
struct LightData {
    highp vec3 position;
    highp float inv_radius;
    mediump vec3 direction;
    highp float size;
    mediump vec3 color;
    mediump float attenuation;
    mediump float cone_attenuation;
    mediump float cone_angle;
    mediump float specular_amount;
    mediump float shadow_opacity;
    lowp vec3 pad;
    lowp int bake_mode;
};

#if !defined(DISABLE_LIGHT_OMNI) || defined(ADDITIVE_OMNI)
uniform LightData omni_lights[MAX_LIGHT_DATA_STRUCTS];
#ifdef BASE_PASS
uniform int omni_light_indices[MAX_FORWARD_LIGHTS];
uniform int omni_light_count;
#endif
#endif

#if !defined(DISABLE_LIGHT_SPOT) || defined(ADDITIVE_SPOT)
uniform LightData spot_lights[MAX_LIGHT_DATA_STRUCTS];
#ifdef BASE_PASS
uniform int spot_light_indices[MAX_FORWARD_LIGHTS];
uniform int spot_light_count;
#endif
#endif
#endif

#ifdef USE_ADDITIVE_LIGHTING
#ifdef ADDITIVE_OMNI
uniform highp samplerCube omni_shadow_texture;
uniform lowp int omni_light_index;
#endif
#ifdef ADDITIVE_SPOT
uniform highp sampler2D spot_shadow_texture;
uniform lowp int spot_light_index;
#endif

#if defined(ADDITIVE_OMNI) || defined(ADDITIVE_SPOT)
struct PositionalShadowData {
    highp mat4 shadow_matrix;
    highp vec3 light_position;
    highp float shadow_normal_bias;
    vec3 pad;
    highp float shadow_atlas_pixel_size;
};

uniform PositionalShadowData positional_shadows[MAX_LIGHT_DATA_STRUCTS];
uniform lowp int positional_shadow_index;
#else
struct DirectionalShadowData {
    highp vec3 direction;
    highp float shadow_atlas_pixel_size;
    highp vec4 shadow_normal_bias;
    highp vec4 shadow_split_offsets;
    highp mat4 shadow_matrix1;
    highp mat4 shadow_matrix2;
    highp mat4 shadow_matrix3;
    highp mat4 shadow_matrix4;
    mediump float fade_from;
    mediump float fade_to;
    mediump vec2 pad;
};

uniform DirectionalShadowData directional_shadows[MAX_DIRECTIONAL_LIGHT_DATA_STRUCTS];
uniform lowp int directional_shadow_index;
#endif
#endif

#endif // !MODE_RENDER_DEPTH

#ifndef DISABLE_LIGHTMAP
#ifdef USE_LIGHTMAP
uniform mediump sampler2D lightmap_textures;
uniform lowp int lightmap_slice;
uniform highp vec4 lightmap_uv_scale;
uniform float lightmap_exposure_normalization;

#ifdef LIGHTMAP_BICUBIC_FILTER
uniform highp vec2 lightmap_texture_size;
#endif

#ifdef USE_SH_LIGHTMAP
uniform mediump mat3 lightmap_normal_xform;
#endif
#endif

#ifdef USE_LIGHTMAP_CAPTURE
uniform mediump vec4 lightmap_captures[9];
#endif
#endif

uniform highp sampler2D depth_buffer;
uniform highp sampler2D color_buffer;

uniform highp mat4 world_transform;
uniform mediump float opaque_prepass_threshold;
uniform int model_flags;

#if defined(RENDER_MATERIAL)
#define MAX_RENDER_MATERIAL_OUTPUTS 4
#endif

#ifndef RENDER_MATERIAL
// Normal color rendering
#define MAX_RENDER_MATERIAL_OUTPUTS 1
#endif

/* Output buffers */
#if __VERSION__ >= 130
out vec4 frag_color[MAX_RENDER_MATERIAL_OUTPUTS];
#else
#define frag_color gl_FragData
#endif

/* Function declarations */
vec3 F0(float metallic, float specular, vec3 albedo) {
    float dielectric = 0.16 * specular * specular;
    return mix(vec3(dielectric), albedo, vec3(metallic));
}

/* Main function */
void main() {
    highp vec3 vertex = vertex_interp;
    vec3 view = -normalize(vertex_interp);
    highp mat4 model_matrix = world_transform;
    vec3 albedo = vec3(1.0);
    vec3 backlight = vec3(0.0);
    vec4 transmittance_color = vec4(0.0, 0.0, 0.0, 1.0);
    float transmittance_depth = 0.0;
    float transmittance_boost = 0.0;
    float metallic = 0.0;
    float specular = 0.5;
    vec3 emission = vec3(0.0);
    float roughness = 1.0;
    float rim = 0.0;
    float rim_tint = 0.0;
    float clearcoat = 0.0;
    float clearcoat_roughness = 0.0;
    float anisotropy = 0.0;
    vec2 anisotropy_flow = vec2(1.0, 0.0);
#ifdef PREMUL_ALPHA_USED
    float premul_alpha = 1.0;
#endif
#ifndef FOG_DISABLED
    vec4 fog = vec4(0.0);
#endif
#if defined(CUSTOM_RADIANCE_USED)
    vec4 custom_radiance = vec4(0.0);
#endif
#if defined(CUSTOM_IRRADIANCE_USED)
    vec4 custom_irradiance = vec4(0.0);
#endif

    float ao = 1.0;
    float ao_light_affect = 0.0;

    float alpha = 1.0;

#if defined(TANGENT_USED) || defined(NORMAL_MAP_USED) || defined(LIGHT_ANISOTROPY_USED)
    vec3 binormal = normalize(binormal_interp);
    vec3 tangent = normalize(tangent_interp);
#else
    vec3 binormal = vec3(0.0);
    vec3 tangent = vec3(0.0);
#endif

#ifdef NORMAL_USED
    vec3 normal = normalize(normal_interp);
#endif

#ifdef UV_USED
    vec2 uv = uv_interp;
#endif

#if defined(UV2_USED) || defined(USE_LIGHTMAP)
    vec2 uv2 = uv2_interp;
#endif

#if defined(COLOR_USED)
    vec4 color = color_interp;
#endif

#if defined(NORMAL_MAP_USED)
    vec3 normal_map = vec3(0.5);
#endif

    float normal_map_depth = 1.0;

    vec2 screen_uv = gl_FragCoord.xy * screen_pixel_size;

    float sss_strength = 0.0;

#ifdef ALPHA_SCISSOR_USED
    float alpha_scissor_threshold = 1.0;
#endif

#ifdef ALPHA_HASH_USED
    float alpha_hash_scale = 1.0;
#endif

#ifdef ALPHA_ANTIALIASING_EDGE_USED
    float alpha_antialiasing_edge = 0.0;
    vec2 alpha_texture_coordinate = vec2(0.0, 0.0);
#endif

#ifdef LIGHT_VERTEX_USED
    vec3 light_vertex = vertex;
#endif

    highp mat3 model_normal_matrix;
    if (model_flags == FLAGS_NON_UNIFORM_SCALE) {
        model_normal_matrix = mat3(model_matrix);
        /* GLES2 doesn't have inverse(), so we'll need to implement it */
        /* model_normal_matrix = transpose(inverse(mat3(model_matrix))); */
    } else {
        model_normal_matrix = mat3(model_matrix);
    }

    {
#CODE : FRAGMENT
    }

    // Keep albedo values in positive number range
    albedo = max(albedo, vec3(0.0));

#ifdef LIGHT_VERTEX_USED
    vertex = light_vertex;
    view = -normalize(vertex);
#endif //LIGHT_VERTEX_USED

#ifndef USE_SHADOW_TO_OPACITY

#if defined(ALPHA_SCISSOR_USED)
    if (alpha < alpha_scissor_threshold) {
        discard;
    }
    alpha = 1.0;
#else
#ifdef MODE_RENDER_DEPTH
#ifdef USE_OPAQUE_PREPASS
    if (alpha < opaque_prepass_threshold) {
        discard;
    }
#endif // USE_OPAQUE_PREPASS
#endif // MODE_RENDER_DEPTH
#endif // !ALPHA_SCISSOR_USED

#endif // !USE_SHADOW_TO_OPACITY

#ifdef NORMAL_MAP_USED
    normal_map.xy = normal_map.xy * 2.0 - 1.0;
    normal_map.z = sqrt(max(0.0, 1.0 - dot(normal_map.xy, normal_map.xy)));
    normal = normalize(mix(normal, tangent * normal_map.x + binormal * normal_map.y + normal * normal_map.z, normal_map_depth));
#endif

#ifdef LIGHT_ANISOTROPY_USED
    if (anisotropy > 0.01) {
        //rotation matrix
        mat3 rot = mat3(tangent, binormal, normal);
        //make local to space
        tangent = normalize(rot * vec3(anisotropy_flow.x, anisotropy_flow.y, 0.0));
        binormal = normalize(rot * vec3(-anisotropy_flow.y, anisotropy_flow.x, 0.0));
    }
#endif

#ifndef MODE_RENDER_DEPTH

#ifndef FOG_DISABLED
#ifndef CUSTOM_FOG_USED
#ifndef DISABLE_FOG
    // fog must be processed as early as possible and then packed.
    // to maximize VGPR usage
    if (fog_enabled) {
        fog = fog_process(vertex);
    }
#endif // !DISABLE_FOG
#endif // !CUSTOM_FOG_USED

    // GLES2 doesn't support packHalf2x16, so we'll need an alternative packing method
    // For now, we'll just store the fog values directly
    vec2 fog_rg = fog.rg;
    vec2 fog_ba = fog.ba;
#endif // !FOG_DISABLED

    // Convert colors to linear
    albedo = srgb_to_linear(albedo);
    emission = srgb_to_linear(emission);

#ifndef MODE_UNSHADED
    vec3 f0 = F0(metallic, specular, albedo);
    vec3 specular_light = vec3(0.0, 0.0, 0.0);
    vec3 diffuse_light = vec3(0.0, 0.0, 0.0);
    vec3 ambient_light = vec3(0.0, 0.0, 0.0);

#ifdef BASE_PASS
    /////////////////////// LIGHTING //////////////////////////////

#ifndef AMBIENT_LIGHT_DISABLED
    // IBL precalculations
    float ndotv = clamp(dot(normal, view), 0.0, 1.0);
    vec3 F = f0 + (max(vec3(1.0 - roughness), f0) - f0) * pow(1.0 - ndotv, 5.0);

#ifdef USE_RADIANCE_MAP
    if (use_reflection_cubemap) {
#ifdef LIGHT_ANISOTROPY_USED
        // Anisotropic reflection code here (simplified for GLES2)
        vec3 anisotropic_direction = anisotropy >= 0.0 ? binormal : tangent;
        vec3 anisotropic_tangent = cross(anisotropic_direction, view);
        vec3 anisotropic_normal = cross(anisotropic_tangent, anisotropic_direction);
        vec3 bent_normal = normalize(mix(normal, anisotropic_normal, abs(anisotropy) * clamp(5.0 * roughness, 0.0, 1.0)));
        vec3 ref_vec = reflect(-view, bent_normal);
#else
        vec3 ref_vec = reflect(-view, normal);
#endif
        ref_vec = mix(ref_vec, normal, roughness * roughness);
        float horizon = min(1.0 + dot(ref_vec, normal), 1.0);
        ref_vec = radiance_inverse_xform * ref_vec;
        specular_light = textureCube(radiance_map, ref_vec).rgb;
        specular_light = srgb_to_linear(specular_light);
        specular_light *= horizon * horizon;
        specular_light *= IBL_exposure_normalization;
    }
#endif // USE_RADIANCE_MAP

    // Calculate Reflection probes
#ifndef DISABLE_REFLECTION_PROBE
    vec4 ambient_accum = vec4(0.0);
    {
        vec4 reflection_accum = vec4(0.0);

        reflection_process(refprobe1_texture, normal, vertex_interp, refprobe1_local_matrix,
                refprobe1_use_box_project, refprobe1_box_extents, refprobe1_box_offset,
                refprobe1_exterior, refprobe1_intensity, refprobe1_ambient_mode, refprobe1_ambient_color,
                roughness, ambient_light, specular_light, reflection_accum, ambient_accum);

#ifdef SECOND_REFLECTION_PROBE
        reflection_process(refprobe2_texture, normal, vertex_interp, refprobe2_local_matrix,
                refprobe2_use_box_project, refprobe2_box_extents, refprobe2_box_offset,
                refprobe2_exterior, refprobe2_intensity, refprobe2_ambient_mode, refprobe2_ambient_color,
                roughness, ambient_light, specular_light, reflection_accum, ambient_accum);
#endif // SECOND_REFLECTION_PROBE

        if (reflection_accum.a > 0.0) {
            specular_light = reflection_accum.rgb / reflection_accum.a;
        }
    }
#endif // DISABLE_REFLECTION_PROBE

#if defined(CUSTOM_RADIANCE_USED)
    specular_light = mix(specular_light, custom_radiance.rgb, custom_radiance.a);
#endif // CUSTOM_RADIANCE_USED

#ifndef USE_LIGHTMAP
    //lightmap overrides everything
    if (use_ambient_light) {
        ambient_light = ambient_light_color_energy.rgb;

#ifdef USE_RADIANCE_MAP
        if (use_ambient_cubemap) {
            vec3 ambient_dir = radiance_inverse_xform * normal;
            vec3 cubemap_ambient = textureCube(radiance_map, ambient_dir).rgb;
            cubemap_ambient = srgb_to_linear(cubemap_ambient);
            ambient_light = mix(ambient_light, cubemap_ambient * ambient_light_color_energy.a, ambient_color_sky_mix);
        }
#endif // USE_RADIANCE_MAP

#ifndef DISABLE_REFLECTION_PROBE
        if (ambient_accum.a > 0.0) {
            ambient_light = mix(ambient_light, (ambient_accum.rgb / ambient_accum.a) * ambient_light_color_energy.a, ambient_color_sky_mix);
        }
#endif // DISABLE_REFLECTION_PROBE
    }
#endif // USE_LIGHTMAP

#if defined(CUSTOM_IRRADIANCE_USED)
    ambient_light = mix(ambient_light, custom_irradiance.rgb, custom_irradiance.a);
#endif // CUSTOM_IRRADIANCE_USED

    // Rest of the lighting calculations...
    // (Directional lights, omni lights, spot lights, etc.)
    // These calculations need to be adapted for GLES2, possibly simplified

#endif // !AMBIENT_LIGHT_DISABLED

#endif // BASE_PASS
#endif // !MODE_UNSHADED

#endif // !MODE_RENDER_DEPTH

    // Final color calculations and output
#ifdef RENDER_MATERIAL
    frag_color[0] = vec4(albedo, alpha);
    frag_color[1] = vec4(normal * 0.5 + 0.5, 0.0);
    frag_color[2] = vec4(ao, roughness, metallic, 1.0);
    frag_color[3] = vec4(emission, 0.0);
#else
#ifdef BASE_PASS
#ifdef MODE_UNSHADED
    frag_color[0] = vec4(albedo, alpha);
#else
    diffuse_light *= albedo;
    diffuse_light *= 1.0 - metallic;
    ambient_light *= 1.0 - metallic;

    frag_color[0] = vec4(diffuse_light + specular_light, alpha);
    frag_color[0].rgb += emission + ambient_light;
#endif //!MODE_UNSHADED

#ifndef FOG_DISABLED
    // GLES2 doesn't support packHalf2x16, so we use the stored fog values directly
    fog.rg = fog_rg;
    fog.ba = fog_ba;

    frag_color[0].rgb = mix(frag_color[0].rgb, fog.rgb, fog.a);
#endif // !FOG_DISABLED

    // Tonemap
    frag_color[0].rgb *= exposure;
#ifdef APPLY_TONEMAPPING
    frag_color[0].rgb = apply_tonemapping(frag_color[0].rgb, white);
#endif
    frag_color[0].rgb = linear_to_srgb(frag_color[0].rgb);

#else // !BASE_PASS
    frag_color[0] = vec4(0.0, 0.0, 0.0, alpha);
#endif // !BASE_PASS

#endif // !RENDER_MATERIAL

#ifdef PREMUL_ALPHA_USED
    frag_color[0].rgb *= premul_alpha;
#endif // PREMUL_ALPHA_USED
}
