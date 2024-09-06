#[vertex]
attribute vec2 vertex_attrib;

varying vec2 uv_interp;

#ifdef USE_MULTIVIEW
uniform highp mat4 projection_matrix_view[2];
#else
uniform highp mat4 projection_matrix;
#endif

void main() {
    uv_interp = vertex_attrib * 0.5 + 0.5;
    gl_Position = vec4(vertex_attrib, 1.0, 1.0);
}

#[fragment]
precision mediump float;

uniform samplerCube radiance;
#ifdef USE_MULTIVIEW
uniform sampler2DArray half_res;
uniform sampler2DArray quarter_res;
#else
uniform sampler2D half_res;
uniform sampler2D quarter_res;
#endif

#ifdef USE_MULTIVIEW
uniform highp mat4 inv_projection_matrix_view[2];
#else
uniform highp mat4 inv_projection_matrix;
#endif

uniform mediump vec4 fog_light_color;
uniform highp mat4 orientation;
uniform vec3 position;
uniform vec3 radiance_inverse_xform;
uniform float time;
uniform float exposure;

varying vec2 uv_interp;

#ifdef USE_MULTIVIEW
#define ViewIndex gl_ViewID_OVR
#else
#define ViewIndex 0
#endif

vec4 texturePanorama(samplerCube tex, vec3 normal) {
    vec3 inverted = normal * radiance_inverse_xform;
    return textureCube(tex, inverted);
}

void main() {
    vec3 cube_normal;
    
    #ifdef USE_MULTIVIEW
    vec4 uv = inv_projection_matrix_view[ViewIndex] * vec4(uv_interp * 2.0 - 1.0, 0.0, 1.0);
    cube_normal = normalize(mat3(orientation) * uv.xyz);
    #else
    cube_normal.z = -1.0;
    cube_normal.x = (uv_interp.x - 0.5) * 2.0;
    cube_normal.y = -(uv_interp.y - 0.5) * 2.0;
    cube_normal = mat3(orientation) * cube_normal;
    #endif
    
    vec3 color = texturePanorama(radiance, cube_normal).rgb;
    
    // Apply exposure
    color *= exposure;

    // Apply fog
    float z = cube_normal.z;
    float fog_amount = clamp(exp(-length(position) * 0.05), 0.0, 1.0);
    color = mix(fog_light_color.rgb, color, fog_amount);
    
    gl_FragColor = vec4(color, 1.0);
}
