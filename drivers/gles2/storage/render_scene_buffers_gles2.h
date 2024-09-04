/**************************************************************************/
/*  render_scene_buffers_gles2.h                                          */
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

#ifndef RENDER_SCENE_BUFFERS_GLES2_H
#define RENDER_SCENE_BUFFERS_GLES2_H

#ifdef GLES2_ENABLED

#include "servers/rendering/storage/render_scene_buffers.h"
#include "platform_gl.h"

class RenderSceneBuffersGLES2 : public RenderSceneBuffers {
    GDCLASS(RenderSceneBuffersGLES2, RenderSceneBuffers);

public:
    Size2i internal_size; // Size of the buffer we render 3D content to.
    Size2i target_size; // Size of our output buffer (render target).
    RS::ViewportScaling3DMode scaling_3d_mode = RS::VIEWPORT_SCALING_3D_MODE_OFF;
    uint32_t view_count = 1;

    RID render_target;

    // Color format details from our render target
    GLuint color_internal_format = GL_RGBA;
    GLuint color_format = GL_RGBA;
    GLuint color_type = GL_UNSIGNED_BYTE;
    uint32_t color_format_size = 4;

    struct FBDEF {
        GLuint color = 0;
        GLuint depth = 0;
        GLuint fbo = 0;
    };

    FBDEF internal3d; // buffers used to render 3D

    FBDEF backbuffer3d; // our back buffer

    // Buffers for our glow implementation
    struct GLOW {
        struct GLOWLEVEL {
            GLuint fbo;
            GLuint color;
            int size;
        } levels[4];
    } glow;

private:
    void _clear_intermediate_buffers();
    void _clear_back_buffers();
    void _clear_glow_buffers();

public:
    RenderSceneBuffersGLES2();
    virtual ~RenderSceneBuffersGLES2();
    virtual void configure(const RenderSceneBuffersConfiguration *p_config) override;
    void configure_for_probe(Size2i p_size);

    virtual void set_fsr_sharpness(float p_fsr_sharpness) override {}
    virtual void set_texture_mipmap_bias(float p_texture_mipmap_bias) override {}
    virtual void set_use_debanding(bool p_use_debanding) override {}

    void free_render_buffer_data();

    void check_backbuffer(bool p_need_color, bool p_need_depth); // Check if we need to initialize our backbuffer.
    void check_glow_buffers(); // Check if we need to initialize our glow buffers.

    GLuint get_render_fbo();
    GLuint get_internal_fbo() {
        return internal3d.fbo;
    }
    GLuint get_internal_color() {
        return internal3d.color;
    }
    GLuint get_internal_depth() {
        return internal3d.depth;
    }
    GLuint get_backbuffer_fbo() const { return backbuffer3d.fbo; }
    GLuint get_backbuffer() const { return backbuffer3d.color; }
    GLuint get_backbuffer_depth() const { return backbuffer3d.depth; }

    const GLOW::GLOWLEVEL *get_glow_buffers() const { return &glow.levels[0]; }

    // Getters

    _FORCE_INLINE_ RID get_render_target() const { return render_target; }
    _FORCE_INLINE_ uint32_t get_view_count() const { return view_count; }
    _FORCE_INLINE_ Size2i get_internal_size() const { return internal_size; }
    _FORCE_INLINE_ Size2i get_target_size() const { return target_size; }
    _FORCE_INLINE_ RS::ViewportScaling3DMode get_scaling_3d_mode() const { return scaling_3d_mode; }
};

#endif // GLES2_ENABLED

#endif // RENDER_SCENE_BUFFERS_GLES2_H
