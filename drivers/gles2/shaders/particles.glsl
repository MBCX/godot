/* clang-format off */
#define MODE_2D 0
#define MODE_3D 1

// Specialization Constants (to be handled by the engine)
// #define MODE_3D false
// #define USERDATA1_USED false
// #define USERDATA2_USED false
// ... (other USERDATA flags)

#[vertex]

// Attributes
attribute vec4 color;
attribute vec4 velocity_flags;
attribute vec4 custom;
attribute vec4 xform_1;
attribute vec4 xform_2;
#ifdef MODE_3D
attribute vec4 xform_3;
#endif
#ifdef USERDATA1_USED
attribute vec4 userdata1;
#endif
#ifdef USERDATA2_USED
attribute vec4 userdata2;
#endif
#ifdef USERDATA3_USED
attribute vec4 userdata3;
#endif
#ifdef USERDATA4_USED
attribute vec4 userdata4;
#endif
#ifdef USERDATA5_USED
attribute vec4 userdata5;
#endif
#ifdef USERDATA6_USED
attribute vec4 userdata6;
#endif

// Varyings
varying vec4 out_color;
varying vec4 out_velocity_flags;
varying vec4 out_custom;
varying vec4 out_xform_1;
varying vec4 out_xform_2;
#ifdef MODE_3D
varying vec4 out_xform_3;
#endif
#ifdef USERDATA1_USED
varying vec4 out_userdata1;
#endif
#ifdef USERDATA2_USED
varying vec4 out_userdata2;
#endif
#ifdef USERDATA3_USED
varying vec4 out_userdata3;
#endif
#ifdef USERDATA4_USED
varying vec4 out_userdata4;
#endif
#ifdef USERDATA5_USED
varying vec4 out_userdata5;
#endif
#ifdef USERDATA6_USED
varying vec4 out_userdata6;
#endif

// Uniforms
uniform bool emitting;
uniform int cycle;
uniform float system_phase;
uniform float prev_system_phase;

uniform float explosiveness;
uniform float randomness;
uniform float time;
uniform float delta;

uniform float particle_size;
uniform float amount_ratio;

uniform uint random_seed;
uniform uint attractor_count;
uniform uint collider_count;
uniform uint frame;

uniform mat4 emission_transform;
uniform vec3 emitter_velocity;
uniform float interp_to_end;

// Constants
#define MATH_PI 3.14159265359
#define MAX_ATTRACTORS 32
#define MAX_COLLIDERS 32

struct Attractor {
	mat4 transform;
	vec4 extents; // Extents or radius. w-channel is padding.

	uint type;
	float strength;
	float attenuation;
	float directionality;
};

void main() {
    bool restart = false;
    float fraction = 0.0;
    float rand_from_seed = 0.0;
    uint alt_seed = random_seed;

    float pi = 3.14159265359;
    uint number = uint(gl_VertexID);
    uint index = number / uint(total_particles);
    uint sub_index = number % uint(total_particles);
    float total_particles_f = float(total_particles);
    float particle_size_final = particle_size;

    if (bool(int(velocity_flags.w) & int(PARTICLE_FLAG_ACTIVE))) {
        // Initialize particle transform
        mat4 xform = mat4(vec4(xform_1.xyz, 0.0), vec4(xform_2.xyz, 0.0), vec4(0.0, 0.0, 1.0, 0.0), vec4(xform_1.w, xform_2.w, 0.0, 1.0));

        // Particle update logic
        if (emitting) {
            float restart_phase = float(sub_index) / total_particles_f;

            if (randomness > 0.0) {
                int rand_seed = int(random_seed) * int(frame) + int(index);
                rand_from_seed = rand_from_seed_m1_p1(rand_seed);
                restart_phase += randomness * rand_from_seed;
            }

            restart_phase = mod(restart_phase, 1.0);

            if (restart_phase >= prev_system_phase && restart_phase < system_phase) {
                restart = true;
                fraction = (system_phase - restart_phase) / (system_phase - prev_system_phase);
            }
        }

        if (restart) {
            // Restart particle
            out_velocity_flags = vec4(0.0);
            out_custom = vec4(0.0);
            xform = emission_transform;
            out_velocity_flags.w = float(PARTICLE_FLAG_ACTIVE | PARTICLE_FLAG_STARTED);

            // Apply emission parameters (EMISSION_CODE placeholder)
            // ...

        } else {
            // Particle update logic (PROCESS_CODE placeholder)
            // ...
        }

        // Attractor logic
        for (int i = 0; i < MAX_ATTRACTORS; i++) {
            if (i >= int(attractor_count)) {
                break;
            }

            vec3 dir;
            float amount;
            vec3 rel_vec = xform[3].xyz - attractors[i].transform[3].xyz;
            vec3 local_pos = (inverse(attractors[i].transform) * vec4(rel_vec, 0.0)).xyz;

            if (attractors[i].type == ATTRACTOR_TYPE_SPHERE) {
                dir = normalize(rel_vec);
                float d = length(local_pos) / attractors[i].extents.x;
                if (d > 1.0) continue;
                amount = max(0.0, 1.0 - d);
            } else if (attractors[i].type == ATTRACTOR_TYPE_BOX) {
                dir = normalize(rel_vec);
                vec3 abs_pos = abs(local_pos / attractors[i].extents.xyz);
                float d = max(abs_pos.x, max(abs_pos.y, abs_pos.z));
                if (d > 1.0) continue;
                amount = max(0.0, 1.0 - d);
            } else if (attractors[i].type == ATTRACTOR_TYPE_VECTOR_FIELD) {
                // Vector field logic (simplified for GLES2)
                dir = normalize(attractors[i].transform[2].xyz);
                amount = 1.0;
            }

            amount *= attractors[i].strength;
            out_velocity_flags.xyz += amount * dir;
        }

        // Output updated particle data
        out_color = color;
        out_custom = custom;
        out_xform_1 = xform[0];
        out_xform_2 = xform[1];
        #ifdef MODE_3D
        out_xform_3 = xform[2];
        #endif

        #ifdef USERDATA1_USED
        out_userdata1 = userdata1;
        #endif
        #ifdef USERDATA2_USED
        out_userdata2 = userdata2;
        #endif
        #ifdef USERDATA3_USED
        out_userdata3 = userdata3;
        #endif
        #ifdef USERDATA4_USED
        out_userdata4 = userdata4;
        #endif
        #ifdef USERDATA5_USED
        out_userdata5 = userdata5;
        #endif
        #ifdef USERDATA6_USED
        out_userdata6 = userdata6;
        #endif
    } else {
        // Particle is inactive
        out_color = vec4(0.0);
        out_custom = vec4(0.0);
        out_velocity_flags = vec4(0.0);
        out_xform_1 = vec4(0.0);
        out_xform_2 = vec4(0.0);
        #ifdef MODE_3D
        out_xform_3 = vec4(0.0);
        #endif
    }

    gl_Position = vec4(0.0, 0.0, 0.0, 1.0);
}

// Helper functions
float rand_from_seed_m1_p1(int seed) {
    int hash = seed * 1103515245 + 12345;
    float result = float(hash) / 2147483648.0;
    return result * 2.0 - 1.0;
}

#[fragment]

#ifdef GL_ES
precision mediump float;
#endif

varying vec4 out_color;
varying vec4 out_velocity_flags;
varying vec4 out_custom;
varying vec4 out_xform_1;
varying vec4 out_xform_2;
#ifdef MODE_3D
varying vec4 out_xform_3;
#endif
#ifdef USERDATA1_USED
varying vec4 out_userdata1;
#endif
#ifdef USERDATA2_USED
varying vec4 out_userdata2;
#endif
#ifdef USERDATA3_USED
varying vec4 out_userdata3;
#endif
#ifdef USERDATA4_USED
varying vec4 out_userdata4;
#endif
#ifdef USERDATA5_USED
varying vec4 out_userdata5;
#endif
#ifdef USERDATA6_USED
varying vec4 out_userdata6;
#endif

uniform sampler2D color_texture;
uniform sampler2D normal_texture;
uniform sampler2D specular_texture;

uniform float lifetime;
uniform bool use_particle_trails;

void main() {
    // In GLES2, we can't update particles using transform feedback.
    // Instead, we'll render the particles to the screen.
    vec4 color = out_color;
    
    // Apply textures if available
    if (color_texture != 0) {
        color *= texture2D(color_texture, gl_PointCoord);
    }
    
    // Discard transparent fragments
    if (color.a < 0.01) {
        discard;
    }
    
    // Apply normal mapping if available
    if (normal_texture != 0) {
        vec3 normal = texture2D(normal_texture, gl_PointCoord).xyz * 2.0 - 1.0;
        // Apply normal in view space (simplified)
        color.rgb *= max(dot(normal, vec3(0.0, 0.0, 1.0)), 0.0);
    }
    
    // Apply specular if available
    if (specular_texture != 0) {
        vec4 specular = texture2D(specular_texture, gl_PointCoord);
        color.rgb += specular.rgb * specular.a;
    }
    
    // Apply particle trail effect if enabled
    if (use_particle_trails) {
        float trail_factor = 1.0 - (out_velocity_flags.w / lifetime);
        color.a *= smoothstep(0.0, 0.2, trail_factor);
    }
    
    gl_FragColor = color;
}

// Utility functions

float rand_from_seed(inout int seed) {
    seed = (seed * 1103515245 + 12345) & 0x7fffffff;
    return float(seed) / 2147483647.0;
}

vec3 rand_from_seed_m1_p1(inout int seed) {
    return vec3(
        rand_from_seed(seed) * 2.0 - 1.0,
        rand_from_seed(seed) * 2.0 - 1.0,
        rand_from_seed(seed) * 2.0 - 1.0
    );
}

float rand_from_seed_0_1(inout int seed) {
    return rand_from_seed(seed);
}

vec3 rand_vec3_0_1(inout int seed) {
    return vec3(
        rand_from_seed(seed),
        rand_from_seed(seed),
        rand_from_seed(seed)
    );
}

vec3 rand_vec3_m1_p1(inout int seed) {
    return vec3(
        rand_from_seed(seed) * 2.0 - 1.0,
        rand_from_seed(seed) * 2.0 - 1.0,
        rand_from_seed(seed) * 2.0 - 1.0
    );
}
