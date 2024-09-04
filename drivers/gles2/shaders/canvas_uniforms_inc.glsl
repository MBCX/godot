#define MAX_LIGHTS_PER_ITEM 16

#define M_PI 3.14159265359

#define SDF_MAX_LENGTH 16384.0

#define FLAGS_INSTANCING_MASK 0x7F
#define FLAGS_INSTANCING_HAS_COLORS 128
#define FLAGS_INSTANCING_HAS_CUSTOM_DATA 256

#define FLAGS_CLIP_RECT_UV 512
#define FLAGS_TRANSPOSE_RECT 1024
#define FLAGS_NINEPACH_DRAW_CENTER 4096

#define FLAGS_NINEPATCH_H_MODE_SHIFT 16
#define FLAGS_NINEPATCH_V_MODE_SHIFT 18

#define FLAGS_LIGHT_COUNT_SHIFT 20

#define FLAGS_DEFAULT_NORMAL_MAP_USED 67108864
#define FLAGS_DEFAULT_SPECULAR_MAP_USED 134217728

#define FLAGS_USE_MSDF 268435456
#define FLAGS_USE_LCD 536870912

#define FLAGS_FLIP_H 1073741824
#define FLAGS_FLIP_V 2147483648

// GLES2 doesn't support uniform buffers, so we need to declare individual uniforms
uniform vec4 global_shader_uniforms[MAX_GLOBAL_SHADER_UNIFORMS];

uniform mat4 canvas_transform;
uniform mat4 screen_transform;
uniform mat4 canvas_normal_transform;
uniform vec4 canvas_modulation;
uniform vec2 screen_pixel_size;
uniform float time;
uniform bool use_pixel_snap;

uniform vec4 sdf_to_tex;
uniform vec2 screen_to_sdf;
uniform vec2 sdf_to_screen;

uniform int directional_light_count;
uniform float tex_to_sdf;

#ifndef DISABLE_LIGHTING

#define LIGHT_FLAGS_BLEND_MASK 196608
#define LIGHT_FLAGS_BLEND_MODE_ADD 0
#define LIGHT_FLAGS_BLEND_MODE_SUB 65536
#define LIGHT_FLAGS_BLEND_MODE_MIX 131072
#define LIGHT_FLAGS_BLEND_MODE_MASK 196608
#define LIGHT_FLAGS_HAS_SHADOW 1048576
#define LIGHT_FLAGS_FILTER_SHIFT 22
#define LIGHT_FLAGS_FILTER_MASK 12582912
#define LIGHT_FLAGS_SHADOW_NEAREST 0
#define LIGHT_FLAGS_SHADOW_PCF5 4194304
#define LIGHT_FLAGS_SHADOW_PCF13 8388608

struct Light {
    mat2x4 texture_matrix;
    mat2x4 shadow_matrix;
    vec4 color;

    float shadow_color;
    float flags;
    float shadow_pixel_size;
    float height;

    vec2 position;
    float shadow_zfar_inv;
    float shadow_y_ofs;

    vec4 atlas_rect;
};

uniform Light light_array[MAX_LIGHTS_PER_ITEM];

#endif // DISABLE_LIGHTING