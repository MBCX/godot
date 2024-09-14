/**************************************************************************/
/*  cubemap_filter.h                                                      */
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

#ifndef CUBEMAP_FILTER_GLES2_H
#define CUBEMAP_FILTER_GLES2_H

#ifdef GLES2_ENABLED

#include "drivers/gles2/shaders/effects/cubemap_filter.glsl.gen.h"

namespace GLES2 {

class CubemapFilter {
private:
    struct CMF {
        CubemapFilterShaderGLES2 shader;
        RID shader_version;
    } cubemap_filter;

    static CubemapFilter *singleton;

    // Use for full-screen effects.
    GLuint screen_quad = 0;
    GLuint screen_quad_array = 0;

    uint32_t ggx_samples = 128;

public:
    static CubemapFilter *get_singleton() {
        return singleton;
    }

    CubemapFilter();
    ~CubemapFilter();

    void filter_radiance(GLuint p_source_cubemap, GLuint p_dest_cubemap, GLuint p_dest_framebuffer, int p_source_size, int p_mipmap_count);
};

} //namespace GLES2

#endif // GLES2_ENABLED

#endif // CUBEMAP_FILTER_GLES2_H
