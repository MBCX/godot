/* clang-format off */
#[modes]

mode_default =
mode_copy = #define MODE_DIRECT_WRITE

#[vertex]
attribute highp vec2 vertex_attrib;

varying highp vec2 uv_interp;

void main() {
    uv_interp = vertex_attrib;
    gl_Position = vec4(uv_interp, 0.0, 1.0);
}

#[fragment]
precision highp float;

#define M_PI 3.14159265359

uniform samplerCube source_cube;

uniform int face_id;

#ifndef MODE_DIRECT_WRITE
uniform int sample_count;
uniform vec4 sample_directions_mip[64]; // Adjusted for GLES2 array size limit
uniform float weight;
#endif

varying highp vec2 uv_interp;

// Don't include tonemap_inc.glsl because all we want is these functions, we don't want the uniforms
vec3 linear_to_srgb(vec3 color) {
    return max(vec3(1.055) * pow(color, vec3(0.416666667)) - vec3(0.055), vec3(0.0));
}

vec3 srgb_to_linear(vec3 color) {
    return color * (color * (color * 0.305306011 + 0.682171111) + 0.012522878);
}

vec3 texelCoordToVec(vec2 uv, int faceID) {
    vec3 result = vec3(0.0);
    if (faceID == 0) result = vec3(1.0, -uv.y, -uv.x);
    else if (faceID == 1) result = vec3(-1.0, -uv.y, uv.x);
    else if (faceID == 2) result = vec3(uv.x, 1.0, uv.y);
    else if (faceID == 3) result = vec3(uv.x, -1.0, -uv.y);
    else if (faceID == 4) result = vec3(uv.x, -uv.y, 1.0);
    else if (faceID == 5) result = vec3(-uv.x, -uv.y, -1.0);
    return normalize(result);
}

void main() {
    vec3 N = texelCoordToVec(uv_interp * 2.0 - 1.0, face_id);

#ifdef MODE_DIRECT_WRITE
    gl_FragColor = textureCube(source_cube, N);
#else
    vec4 sum = vec4(0.0);
    vec3 UpVector = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 TangentX = normalize(cross(UpVector, N));
    vec3 TangentY = cross(N, TangentX);

    for (int sample_num = 0; sample_num < 64; sample_num++) {
        if (sample_num >= sample_count) break;
        vec4 sample_direction_mip = sample_directions_mip[sample_num];
        vec3 L = TangentX * sample_direction_mip.x + TangentY * sample_direction_mip.y + N * sample_direction_mip.z;
        vec3 val = textureCube(source_cube, L).rgb;
        val = srgb_to_linear(val);
        sum.rgb += val * sample_direction_mip.z;
    }

    sum /= weight;

    sum.rgb = linear_to_srgb(sum.rgb);
    gl_FragColor = vec4(sum.rgb, 1.0);
#endif
}