#[vertex]
attribute vec2 vertex_attrib;
attribute vec2 uv_in;

varying vec2 uv_interp;

#ifdef USE_BLUR_SECTION
uniform vec4 blur_section;
#endif

void main() {
    uv_interp = uv_in;
    gl_Position = vec4(vertex_attrib, 0.0, 1.0);
#ifdef USE_BLUR_SECTION
    uv_interp = blur_section.xy + uv_interp * blur_section.zw;
    gl_Position.xy = (blur_section.xy + (gl_Position.xy * 0.5 + 0.5) * blur_section.zw) * 2.0 - 1.0;
#endif
}

#[fragment]
#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif

uniform sampler2D source_color;

uniform float lod;
uniform vec2 pixel_size;

#if defined(GLOW_GAUSSIAN_HORIZONTAL) || defined(GLOW_GAUSSIAN_VERTICAL)
uniform float glow_strength;
#endif

#if defined(DOF_FAR_BLUR) || defined(DOF_NEAR_BLUR)
uniform sampler2D dof_source_depth;
uniform float dof_begin;
uniform float dof_end;
uniform vec2 dof_dir;
uniform float dof_radius;
#endif

#ifdef GLOW_FIRST_PASS
uniform float luminance_cap;
uniform float glow_bloom;
uniform float glow_hdr_threshold;
uniform float glow_hdr_scale;
#endif

uniform float camera_z_far;
uniform float camera_z_near;

varying vec2 uv_interp;

void main() {
    vec4 color = vec4(0.0);
    
#ifdef GLOW_GAUSSIAN_HORIZONTAL
    vec2 pix_size = pixel_size;
    pix_size *= 0.5;
    color = texture2D(source_color, uv_interp) * 0.174938;
    color += texture2D(source_color, uv_interp + vec2(1.0, 0.0) * pix_size) * 0.165569;
    color += texture2D(source_color, uv_interp + vec2(2.0, 0.0) * pix_size) * 0.140367;
    color += texture2D(source_color, uv_interp + vec2(3.0, 0.0) * pix_size) * 0.106595;
    color += texture2D(source_color, uv_interp + vec2(-1.0, 0.0) * pix_size) * 0.165569;
    color += texture2D(source_color, uv_interp + vec2(-2.0, 0.0) * pix_size) * 0.140367;
    color += texture2D(source_color, uv_interp + vec2(-3.0, 0.0) * pix_size) * 0.106595;
    color *= glow_strength;
#endif

#ifdef GLOW_GAUSSIAN_VERTICAL
    color = texture2D(source_color, uv_interp) * 0.288713;
    color += texture2D(source_color, uv_interp + vec2(0.0, 1.0) * pixel_size) * 0.233062;
    color += texture2D(source_color, uv_interp + vec2(0.0, 2.0) * pixel_size) * 0.122581;
    color += texture2D(source_color, uv_interp + vec2(0.0, -1.0) * pixel_size) * 0.233062;
    color += texture2D(source_color, uv_interp + vec2(0.0, -2.0) * pixel_size) * 0.122581;
    color *= glow_strength;
#endif

#if defined(DOF_FAR_BLUR) || defined(DOF_NEAR_BLUR)
    vec4 color_accum = vec4(0.0);
    float weight_accum = 0.0;

    float depth = texture2D(dof_source_depth, uv_interp).r;
    depth = depth * 2.0 - 1.0;
    depth = (2.0 * camera_z_near) / (camera_z_far + camera_z_near - depth * (camera_z_far - camera_z_near));

#ifdef DOF_FAR_BLUR
    float amount = smoothstep(dof_begin, dof_end, depth);
#else // DOF_NEAR_BLUR
    float amount = smoothstep(dof_end, dof_begin, depth);
#endif

    for (int i = -5; i <= 5; i++) {
        for (int j = -5; j <= 5; j++) {
            vec2 tap_uv = uv_interp + vec2(float(i), float(j)) * dof_dir * amount * dof_radius;
            float tap_weight = 1.0 - length(vec2(i, j)) / 5.0;
            color_accum += texture2D(source_color, tap_uv) * tap_weight;
            weight_accum += tap_weight;
        }
    }

    if (weight_accum > 0.0) {
        color_accum /= weight_accum;
    }

    color = color_accum;
#endif // DOF_FAR_BLUR || DOF_NEAR_BLUR

#ifdef GLOW_FIRST_PASS
    float luminance = max(color.r, max(color.g, color.b));
    float feedback = max(smoothstep(glow_hdr_threshold, glow_hdr_threshold + glow_hdr_scale, luminance), glow_bloom);
    color = min(color * feedback, vec4(luminance_cap));
#endif

    gl_FragColor = color;
}
