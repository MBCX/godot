/**************************************************************************/
/*  copy_effects.h                                                        */
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

#ifndef COPY_EFFECTS_GLES2_H
#define COPY_EFFECTS_GLES2_H

#ifdef GLES2_ENABLED

#include "drivers/gles2/shaders/effects/copy.glsl.gen.h"

namespace GLES2 {

class CopyEffects {
private:
    struct Copy {
        CopyShaderGLES2 shader;
        RID shader_version;
    } copy;

    static CopyEffects *singleton;

    // Use for full-screen effects.
    GLuint screen_quad = 0;
    GLuint screen_quad_array = 0;

public:
    static CopyEffects *get_singleton();

    CopyEffects();
    ~CopyEffects();

    // These functions assume that a framebuffer and texture are bound already. They only manage the shader, uniforms, and vertex array.
    void copy_to_rect(const Rect2 &p_rect);
    void copy_to_rect_2d(const Rect2 &p_rect, float p_lod = 0.0f);
    void copy_to_and_from_rect(const Rect2 &p_rect);
    void copy_screen(float p_multiply = 1.0);
    void copy_cube_to_rect(const Rect2 &p_rect);
    void copy_cube_to_panorama(float p_mip_level);
    void bilinear_blur(GLuint p_source_texture, int p_mipmap_count, const Rect2i &p_region);
    void gaussian_blur(GLuint p_source_texture, int p_mipmap_count, const Rect2i &p_region, const Size2i &p_size);
    void set_color(const Color &p_color, const Rect2i &p_region);
    void draw_screen_quad();
};

} //namespace GLES2

#endif // GLES2_ENABLED

#endif // COPY_EFFECTS_GLES2_H
