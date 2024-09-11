/* clang-format off */

#[modes]

mode_background =
mode_half_res = #define USE_HALF_RES_PASS
mode_quarter_res = #define USE_QUARTER_RES_PASS
mode_cubemap = #define USE_CUBEMAP_PASS
mode_cubemap_half_res = #define USE_CUBEMAP_PASS \n#define USE_HALF_RES_PASS
mode_cubemap_quarter_res = #define USE_CUBEMAP_PASS \n#define USE_QUARTER_RES_PASS

#[specializations]

USE_MULTIVIEW = false
USE_INVERTED_Y = true
APPLY_TONEMAPPING = true

#[vertex]

attribute vec2 vertex_attrib;

varying vec2 uv_interp;

/* clang-format on */

void main() {
#ifdef USE_INVERTED_Y
    uv_interp = vertex_attrib;
#else
    // We're doing clockwise culling so flip the order
    uv_interp = vec2(vertex_attrib.x, vertex_attrib.y * -1.0);
#endif
    gl_Position = vec4(uv_interp, -1.0, 1.0);
}

/* clang-format off */

#[fragment]

#define M_PI 3.14159265359

#include "tonemap_inc.glsl"

precision mediump float;
precision mediump int;

varying vec2 uv_interp;

/* clang-format on */

uniform samplerCube radiance; //texunit:-1

#ifdef USE_CUBEMAP_PASS
uniform samplerCube half_res; //texunit:-2
uniform samplerCube quarter_res; //texunit:-3
#elif defined(USE_MULTIVIEW)
uniform sampler2DArray half_res; //texunit:-2
uniform sampler2DArray quarter_res; //texunit:-3
#else
uniform sampler2D half_res; //texunit:-2
uniform sampler2D quarter_res; //texunit:-3
#endif

uniform vec4 global_shader_uniforms[MAX_GLOBAL_SHADER_UNIFORMS];

struct DirectionalLightData {
    vec4 direction_energy;
    vec4 color_size;
    bool enabled;
};

uniform DirectionalLightData directional_lights_data[MAX_DIRECTIONAL_LIGHT_DATA_STRUCTS];

/* clang-format off */

#ifdef MATERIAL_UNIFORMS_USED
#MATERIAL_UNIFORMS
#endif

/* clang-format on */

#GLOBALS

#ifdef USE_CUBEMAP_PASS
#define AT_CUBEMAP_PASS true
#else
#define AT_CUBEMAP_PASS false
#endif

#ifdef USE_HALF_RES_PASS
#define AT_HALF_RES_PASS true
#else
#define AT_HALF_RES_PASS false
#endif

#ifdef USE_QUARTER_RES_PASS
#define AT_QUARTER_RES_PASS true
#else
#define AT_QUARTER_RES_PASS false
#endif

uniform mat4 orientation;
uniform vec4 projection;
uniform vec3 position;
uniform float time;
uniform float sky_energy_multiplier;
uniform float luminance_multiplier;

uniform float fog_aerial_perspective;
uniform vec4 fog_light_color;
uniform float fog_sun_scatter;
uniform bool fog_enabled;
uniform float fog_density;
uniform float fog_sky_affect;
uniform int directional_light_count;

#ifdef USE_MULTIVIEW
uniform mat4 projection_matrix_view[MAX_VIEWS];
uniform mat4 inv_projection_matrix_view[MAX_VIEWS];
uniform vec4 eye_offset[MAX_VIEWS];
#endif

#ifdef USE_DEBANDING
vec3 interleaved_gradient_noise(vec2 pos) {
    const vec3 magic = vec3(0.06711056, 0.00583715, 52.9829189);
    float res = fract(magic.z * fract(dot(pos, magic.xy))) * 2.0 - 1.0;
    return vec3(res, -res, res) / 255.0;
}
#endif

#if !defined(DISABLE_FOG)
vec4 fog_process(vec3 view, vec3 sky_color) {
    vec3 fog_color = mix(fog_light_color.rgb, sky_color, fog_aerial_perspective);

    if (fog_sun_scatter > 0.001) {
        vec4 sun_scatter = vec4(0.0);
        float sun_total = 0.0;
        for (int i = 0; i < directional_light_count; i++) {
            vec3 light_color = directional_lights_data[i].color_size.xyz * directional_lights_data[i].direction_energy.w;
            float light_amount = pow(max(dot(view, directional_lights_data[i].direction_energy.xyz), 0.0), 8.0);
            fog_color += light_color * light_amount * fog_sun_scatter;
        }
    }

    return vec4(fog_color, 1.0);
}
#endif // !DISABLE_FOG

void main() {
    vec3 cube_normal;

#ifdef USE_MULTIVIEW
    // In multiview our projection matrices will contain positional and rotational offsets that we need to properly unproject.
    vec4 unproject = vec4(uv_interp.xy, -1.0, 1.0); // unproject at the far plane
    vec4 unprojected = inv_projection_matrix_view[ViewIndex] * unproject;
    cube_normal = unprojected.xyz / unprojected.w;
    // Unproject will give us the position between the eyes, need to re-offset.
    cube_normal += eye_offset[ViewIndex].xyz;
#else
    cube_normal.z = -1.0;
    cube_normal.x = (uv_interp.x + projection.x) / projection.y;
    cube_normal.y = (-uv_interp.y - projection.z) / projection.w;
#endif

    cube_normal = mat3(orientation) * cube_normal;
    cube_normal = normalize(cube_normal);

    vec2 uv = gl_FragCoord.xy; // uv_interp * 0.5 + 0.5;

    vec2 panorama_coords = vec2(atan(cube_normal.x, -cube_normal.z), acos(cube_normal.y));

    if (panorama_coords.x < 0.0) {
        panorama_coords.x += M_PI * 2.0;
    }

    panorama_coords /= vec2(M_PI * 2.0, M_PI);

    vec3 color = vec3(0.0, 0.0, 0.0);
    float alpha = 1.0; // Only available to subpasses
    vec4 half_res_color = vec4(1.0);
    vec4 quarter_res_color = vec4(1.0);
    vec4 custom_fog = vec4(0.0);

#ifdef USE_CUBEMAP_PASS
#ifdef USES_HALF_RES_COLOR
    half_res_color = textureCube(half_res, cube_normal);
#endif
#ifdef USES_QUARTER_RES_COLOR
    quarter_res_color = textureCube(quarter_res, cube_normal);
#endif
#else
#ifdef USES_HALF_RES_COLOR
#ifdef USE_MULTIVIEW
    half_res_color = texture2D(half_res, vec3(uv, float(ViewIndex)));
#else
    half_res_color = texture2D(half_res, uv);
#endif
#endif
#ifdef USES_QUARTER_RES_COLOR
#ifdef USE_MULTIVIEW
    quarter_res_color = texture2D(quarter_res, vec3(uv, float(ViewIndex)));
#else
    quarter_res_color = texture2D(quarter_res, uv);
#endif
#endif
#endif

    {
#CODE : SKY
    }

    color *= sky_energy_multiplier;

    // Convert to Linear for tonemapping so color matches scene shader better
    color = srgb_to_linear(color);

#if !defined(DISABLE_FOG) && !defined(USE_CUBEMAP_PASS)
    // Draw "fixed" fog before volumetric fog to ensure volumetric fog can appear in front of the sky.
    if (fog_enabled) {
        vec4 fog = fog_process(cube_normal, color.rgb);
        color.rgb = mix(color.rgb, fog.rgb, fog.a * fog_sky_affect);
    }

    if (custom_fog.a > 0.0) {
        color.rgb = mix(color.rgb, custom_fog.rgb, custom_fog.a);
    }
#endif // DISABLE_FOG

    color *= exposure;

#ifdef APPLY_TONEMAPPING
    color = apply_tonemapping(color, white);
#endif

    color = linear_to_srgb(color);

    gl_FragColor.rgb = color * luminance_multiplier;
    gl_FragColor.a = alpha;

#ifdef USE_DEBANDING
    gl_FragColor.rgb += interleaved_gradient_noise(gl_FragCoord.xy) * sky_energy_multiplier * luminance_multiplier;
#endif
}
