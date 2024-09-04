// Modes
#define MODE_QUAD 0
#define MODE_NINEPATCH 1
#define MODE_PRIMITIVE 2
#define MODE_ATTRIBUTES 3

// Specializations
// Note: In GLES2, we'll use preprocessor definitions instead of specialization constants
#define DISABLE_LIGHTING
// #define USE_RGBA_SHADOWS
// #define SINGLE_INSTANCE

#[vertex]
attribute vec2 vertex_attrib;
attribute vec4 color_attrib;
attribute vec2 uv_attrib;

#if defined(USE_INSTANCING)
attribute vec4 instance_xform0;
attribute vec4 instance_xform1;
attribute vec4 instance_color;
#endif

#if defined(USE_PRIMITIVE)
attribute vec2 point_a;
attribute vec2 point_b;
attribute vec2 point_c;
attribute vec2 uv_a;
attribute vec2 uv_b;
attribute vec2 uv_c;
attribute vec4 color_a;
attribute vec4 color_b;
attribute vec4 color_c;
#endif

#ifdef USE_CUSTOM0
attribute vec4 custom0_attrib;
#endif

#ifdef USE_CUSTOM1
attribute vec4 custom1_attrib;
#endif

// Replace input attributes with uniforms
uniform vec4 draw_data_world_x;
uniform vec4 draw_data_world_y;
uniform vec2 draw_data_world_ofs;
uniform vec2 draw_data_color_texture_pixel_size;
uniform vec4 draw_data_ninepatch_margins;
uniform vec4 draw_data_dst_rect;
uniform vec4 draw_data_src_rect;
uniform int draw_data_flags;
uniform float draw_data_specular_shininess;
uniform int draw_data_lights[4];

varying vec2 uv_interp;
varying vec4 color_interp;
varying vec2 vertex_interp;

#ifdef USE_NINEPATCH
varying vec2 pixel_size_interp;
#endif

#include "canvas_uniforms_inc.glsl"

// Add any additional uniform declarations here

void main() {
    vec4 instance_custom = vec4(0.0);

#ifdef USE_PRIMITIVE
    vec2 vertex;
    vec2 uv;
    vec4 color;

    if (gl_VertexID == 0) {
        vertex = point_a;
        uv = uv_a;
        color = color_a;
    } else if (gl_VertexID == 1) {
        vertex = point_b;
        uv = uv_b;
        color = color_b;
    } else {
        vertex = point_c;
        uv = uv_c;
        color = color_c;
    }
#elif defined(USE_ATTRIBUTES)
    vec2 vertex = vertex_attrib;
    vec4 color = color_attrib * draw_data_modulation;
    vec2 uv = uv_attrib;
#ifdef USE_INSTANCING
    color *= instance_color;
    instance_custom = instance_color;
#endif
#else
    vec2 vertex_base;
    if (gl_VertexID == 0) {
        vertex_base = vec2(0.0, 0.0);
    } else if (gl_VertexID == 1) {
        vertex_base = vec2(0.0, 1.0);
    } else if (gl_VertexID == 2) {
        vertex_base = vec2(1.0, 1.0);
    } else if (gl_VertexID == 3) {
        vertex_base = vec2(1.0, 0.0);
    }
    
    vec2 uv = draw_data_src_rect.xy + abs(draw_data_src_rect.zw) * ((draw_data_flags & FLAGS_TRANSPOSE_RECT) != 0 ? vertex_base.yx : vertex_base.xy);
    vec4 color = draw_data_modulation;
    vec2 vertex = draw_data_dst_rect.xy + abs(draw_data_dst_rect.zw) * mix(vertex_base, vec2(1.0, 1.0) - vertex_base, lessThan(draw_data_src_rect.zw, vec2(0.0, 0.0)));
#endif


    mat4 model_matrix = mat4(vec4(draw_data_world_x.xy, 0.0, 0.0),
                             vec4(draw_data_world_y.xy, 0.0, 0.0),
                             vec4(0.0, 0.0, 1.0, 0.0),
                             vec4(draw_data_world_ofs, 0.0, 1.0));

#ifdef USE_INSTANCING
    model_matrix = model_matrix * mat4(instance_xform0, instance_xform1, vec4(0.0, 0.0, 1.0, 0.0), vec4(0.0, 0.0, 0.0, 1.0));
#endif

    vec2 color_texture_pixel_size = draw_data_color_texture_pixel_size;

#ifdef USE_POINT_SIZE
    float point_size = 1.0;
#endif

#ifdef USE_TEXTURE_RECT
    if ((draw_data_flags & FLAGS_CLIP_RECT_UV) != 0) {
        uv = clamp(uv, draw_data_src_rect.xy, draw_data_src_rect.xy + abs(draw_data_src_rect.zw));
    }
#endif

    {
#CODE : VERTEX
    }

#ifdef USE_NINEPATCH
    pixel_size_interp = abs(draw_data_dst_rect.zw) * vertex_base;
#endif

    vertex = (model_matrix * vec4(vertex, 0.0, 1.0)).xy;
    vertex_interp = vertex;
    color_interp = color;

    if (use_pixel_snap) {
        vertex = floor(vertex + 0.5);
    }

    uv_interp = uv;
    vertex = (canvas_transform * vec4(vertex, 0.0, 1.0)).xy;
    gl_Position = screen_transform * vec4(vertex, 0.0, 1.0);

#ifdef USE_POINT_SIZE
    gl_PointSize = point_size;
#endif
}

#[fragment]

#ifdef GL_ES
precision mediump float;
#endif

varying vec2 uv_interp;
varying vec4 color_interp;
varying vec2 vertex_interp;

#ifdef USE_NINEPATCH
varying vec2 pixel_size_interp;
#endif

uniform sampler2D color_texture;
uniform highp vec2 color_texpixel_size;

#ifndef DISABLE_LIGHTING

uniform sampler2D normal_texture;
uniform sampler2D specular_texture;

#ifdef USE_RGBA_SHADOWS
uniform highp sampler2D shadow_texture;
#else
uniform highp sampler2DShadow shadow_texture;
#endif

#endif // DISABLE_LIGHTING

uniform float shadow_pixel_size;
uniform vec4 shadow_color;
uniform float light_height;
uniform float light_outside_alpha;

uniform vec2 screen_uv_to_sdf_uv;
uniform vec2 sdf_to_screen_uv;
uniform vec2 sdf_to_tex_pixel;
uniform float sdf_max_distance;
uniform highp sampler2D sdf_texture;

vec2 screen_uv_to_sdf(vec2 p_uv) {
    return p_uv * screen_uv_to_sdf_uv;
}

vec2 sdf_to_screen_uv(vec2 p_uv) {
    return p_uv * sdf_to_screen_uv;
}

// Add any additional uniform declarations here

void main() {
    vec4 color = color_interp;
    vec2 uv = uv_interp;
    vec2 vertex = vertex_interp;

#ifndef USE_ATTRIBUTES
#ifdef USE_NINEPATCH
    // Nine patch calculations
#endif
#endif

    // Texture sampling
    vec4 texture_color = texture2D(color_texture, uv);
    color *= texture_color;

#ifndef DISABLE_LIGHTING
    // Normal mapping
    vec3 normal;
#ifdef USE_NORMALMAP
    normal.xy = texture2D(normal_texture, uv).xy * 2.0 - 1.0;
    normal.z = sqrt(1.0 - dot(normal.xy, normal.xy));
#else
    normal = vec3(0.0, 0.0, 1.0);
#endif

    // Specular mapping
    float specular_shininess = draw_data_specular_shininess;
#ifdef USE_SPECULARMAP
    vec4 specular_texture_color = texture2D(specular_texture, uv);
    specular_shininess *= specular_texture_color.r;
#endif

    // Lighting calculations
    vec3 light_dir = normalize(vec3(0.0, 0.0, 1.0));
    float ndotl = max(dot(normal, light_dir), 0.0);
    vec3 view_dir = normalize(vec3(0.0, 0.0, 1.0));
    vec3 half_dir = normalize(light_dir + view_dir);
    float ndoth = max(dot(normal, half_dir), 0.0);

    vec3 diffuse = vec3(ndotl);
    vec3 specular = vec3(pow(ndoth, specular_shininess));

    // Shadow calculations
#ifdef USE_SHADOWS
    vec4 shadow_coord = (draw_data_shadow_matrix * vec4(vertex, 0.0, 1.0));
    shadow_coord.xyz /= shadow_coord.w;
    float shadow = 1.0;

#ifdef USE_RGBA_SHADOWS
    float depth = texture2D(shadow_texture, shadow_coord.xy).r;
    shadow = step(shadow_coord.z, depth);
#else
    shadow = shadow2D(shadow_texture, shadow_coord.xyz).r;
#endif

    diffuse *= mix(shadow_color.rgb, vec3(1.0), shadow);
#endif // USE_SHADOWS

#CODE : LIGHT

    // Apply lighting
    color.rgb *= diffuse;
    color.rgb += specular * specular_shininess;

#endif // DISABLE_LIGHTING

    // Custom fragment code insertion point
    {
#CODE : FRAGMENT
    }

#ifdef USE_SDF
    // SDF calculations
    vec2 sdf_uv = screen_uv_to_sdf(gl_FragCoord.xy * color_texpixel_size);
    float sdf_distance = texture2D(sdf_texture, sdf_uv).r * sdf_max_distance - sdf_max_distance;
    float sdf_alpha = clamp(0.5 - sdf_distance, 0.0, 1.0);
    color.a *= sdf_alpha;
#endif

#ifdef USE_PREMULTIPLIED_ALPHA
    color.rgb *= color.a;
#endif

    gl_FragColor = color;
}