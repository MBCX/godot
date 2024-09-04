/* clang-format off */
#[modes]
mode_default =

#[vertex]
attribute vec2 vertex_attrib;

varying vec2 uv_interp;

void main() {
    uv_interp = vertex_attrib * 0.5 + 0.5;
    gl_Position = vec4(vertex_attrib, 1.0, 1.0);
}

#[fragment]
precision mediump float;

// If we reach this code, we always tonemap.
#define APPLY_TONEMAPPING

// Include tonemap functions (you'll need to adapt these for GLES2)
#include "../tonemap_inc.glsl"

uniform sampler2D source_color;

#ifdef USE_GLOW
uniform sampler2D glow_color;
uniform vec2 pixel_size;
uniform float glow_intensity;
#endif

#ifdef USE_BCS
uniform float brightness;
uniform float contrast;
uniform float saturation;
#endif

#ifdef USE_COLOR_CORRECTION
#ifdef USE_1D_LUT
uniform sampler2D source_color_correction;
#else
uniform sampler2D source_color_correction;
#endif
#endif

varying vec2 uv_interp;

// Helper functions (you'll need to implement these)
vec3 srgb_to_linear(vec3 color) {
    return color * (color * (color * 0.305306011 + 0.682171111) + 0.012522878);
}

vec3 linear_to_srgb(vec3 color) {
    return max(vec3(1.055) * pow(color, vec3(0.416666667)) - vec3(0.055), vec3(0.0));
}

vec3 apply_tonemapping(vec3 color, float white) {
    // Implement tonemapping for GLES2
    return color / (color + vec3(1.0));
}

#ifdef USE_GLOW
vec4 get_glow_color(vec2 uv) {
    vec2 half_pixel = pixel_size * 0.5;

    vec4 color = texture2D(glow_color, uv + vec2(-half_pixel.x * 2.0, 0.0));
    color += texture2D(glow_color, uv + vec2(-half_pixel.x, half_pixel.y)) * 2.0;
    color += texture2D(glow_color, uv + vec2(0.0, half_pixel.y * 2.0));
    color += texture2D(glow_color, uv + vec2(half_pixel.x, half_pixel.y)) * 2.0;
    color += texture2D(glow_color, uv + vec2(half_pixel.x * 2.0, 0.0));
    color += texture2D(glow_color, uv + vec2(half_pixel.x, -half_pixel.y)) * 2.0;
    color += texture2D(glow_color, uv + vec2(0.0, -half_pixel.y * 2.0));
    color += texture2D(glow_color, uv + vec2(-half_pixel.x, -half_pixel.y)) * 2.0;

    return color / 12.0;
}
#endif

#ifdef USE_COLOR_CORRECTION
vec3 apply_color_correction(vec3 color) {
#ifdef USE_1D_LUT
    color.r = texture2D(source_color_correction, vec2(color.r, 0.0)).r;
    color.g = texture2D(source_color_correction, vec2(color.g, 0.0)).g;
    color.b = texture2D(source_color_correction, vec2(color.b, 0.0)).b;
#else
    color = texture2D(source_color_correction, color.rg).rgb;
#endif
    return color;
}
#endif

#ifdef USE_BCS
vec3 apply_bcs(vec3 color) {
    color = mix(vec3(0.0), color, brightness);
    color = mix(vec3(0.5), color, contrast);
    color = mix(vec3(dot(vec3(1.0), color) * 0.33333), color, saturation);

    return color;
}
#endif

void main() {
    vec4 color = texture2D(source_color, uv_interp);

#ifdef USE_GLOW
    vec4 glow = get_glow_color(uv_interp) * glow_intensity;

    // Just use softlight...
    glow.rgb = clamp(glow.rgb, vec3(0.0), vec3(1.0));
    color.rgb = max((color.rgb + glow.rgb) - (color.rgb * glow.rgb), vec3(0.0));
#endif

#ifdef USE_LUMINANCE_MULTIPLIER
    color = color / luminance_multiplier;
#endif

    color.rgb = srgb_to_linear(color.rgb);
    color.rgb = apply_tonemapping(color.rgb, 1.0); // Assuming white = 1.0
    color.rgb = linear_to_srgb(color.rgb);

#ifdef USE_BCS
    color.rgb = apply_bcs(color.rgb);
#endif

#ifdef USE_COLOR_CORRECTION
    color.rgb = apply_color_correction(color.rgb);
#endif

    gl_FragColor = color;
}