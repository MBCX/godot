/* clang-format off */
#[modes]

// Based on Dual filtering glow as explained in Marius Bjørge presentation at Siggraph 2015 "Bandwidth-Efficient Rendering"

mode_filter = #define MODE_FILTER
mode_downsample = #define MODE_DOWNSAMPLE
mode_upsample = #define MODE_UPSAMPLE

#[vertex]
attribute vec2 vertex_attrib;

varying vec2 uv_interp;

void main() {
    uv_interp = vertex_attrib * 0.5 + 0.5;
    gl_Position = vec4(vertex_attrib, 1.0, 1.0);
}

#[fragment]
precision mediump float;

#ifdef MODE_FILTER
uniform sampler2D source_color;
uniform float view;
uniform vec2 pixel_size;
uniform float luminance_multiplier;
uniform float glow_bloom;
uniform float glow_hdr_threshold;
uniform float glow_hdr_scale;
uniform float glow_luminance_cap;
#endif

#ifdef MODE_DOWNSAMPLE
uniform sampler2D source_color;
uniform vec2 pixel_size;
#endif

#ifdef MODE_UPSAMPLE
uniform sampler2D source_color;
uniform vec2 pixel_size;
#endif

varying vec2 uv_interp;

void main() {
#ifdef MODE_FILTER
    vec2 half_pixel = pixel_size * 0.5;
    vec2 uv = uv_interp;
    vec3 color = texture2D(source_color, uv).rgb * 4.0;
    color += texture2D(source_color, uv - half_pixel).rgb;
    color += texture2D(source_color, uv + half_pixel).rgb;
    color += texture2D(source_color, uv - vec2(half_pixel.x, -half_pixel.y)).rgb;
    color += texture2D(source_color, uv + vec2(half_pixel.x, -half_pixel.y)).rgb;

    color /= luminance_multiplier * 8.0;

    float feedback_factor = max(color.r, max(color.g, color.b));
    float feedback = max(smoothstep(glow_hdr_threshold, glow_hdr_threshold + glow_hdr_scale, feedback_factor), glow_bloom);

    color = min(color * feedback, vec3(glow_luminance_cap));

    gl_FragColor = vec4(luminance_multiplier * color, 1.0);
#endif

#ifdef MODE_DOWNSAMPLE
    vec2 half_pixel = pixel_size * 0.5;
    vec4 color = texture2D(source_color, uv_interp) * 4.0;
    color += texture2D(source_color, uv_interp - half_pixel);
    color += texture2D(source_color, uv_interp + half_pixel);
    color += texture2D(source_color, uv_interp - vec2(half_pixel.x, -half_pixel.y));
    color += texture2D(source_color, uv_interp + vec2(half_pixel.x, -half_pixel.y));
    gl_FragColor = color / 8.0;
#endif

#ifdef MODE_UPSAMPLE
    vec2 half_pixel = pixel_size * 0.5;

    vec4 color = texture2D(source_color, uv_interp + vec2(-half_pixel.x * 2.0, 0.0));
    color += texture2D(source_color, uv_interp + vec2(-half_pixel.x, half_pixel.y)) * 2.0;
    color += texture2D(source_color, uv_interp + vec2(0.0, half_pixel.y * 2.0));
    color += texture2D(source_color, uv_interp + vec2(half_pixel.x, half_pixel.y)) * 2.0;
    color += texture2D(source_color, uv_interp + vec2(half_pixel.x * 2.0, 0.0));
    color += texture2D(source_color, uv_interp + vec2(half_pixel.x, -half_pixel.y)) * 2.0;
    color += texture2D(source_color, uv_interp + vec2(0.0, -half_pixel.y * 2.0));
    color += texture2D(source_color, uv_interp + vec2(-half_pixel.x, -half_pixel.y)) * 2.0;

    gl_FragColor = color / 12.0;
#endif
}