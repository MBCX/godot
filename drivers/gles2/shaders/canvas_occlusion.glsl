#[vertex]
attribute vec3 vertex;

uniform highp mat4 projection;
uniform highp vec4 modelview1;
uniform highp vec4 modelview2;
uniform highp vec2 direction;
uniform highp float z_far;

varying highp float depth;

void main() {
    highp vec4 vtx = vec4(vertex, 1.0) * mat4(modelview1, modelview2, vec4(0.0, 0.0, 1.0, 0.0), vec4(0.0, 0.0, 0.0, 1.0));

    #ifdef MODE_SHADOW
    depth = dot(direction, vtx.xy);
    #endif
    gl_Position = projection * vtx;
}

#[fragment]
#ifdef USE_RGBA_SHADOWS
#extension GL_EXT_frag_depth : enable
#endif

uniform highp mat4 projection;
uniform highp vec4 modelview1;
uniform highp vec4 modelview2;
uniform highp vec2 direction;
uniform highp float z_far;

varying highp float depth;

void main() {
    highp float out_depth = 1.0;

#ifdef MODE_SHADOW
    out_depth = depth / z_far;
#endif

#ifdef USE_RGBA_SHADOWS
    out_depth = clamp(out_depth, -1.0, 1.0);
    out_depth = out_depth * 0.5 + 0.5;
    highp vec4 comp = fract(out_depth * vec4(255.0 * 255.0 * 255.0, 255.0 * 255.0, 255.0, 1.0));
    comp -= comp.xxyz * vec4(0.0, 1.0 / 255.0, 1.0 / 255.0, 1.0 / 255.0);
    gl_FragColor = comp;
#ifdef GL_EXT_frag_depth
    gl_FragDepthEXT = out_depth;
#endif
#else
    gl_FragColor = vec4(out_depth);
#endif // USE_RGBA_SHADOWS
}