#[vertex]
attribute highp vec4 vertex_attrib;
attribute vec2 uv_in;

varying vec2 uv_interp;

void main() {
    uv_interp = uv_in;
    gl_Position = vertex_attrib;
}

#[fragment]
#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif

uniform samplerCube source_cube;

uniform bool z_flip;
uniform float z_far;
uniform float z_near;
uniform float bias;

varying vec2 uv_interp;

void main() {
    vec3 normal = vec3(uv_interp * 2.0 - 1.0, 0.0);

    normal.z = 0.5 - 0.5 * ((normal.x * normal.x) + (normal.y * normal.y));
    normal = normalize(normal);

    if (!z_flip) {
        normal.z = -normal.z;
    }

    float depth = textureCube(source_cube, normal).r;

    // absolute values for direction cosines, bigger value equals closer to basis axis
    vec3 unorm = abs(normal);

    if ((unorm.x >= unorm.y) && (unorm.x >= unorm.z)) {
        // x code
        unorm = normal.x > 0.0 ? vec3(1.0, 0.0, 0.0) : vec3(-1.0, 0.0, 0.0);
    } else if ((unorm.y > unorm.x) && (unorm.y >= unorm.z)) {
        // y code
        unorm = normal.y > 0.0 ? vec3(0.0, 1.0, 0.0) : vec3(0.0, -1.0, 0.0);
    } else if ((unorm.z > unorm.x) && (unorm.z > unorm.y)) {
        // z code
        unorm = normal.z > 0.0 ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 0.0, -1.0);
    } else {
        // oh-no we messed up code
        // has to be
        unorm = vec3(1.0, 0.0, 0.0);
    }

    float depth_fix = 1.0 / dot(normal, unorm);

    depth = 2.0 * depth - 1.0;
    float linear_depth = 2.0 * z_near * z_far / (z_far + z_near + depth * (z_far - z_near));
    gl_FragDepth = (z_far - (linear_depth * depth_fix + bias)) / z_far;

    // Output black color as we're only interested in the depth
    gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);
}