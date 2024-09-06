#[vertex]
attribute highp vec4 color;
attribute highp vec4 velocity_flags;
attribute highp vec4 custom;
attribute highp vec4 xform_1;
attribute highp vec4 xform_2;
#ifdef MODE_3D
attribute highp vec4 xform_3;
#endif

uniform highp vec3 sort_direction;
uniform highp float frame_remainder;

uniform highp vec3 align_up;
uniform highp float align_mode;

uniform highp mat4 inv_emission_transform;

varying highp vec4 out_color;
varying highp vec4 out_custom;
varying highp mat4 out_transform;

#define PARTICLE_FLAG_ACTIVE 1.0

void main() {
    highp mat4 txform = mat4(vec4(0.0), vec4(0.0), vec4(0.0), vec4(-1e20, -1e20, -1e20, 1.0));
    
    if (velocity_flags.w >= PARTICLE_FLAG_ACTIVE) {
        txform = mat4(xform_1, xform_2, 
        #ifdef MODE_3D
            xform_3,
        #else
            vec4(0.0, 0.0, 1.0, 0.0),
        #endif
            vec4(0.0, 0.0, 0.0, 1.0));

        // Apply alignment
        if (align_mode > 0.5) {
            highp vec3 v = velocity_flags.xyz;
            highp vec3 up = align_up;
            
            if (length(v) > 0.0) {
                highp vec3 z = normalize(v);
                highp vec3 x = normalize(cross(up, z));
                highp vec3 y = cross(z, x);
                
                txform[0].xyz = x;
                txform[1].xyz = y;
                txform[2].xyz = z;
            }
        }

        // Apply frame remainder
        txform[3].xyz += velocity_flags.xyz * frame_remainder;

        #ifndef MODE_3D
        // In 2D mode, bring particles to local coordinates
        txform = inv_emission_transform * txform;
        #endif
    }

    out_color = color;
    out_custom = custom;
    out_transform = txform;
    
    gl_Position = vec4(0.0, 0.0, 0.0, 1.0);
}

#[fragment]

void main() {
    discard;
}
