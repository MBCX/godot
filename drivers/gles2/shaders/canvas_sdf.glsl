#[vertex]
attribute vec2 vertex_attrib;

uniform ivec2 size;
uniform int stride;
uniform int shift;
uniform ivec2 base_size;

varying vec2 uv_interp;

void main() {
    gl_Position = vec4(vertex_attrib, 0.0, 1.0);
    uv_interp = vertex_attrib * 0.5 + 0.5;
}

#[fragment]
#ifdef GL_FRAGMENT_PRECISION_HIGH
precision highp float;
#else
precision mediump float;
#endif

uniform sampler2D src_pixels;
uniform sampler2D src_process;

uniform ivec2 size;
uniform int stride;
uniform int shift;
uniform ivec2 base_size;

varying vec2 uv_interp;

// Helper function to convert vec4 to float
float vec4_to_float(vec4 p_vec) {
    const vec4 bitSh = vec4(1.0, 1.0 / 255.0, 1.0 / 65025.0, 1.0 / 16581375.0);
    return dot(p_vec, bitSh);
}

// Helper function to convert float to vec4
vec4 float_to_vec4(float p_float) {
    const vec4 bitSh = vec4(1.0, 255.0, 65025.0, 16581375.0);
    const vec4 bitMsk = vec4(1.0 / 255.0, 1.0 / 255.0, 1.0 / 255.0, 0.0);
    vec4 res = fract(p_float * bitSh);
    res -= res.xxyz * bitMsk;
    return res;
}

void main() {
    vec2 pos = uv_interp * vec2(size);
    ivec2 ipos = ivec2(pos);
    
    // MODE_LOAD
    if (shift == 0) {
        bool solid = texture2D(src_pixels, uv_interp).r > 0.5;
        gl_FragColor = solid ? float_to_vec4(-1.0) : float_to_vec4(1.0);
    }
    // MODE_LOAD_SHRINK
    else if (shift > 0) {
        ivec2 base = ipos * (1 << shift);
        ivec2 center = base + ivec2(shift);
        
        float closest = 1e20;
        bool found_solid = false;
        
        for (int i = 0; i < (1 << shift); i++) {
            for (int j = 0; j < (1 << shift); j++) {
                ivec2 ofs = ivec2(i, j);
                vec2 src_uv = vec2(base + ofs) / vec2(base_size);
                
                if (src_uv.x >= 1.0 || src_uv.y >= 1.0) continue;
                
                bool solid = texture2D(src_pixels, src_uv).r > 0.5;
                if (solid) {
                    float dist = length(vec2(base + ofs - center));
                    if (dist < closest) {
                        closest = dist;
                    }
                    found_solid = true;
                }
            }
        }
        
        if (found_solid) {
            gl_FragColor = float_to_vec4(-closest);
        } else {
            gl_FragColor = float_to_vec4(closest);
        }
    }
    // MODE_PROCESS
    else {
        ivec2 base = ipos << abs(shift);
        ivec2 center = base + ivec2(abs(shift));
        
        vec2 rel = vec2(texture2D(src_process, uv_interp).xy);
        bool solid = rel.x < 0.0;
        
        if (solid) {
            rel = -rel - vec2(1.0);
        }
        
        if (center != ivec2(rel)) {
            float dist = length(vec2(rel - center));
            
            for (int i = -1; i <= 1; i++) {
                for (int j = -1; j <= 1; j++) {
                    ivec2 ofs = ivec2(i, j) * stride;
                    vec2 uv = (vec2(ipos + ofs) + vec2(0.5, 0.5)) / vec2(size);
                    
                    if (uv.x < 0.0 || uv.y < 0.0 || uv.x >= 1.0 || uv.y >= 1.0) {
                        continue;
                    }
                    
                    vec2 rel2 = vec2(texture2D(src_process, uv).xy);
                    bool solid2 = rel2.x < 0.0;
                    
                    if (solid2 != solid) {
                        rel2 = vec2(ipos + ofs);
                    }
                    
                    float dist2 = length(vec2(rel2 - center));
                    
                    if (dist2 < dist) {
                        dist = dist2;
                        rel = rel2;
                    }
                }
            }
        }
        
        if (solid) {
            rel = -rel - vec2(1.0);
        }
        
        gl_FragColor = vec4(rel, 0.0, 1.0);
    }
}
