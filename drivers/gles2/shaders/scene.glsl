/* clang-format off */
#[modes]

mode_color =
mode_color_instancing = \n#define USE_INSTANCING
mode_depth = #define MODE_RENDER_DEPTH
mode_depth_instancing = #define MODE_RENDER_DEPTH \n#define USE_INSTANCING

#[specializations]

DISABLE_LIGHTMAP = false
DISABLE_LIGHT_DIRECTIONAL = false
DISABLE_LIGHT_OMNI = false
DISABLE_LIGHT_SPOT = false
DISABLE_REFLECTION_PROBE = true
DISABLE_FOG = false
USE_DEPTH_FOG = false
USE_RADIANCE_MAP = true
USE_LIGHTMAP = false
USE_SH_LIGHTMAP = false
USE_LIGHTMAP_CAPTURE = false
USE_MULTIVIEW = false
RENDER_SHADOWS = false
RENDER_SHADOWS_LINEAR = false
SHADOW_MODE_PCF_5 = false
SHADOW_MODE_PCF_13 = false
LIGHT_USE_PSSM2 = false
LIGHT_USE_PSSM4 = false
LIGHT_USE_PSSM_BLEND = false
BASE_PASS = true
USE_ADDITIVE_LIGHTING = false
APPLY_TONEMAPPING = true
// We can only use one type of light per additive pass. This means that if USE_ADDITIVE_LIGHTING is defined, and
// these are false, we are doing a directional light pass.
ADDITIVE_OMNI = false
ADDITIVE_SPOT = false
RENDER_MATERIAL = false
SECOND_REFLECTION_PROBE = false
LIGHTMAP_BICUBIC_FILTER = false

#[vertex]

#define M_PI 3.14159265359

#define SHADER_IS_SRGB true
#define SHADER_SPACE_FAR -1.0

precision highp float;
precision highp int;

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

#if defined(COLOR_USED)
attribute vec4 color_attrib;
#endif

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

#ifdef USE_INSTANCING
attribute vec4 instance_xform0;
attribute vec4 instance_xform1;
attribute vec4 instance_xform2;
attribute vec4 instance_color_custom_data;
#endif

uniform highp mat4 projection_matrix;
uniform highp mat4 modelview_matrix;
uniform highp mat4 extra_matrix;
uniform highp vec3 compressed_aabb_position;
uniform highp vec3 compressed_aabb_size;
uniform highp vec4 uv_scale;

uniform highp float time;

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

#GLOBALS

/* clang-format on */

void main() {
    highp vec3 vertex = vertex_angle_attrib.xyz * compressed_aabb_size + compressed_aabb_position;

    highp mat4 model_matrix = extra_matrix;
#ifdef USE_INSTANCING
    highp mat4 m = mat4(instance_xform0, instance_xform1, instance_xform2, vec4(0.0, 0.0, 0.0, 1.0));
    model_matrix = model_matrix * transpose(m);
#endif

#ifdef NORMAL_USED
    vec3 normal = normalize((axis_tangent_attrib.xyz * 2.0 - 1.0) * compressed_aabb_size);
#endif

    highp mat3 model_normal_matrix = mat3(model_matrix);

#if defined(NORMAL_USED) || defined(TANGENT_USED) || defined(NORMAL_MAP_USED) || defined(LIGHT_ANISOTROPY_USED)
    vec3 binormal;
    vec3 tangent;
    float binormal_sign;
    float angle = vertex_angle_attrib.w;
    vec3 axis = axis_tangent_attrib.xyz;
    binormal_sign = axis_tangent_attrib.w * 2.0 - 1.0;
    angle *= binormal_sign;
    float s = sin(angle);
    float c = cos(angle);
    float t = 1.0 - c;
    axis = normalize(axis);
    binormal = normalize(vec3(
        axis.x * axis.x * t + c,
        axis.y * axis.x * t - axis.z * s,
        axis.z * axis.x * t + axis.y * s
    ));
    tangent = normalize(vec3(
        axis.x * axis.y * t + axis.z * s,
        axis.y * axis.y * t + c,
        axis.z * axis.y * t - axis.x * s
    ));
#endif

#if defined(COLOR_USED)
    color_interp = color_attrib;
#ifdef USE_INSTANCING
    color_interp *= instance_color_custom_data;
#endif
#endif

#if defined(UV_USED)
    uv_interp = uv_attrib;
#endif

#if defined(UV2_USED) || defined(USE_LIGHTMAP)
    uv2_interp = uv2_attrib;
#endif

    if (uv_scale != vec4(0.0)) {
#ifdef UV_USED
        uv_interp = (uv_interp - 0.5) * uv_scale.xy + 0.5;
#endif
#if defined(UV2_USED) || defined(USE_LIGHTMAP)
        uv2_interp = (uv2_interp - 0.5) * uv_scale.zw + 0.5;
#endif
    }

    highp vec4 outvec = vec4(vertex, 1.0);

    // Using world coordinates
#if !defined(SKIP_TRANSFORM_USED) && defined(VERTEX_WORLD_COORDS_USED)
    vertex = (model_matrix * vec4(vertex, 1.0)).xyz;
#ifdef NORMAL_USED
    normal = model_normal_matrix * normal;
#endif
#if defined(TANGENT_USED) || defined(NORMAL_MAP_USED) || defined(LIGHT_ANISOTROPY_USED)
    binormal = model_normal_matrix * binormal;
    tangent = model_normal_matrix * tangent;
#endif
#endif

    float roughness = 1.0;

    {
#CODE : VERTEX
    }

    // Using local coordinates (default)
#if !defined(SKIP_TRANSFORM_USED) && !defined(VERTEX_WORLD_COORDS_USED)
    vertex = (modelview_matrix * (model_matrix * vec4(vertex, 1.0))).xyz;
#ifdef NORMAL_USED
    normal = normalize((modelview_matrix * (model_matrix * vec4(normal, 0.0))).xyz);
#endif
#if defined(TANGENT_USED) || defined(NORMAL_MAP_USED) || defined(LIGHT_ANISOTROPY_USED)
    binormal = normalize((modelview_matrix * (model_matrix * vec4(binormal, 0.0))).xyz);
    tangent = normalize((modelview_matrix * (model_matrix * vec4(tangent, 0.0))).xyz);
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

#ifdef USE_ADDITIVE_LIGHTING
    // Calculate shadows
    // This part might need adjustments based on your specific shadow implementation
#endif

#if defined(RENDER_SHADOWS) && !defined(RENDER_SHADOWS_LINEAR)
    vertex_interp += vertex_interp * 0.00001;
#endif

    gl_Position = projection_matrix * vec4(vertex_interp, 1.0);

#ifdef RENDER_MATERIAL
    gl_Position.xy = (uv2_attrib * 2.0 - 1.0);
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

precision mediump float;
precision mediump int;

#define M_PI 3.14159265359

#define SHADER_IS_SRGB true
#define SHADER_SPACE_FAR -1.0

uniform sampler2D color_texture; // texunit:0
uniform mediump vec2 color_texpixel_size;

#if defined(SCREEN_TEXTURE_USED)
uniform sampler2D screen_texture; // texunit:-4
#endif

#if defined(SCREEN_UV_USED)
uniform vec2 screen_pixel_size;
#endif

#ifdef USE_LIGHTING
uniform lowp sampler2D light_texture; // texunit:-6
#ifdef USE_SHADOWS
uniform highp sampler2D shadow_texture; // texunit:-5
#endif
#endif

varying mediump vec2 uv_interp;
varying mediump vec4 color_interp;

#ifdef NORMAL_USED
varying mediump vec3 normal_interp;
#endif

#if defined(TANGENT_USED) || defined(NORMAL_MAP_USED) || defined(LIGHT_ANISOTROPY_USED)
varying mediump vec3 tangent_interp;
varying mediump vec3 binormal_interp;
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
#endif
#endif

uniform highp mat4 camera_matrix;
uniform highp mat4 camera_inverse_matrix;
uniform highp mat4 projection_matrix;
uniform highp mat4 projection_inverse_matrix;

uniform highp float time;

uniform bool use_default_normal;

/* clang-format off */

#GLOBALS

/* clang-format on */

void light_compute(
        inout vec3 light,
        vec3 view,
        vec3 normal,
        vec3 albedo,
        vec3 light_dir,
        vec3 attenuation,
        vec3 diffuse,
        vec3 specular,
        float roughness) {
    float NdotL = max(dot(normal, light_dir), 0.0);
    
    // Diffuse
    light += albedo * diffuse * NdotL;
    
    // Specular (Blinn-Phong)
    vec3 half_dir = normalize(view + light_dir);
    float NdotH = max(dot(normal, half_dir), 0.0);
    float spec = pow(NdotH, (1.0 - roughness) * 256.0);
    light += specular * spec;
    
    light *= attenuation;
}

void main() {
    vec4 albedo = color_interp;
    vec2 uv = uv_interp;
    
    albedo *= texture2D(color_texture, uv);
    
    vec3 normal = vec3(0.0, 0.0, 1.0);
    
#ifdef NORMAL_USED
    normal = normalize(normal_interp);
#endif
    
    if (use_default_normal) {
        normal = vec3(0.0, 0.0, 1.0);
    }
    
    {
#CODE : FRAGMENT
    }
    
#ifndef MODE_RENDER_DEPTH
    vec3 view = -normalize(vertex_interp);

#ifdef USE_LIGHTING
    vec3 light = vec3(0.0);
    vec3 ambient = vec3(0.1);  // Ambient light

    // Directional light
    vec3 light_dir = normalize(vec3(1.0, 1.0, -1.0));  // Example light direction
    vec3 light_color = vec3(1.0, 1.0, 1.0);  // White light
    float attenuation = 1.0;  // No attenuation for directional light

    light_compute(light, view, normal, albedo.rgb, light_dir, vec3(attenuation), light_color, light_color, 0.5);

    // Add ambient light
    light += albedo.rgb * ambient;

#ifdef USE_SHADOWS
    // Simple shadow calculation
    float shadow = texture2DProj(shadow_texture, shadow_coord).r;
    light *= shadow;
#endif

    gl_FragColor = vec4(light, albedo.a);

#else // !USE_LIGHTING
    gl_FragColor = albedo;
#endif // USE_LIGHTING

#ifndef FOG_DISABLED
    // Simple distance fog
    float fog_distance = length(vertex_interp);
    float fog_amount = 1.0 - exp(-0.002 * fog_distance);
    vec3 fog_color = vec3(0.5, 0.6, 0.7);  // Example fog color
    gl_FragColor.rgb = mix(gl_FragColor.rgb, fog_color, fog_amount);
#endif // !FOG_DISABLED

#ifdef APPLY_TONEMAPPING
    // Simple tonemapping (Reinhard operator)
    gl_FragColor.rgb = gl_FragColor.rgb / (gl_FragColor.rgb + vec3(1.0));
#endif // APPLY_TONEMAPPING

    // Convert from linear to sRGB color space
    gl_FragColor.rgb = pow(gl_FragColor.rgb, vec3(1.0 / 2.2));

#else // MODE_RENDER_DEPTH
    // Render depth
    float depth = gl_FragCoord.z;
    gl_FragColor = vec4(depth, depth, depth, 1.0);
#endif // !MODE_RENDER_DEPTH
}