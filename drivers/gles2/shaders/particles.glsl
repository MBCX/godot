/* clang-format off */
#[modes]

mode_default =

#[specializations]

MODE_3D = false
USERDATA1_USED = false
USERDATA2_USED = false
USERDATA3_USED = false
USERDATA4_USED = false
USERDATA5_USED = false
USERDATA6_USED = false

#[vertex]

#ifdef USE_GLES_OVER_GL
#define lowp
#define mediump
#define highp
#else
#if defined(USE_HIGHP_PRECISION)
precision highp float;
precision highp int;
#else
precision mediump float;
precision mediump int;
#endif
#endif

// GLES2 doesn't support layout qualifiers, so we'll use attributes and uniforms instead
attribute vec4 color;
attribute vec4 velocity_flags;
attribute vec4 custom;
attribute vec4 xform_1;
attribute vec4 xform_2;
#ifdef MODE_3D
attribute vec4 xform_3;
#endif

// GLES2 doesn't support transform feedback, so we'll remove the tfb comments

varying vec4 v_color;
varying vec4 v_velocity_flags;
varying vec4 v_custom;
varying vec4 v_xform_1;
varying vec4 v_xform_2;
#ifdef MODE_3D
varying vec4 v_xform_3;
#endif

// GLES2 doesn't support ubo, so we'll use individual uniforms
uniform bool u_emitting;
uniform int u_cycle;
uniform float u_system_phase;
uniform float u_prev_system_phase;

uniform float u_explosiveness;
uniform float u_randomness;
uniform float u_time;
uniform float u_delta;

uniform float u_particle_size;
uniform float u_amount_ratio;

uniform int u_random_seed;
uniform int u_attractor_count;
uniform int u_collider_count;
uniform int u_frame;

uniform mat4 u_emission_transform;

uniform vec3 u_emitter_velocity;
uniform float u_interp_to_end;

// We'll need to create individual uniforms for attractors and colliders
// This is a simplified version, you might need to adjust based on your needs
uniform vec4 u_attractor_params[MAX_ATTRACTORS * 4]; // 4 vec4s per attractor
uniform vec4 u_collider_params[MAX_COLLIDERS * 4]; // 4 vec4s per collider

uniform sampler2D u_height_field_texture;

uniform float u_lifetime;
uniform bool u_clear;
uniform int u_total_particles;
uniform bool u_use_fractional_delta;

// GLES2 doesn't support bitwise operations, so we'll need to implement them using arithmetic
#define PARTICLE_FLAG_ACTIVE 1.0
#define PARTICLE_FLAG_STARTED 2.0
#define PARTICLE_FLAG_TRAILED 4.0

float hash(float x) {
    return fract(sin(x) * 43758.5453);
}

vec3 safe_normalize(vec3 v) {
    float len = length(v);
    if (len > 0.001) {
        return v / len;
    }
    return vec3(0.0, 0.0, 0.0);
}

float decode_float(vec4 rgba) {
    return dot(rgba, vec4(1.0, 1.0/255.0, 1.0/65025.0, 1.0/16581375.0)) * 2.0 - 1.0;
}

void main() {
    bool apply_forces = true;
    bool apply_velocity = true;
    float local_delta = u_delta;

    float mass = 1.0;

    bool restart = false;

    bool restart_position = false;
    bool restart_rotation_scale = false;
    bool restart_velocity = false;
    bool restart_color = false;
    bool restart_custom = false;

    mat4 xform = mat4(1.0);
    float flags = velocity_flags.w;

    if (u_clear) {
        v_color = vec4(1.0);
        v_custom = vec4(0.0);
        v_velocity_flags = vec4(0.0);
    } else {
        v_color = color;
        v_velocity_flags = velocity_flags;
        v_custom = custom;
        xform[0] = xform_1;
        xform[1] = xform_2;
#ifdef MODE_3D
        xform[2] = xform_3;
#endif
        xform = transpose(xform);
    }

    // Clear started flag if set
    flags = floor(mod(flags, PARTICLE_FLAG_STARTED));

    bool collided = false;
    vec3 collision_normal = vec3(0.0);
    float collision_depth = 0.0;

    vec3 attractor_force = vec3(0.0);

#if !defined(DISABLE_VELOCITY)
    if (mod(flags, 2.0) >= 1.0) { // Check if PARTICLE_FLAG_ACTIVE is set
        xform[3].xyz += v_velocity_flags.xyz * local_delta;
    }
#endif

    float index = float(gl_VertexID);
    if (u_emitting) {
        float restart_phase = index / float(u_total_particles);

        if (u_randomness > 0.0) {
            float seed = float(u_cycle);
            if (restart_phase >= u_system_phase) {
                seed -= 1.0;
            }
            seed *= float(u_total_particles);
            seed += index;
            float random = fract(sin(seed) * 43758.5453);
            restart_phase += u_randomness * random * 1.0 / float(u_total_particles);
        }

        restart_phase *= (1.0 - u_explosiveness);

        if (u_system_phase > u_prev_system_phase) {
            if (restart_phase >= u_prev_system_phase && restart_phase < u_system_phase) {
                restart = true;
                if (u_use_fractional_delta) {
                    local_delta = (u_system_phase - restart_phase) * u_lifetime;
                }
            }
        } else if (u_delta > 0.0) {
            if (restart_phase >= u_prev_system_phase) {
                restart = true;
                if (u_use_fractional_delta) {
                    local_delta = (1.0 - restart_phase + u_system_phase) * u_lifetime;
                }
            } else if (restart_phase < u_system_phase) {
                restart = true;
                if (u_use_fractional_delta) {
                    local_delta = (u_system_phase - restart_phase) * u_lifetime;
                }
            }
        }

        if (restart) {
            flags = u_emitting ? (PARTICLE_FLAG_ACTIVE + PARTICLE_FLAG_STARTED) : 0.0;
            restart_position = true;
            restart_rotation_scale = true;
            restart_velocity = true;
            restart_color = true;
            restart_custom = true;
        }
    }

    bool particle_active = mod(flags, 2.0) >= 1.0; // Check if PARTICLE_FLAG_ACTIVE is set

    float particle_number = floor(flags / 65536.0) * float(u_total_particles) + index;

    if (restart && particle_active) {
#CODE : START
    }

    if (particle_active) {
        // Attractor logic
        for (int i = 0; i < MAX_ATTRACTORS; i++) {
            if (i >= u_attractor_count) break;

            vec4 attractor_params = u_attractor_params[i * 4];
            vec4 attractor_transform1 = u_attractor_params[i * 4 + 1];
            vec4 attractor_transform2 = u_attractor_params[i * 4 + 2];
            vec4 attractor_transform3 = u_attractor_params[i * 4 + 3];

            vec3 dir;
            float amount;
            vec3 rel_vec = xform[3].xyz - attractor_transform3.xyz;
            mat3 attractor_transform = mat3(attractor_transform1.xyz, attractor_transform2.xyz, attractor_transform3.xyz);
            vec3 local_pos = rel_vec * attractor_transform;

            if (attractor_params.x == 0.0) { // ATTRACTOR_TYPE_SPHERE
                dir = safe_normalize(rel_vec);
                float d = length(local_pos) / attractor_params.y;
                if (d > 1.0) {
                    continue;
                }
                amount = max(0.0, 1.0 - d);
            } else if (attractor_params.x == 1.0) { // ATTRACTOR_TYPE_BOX
                dir = safe_normalize(rel_vec);
                vec3 abs_pos = abs(local_pos / attractor_params.yzw);
                float d = max(abs_pos.x, max(abs_pos.y, abs_pos.z));
                if (d > 1.0) {
                    continue;
                }
                amount = max(0.0, 1.0 - d);
            }

            float attractor_attenuation = attractor_params.w;
            amount = pow(amount, attractor_attenuation);
            dir = safe_normalize(mix(dir, attractor_transform3.xyz, attractor_params.z));
            attractor_force -= amount * dir * attractor_params.y;
        }

        // Collision system adapted for GLES2
        const float EPSILON = 0.001;
        const float SDF_MAX_LENGTH = 16384.0;

        if (u_collider_count == 1.0 && u_collider_params[0].x == 4.0) { // COLLIDER_TYPE_2D_SDF
            // 2D collision
            vec2 pos = xform[3].xy;
            vec4 to_sdf_x = u_collider_params[1];
            vec4 to_sdf_y = u_collider_params[2];
            vec2 sdf_pos = vec2(dot(vec4(pos, 0.0, 1.0), to_sdf_x), dot(vec4(pos, 0.0, 1.0), to_sdf_y));

            vec4 sdf_to_screen = u_collider_params[3];

            vec2 uv_pos = sdf_pos * sdf_to_screen.xy + sdf_to_screen.zw;

            if (uv_pos.x > 0.0 && uv_pos.x < 1.0 && uv_pos.y > 0.0 && uv_pos.y < 1.0) {
                vec2 pos2 = pos + vec2(0.0, u_particle_size);
                vec2 sdf_pos2 = vec2(dot(vec4(pos2, 0.0, 1.0), to_sdf_x), dot(vec4(pos2, 0.0, 1.0), to_sdf_y));
                float sdf_particle_size = distance(sdf_pos, sdf_pos2);

                float d = decode_float(texture2D(u_height_field_texture, uv_pos)) * SDF_MAX_LENGTH;

                d -= sdf_particle_size;
                if (d < EPSILON) {
                    vec2 n = normalize(vec2(
                        decode_float(texture2D(u_height_field_texture, uv_pos + vec2(EPSILON, 0.0))) - 
                        decode_float(texture2D(u_height_field_texture, uv_pos - vec2(EPSILON, 0.0))),
                        decode_float(texture2D(u_height_field_texture, uv_pos + vec2(0.0, EPSILON))) - 
                        decode_float(texture2D(u_height_field_texture, uv_pos - vec2(0.0, EPSILON)))
                    ));

                    collided = true;
                    sdf_pos2 = sdf_pos + n * d;
                    pos2 = vec2(dot(vec4(sdf_pos2, 0.0, 1.0), u_collider_params[4]), 
                                dot(vec4(sdf_pos2, 0.0, 1.0), u_collider_params[5]));

                    n = pos - pos2;

                    collision_normal = normalize(vec3(n, 0.0));
                    collision_depth = length(n);
                }
            }
        } else {
            for (int i = 0; i < MAX_COLLIDERS; i++) {
                if (float(i) >= u_collider_count) break;

                vec4 collider_params = u_collider_params[i * 4];
                vec4 collider_transform1 = u_collider_params[i * 4 + 1];
                vec4 collider_transform2 = u_collider_params[i * 4 + 2];
                vec4 collider_transform3 = u_collider_params[i * 4 + 3];

                vec3 normal;
                float depth;
                bool col = false;

                vec3 rel_vec = xform[3].xyz - collider_transform3.xyz;
                mat3 collider_transform = mat3(collider_transform1.xyz, collider_transform2.xyz, collider_transform3.xyz);
                vec3 local_pos = rel_vec * collider_transform;

                if (collider_params.x == 0.0) { // COLLIDER_TYPE_SPHERE
                    float d = length(rel_vec) - (u_particle_size + collider_params.y);

                    if (d < EPSILON) {
                        col = true;
                        depth = -d;
                        normal = normalize(rel_vec);
                    }
                } else if (collider_params.x == 1.0) { // COLLIDER_TYPE_BOX
                    vec3 abs_pos = abs(local_pos);
                    vec3 sgn_pos = sign(local_pos);

                    if (abs_pos.x > collider_params.y || abs_pos.y > collider_params.z || abs_pos.z > collider_params.w) {
                        // Point outside box
                        vec3 closest = min(abs_pos, collider_params.yzw);
                        vec3 rel = abs_pos - closest;
                        depth = length(rel) - u_particle_size;
                        if (depth < EPSILON) {
                            col = true;
                            normal = collider_transform * (normalize(rel) * sgn_pos);
                            depth = -depth;
                        }
                    } else {
                        // Point inside box
                        vec3 axis_len = collider_params.yzw - abs_pos;
                        if (axis_len.x < axis_len.y && axis_len.x < axis_len.z) {
                            normal = vec3(1.0, 0.0, 0.0);
                        } else if (axis_len.y < axis_len.x && axis_len.y < axis_len.z) {
                            normal = vec3(0.0, 1.0, 0.0);
                        } else {
                            normal = vec3(0.0, 0.0, 1.0);
                        }

                        col = true;
                        depth = dot(normal * axis_len, vec3(1.0)) + u_particle_size;
                        normal = collider_transform * (normal * sgn_pos);
                    }
                }
                // Note: SDF and HEIGHT_FIELD colliders are omitted due to
                // being not supported in GLES2.

                if (col) {
                    if (!collided) {
                        collided = true;
                        collision_normal = normal;
                        collision_depth = depth;
                    } else {
                        vec3 c = collision_normal * collision_depth;
                        c += normal * max(0.0, depth - dot(normal, c));
                        collision_normal = normalize(c);
                        collision_depth = length(c);
                    }
                }
            }
        }
    }

    if (particle_active) {
#CODE : PROCESS
    }

    flags = floor(flags / 2.0) * 2.0;
    if (particle_active) {
        flags += 1.0;
    }

    xform = transpose(xform);
    v_xform_1 = xform[0];
    v_xform_2 = xform[1];
#ifdef MODE_3D
    v_xform_3 = xform[2];
#endif
    v_velocity_flags.w = flags;

    gl_Position = vec4(0.0, 0.0, 0.0, 1.0); // Particles typically don't render in the vertex shader
}

/* clang-format off */
#[fragment]

void main() {
    discard;
}
/* clang-format on */