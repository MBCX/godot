/**************************************************************************/
/*  post_effects.h                                                        */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#ifndef POST_EFFECTS_GLES2_H
#define POST_EFFECTS_GLES2_H

#ifdef GLES2_ENABLED

#include "drivers/gles2/shaders/effects/post.glsl.gen.h"
#include "glow.h"

namespace GLES2 {

class PostEffects {
private:
    struct Post {
        PostShaderGLES2 shader;
        RID shader_version;
    } post;

    static PostEffects *singleton;

    // Use for full-screen effects.
    GLuint screen_quad = 0;
    GLuint screen_quad_array = 0;

    void _draw_screen_quad();

public:
    static PostEffects *get_singleton();

    PostEffects();
    ~PostEffects();

    void post_copy(GLuint p_dest_framebuffer, Size2i p_dest_size, GLuint p_source_color, Size2i p_source_size, float p_luminance_multiplier, const Glow::GLOWLEVEL *p_glow_buffers, float p_glow_intensity, uint64_t p_spec_constants = 0);
};

} //namespace GLES2

#endif // GLES2_ENABLED

#endif // POST_EFFECTS_GLES2_H
