/* clang-format off */
#[modes]

mode_default = #define MODE_SIMPLE_COPY
mode_copy_section = #define USE_COPY_SECTION \n#define MODE_SIMPLE_COPY
mode_copy_section_source = #define USE_COPY_SECTION \n#define MODE_SIMPLE_COPY \n#define MODE_COPY_FROM
mode_screen = #define MODE_SIMPLE_COPY \n#define MODE_MULTIPLY
mode_gaussian_blur = #define MODE_GAUSSIAN_BLUR
mode_mipmap = #define MODE_MIPMAP
mode_simple_color = #define MODE_SIMPLE_COLOR \n#define USE_COPY_SECTION

#[vertex]

attribute vec2 vertex_attrib;

varying vec2 uv_interp;

#if defined(USE_COPY_SECTION) || defined(MODE_GAUSSIAN_BLUR)
uniform highp vec4 copy_section;
#endif
#if defined(MODE_GAUSSIAN_BLUR) || defined(MODE_COPY_FROM)
uniform highp vec4 source_section;
#endif

void main() {
    uv_interp = vertex_attrib * 0.5 + 0.5;
    gl_Position = vec4(vertex_attrib, 1.0, 1.0);

#if defined(USE_COPY_SECTION) || defined(MODE_GAUSSIAN_BLUR)
    gl_Position.xy = (copy_section.xy + uv_interp.xy * copy_section.zw) * 2.0 - 1.0;
#endif
#if defined(MODE_GAUSSIAN_BLUR) || defined(MODE_COPY_FROM)
    uv_interp = source_section.xy + uv_interp * source_section.zw;
#endif
}

#[fragment]
precision mediump float;

varying vec2 uv_interp;

#ifdef MODE_SIMPLE_COLOR
uniform vec4 color_in;
#endif

#ifdef MODE_MULTIPLY
uniform float multiply;
#endif

#ifdef MODE_GAUSSIAN_BLUR
uniform highp vec2 pixel_size;
#endif

uniform sampler2D source;

void main() {
#ifdef MODE_SIMPLE_COPY
    vec4 color = texture2D(source, uv_interp);

#ifdef MODE_MULTIPLY
    color *= multiply;
#endif

    gl_FragColor = color;
#endif

#ifdef MODE_SIMPLE_COLOR
    gl_FragColor = color_in;
#endif

#ifdef MODE_GAUSSIAN_BLUR
    vec4 A = texture2D(source, uv_interp + pixel_size * vec2(-1.0, -1.0));
    vec4 B = texture2D(source, uv_interp + pixel_size * vec2(0.0, -1.0));
    vec4 C = texture2D(source, uv_interp + pixel_size * vec2(1.0, -1.0));
    vec4 D = texture2D(source, uv_interp + pixel_size * vec2(-0.5, -0.5));
    vec4 E = texture2D(source, uv_interp + pixel_size * vec2(0.5, -0.5));
    vec4 F = texture2D(source, uv_interp + pixel_size * vec2(-1.0, 0.0));
    vec4 G = texture2D(source, uv_interp);
    vec4 H = texture2D(source, uv_interp + pixel_size * vec2(1.0, 0.0));
    vec4 I = texture2D(source, uv_interp + pixel_size * vec2(-0.5, 0.5));
    vec4 J = texture2D(source, uv_interp + pixel_size * vec2(0.5, 0.5));
    vec4 K = texture2D(source, uv_interp + pixel_size * vec2(-1.0, 1.0));
    vec4 L = texture2D(source, uv_interp + pixel_size * vec2(0.0, 1.0));
    vec4 M = texture2D(source, uv_interp + pixel_size * vec2(1.0, 1.0));

    float weight = 0.5 / 4.0;
    float lesser_weight = 0.125 / 4.0;

    gl_FragColor = (D + E + I + J) * weight;
    gl_FragColor += (A + B + G + F) * lesser_weight;
    gl_FragColor += (B + C + H + G) * lesser_weight;
    gl_FragColor += (F + G + L + K) * lesser_weight;
    gl_FragColor += (G + H + M + L) * lesser_weight;
#endif

#ifdef MODE_MIPMAP
    gl_FragColor = texture2D(source, uv_interp);
#endif
}