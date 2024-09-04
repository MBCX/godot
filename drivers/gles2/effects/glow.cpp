/**************************************************************************/
/*  glow.cpp                                                              */
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

#ifdef GLES2_ENABLED

#include "glow.h"
#include "../storage/texture_storage.h"

using namespace GLES2;

Glow *Glow::singleton = nullptr;

Glow *Glow::get_singleton() {
    return singleton;
}

Glow::Glow() {
    singleton = this;

    glow.shader.initialize();
    glow.shader_version = glow.shader.version_create();

    {
        // Screen Quad
        glGenBuffers(1, &screen_quad);
        glBindBuffer(GL_ARRAY_BUFFER, screen_quad);

        const float qv[8] = {
            -1.0f, -1.0f,
            1.0f, -1.0f,
            -1.0f, 1.0f,
            1.0f, 1.0f,
        };

        glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 8, qv, GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0); //unbind

        glGenVertexArraysOES(1, &screen_quad_array);
        glBindVertexArrayOES(screen_quad_array);
        glBindBuffer(GL_ARRAY_BUFFER, screen_quad);
        glVertexAttribPointer(RS::ARRAY_VERTEX, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, nullptr);
        glEnableVertexAttribArray(RS::ARRAY_VERTEX);
        glBindVertexArrayOES(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0); // Unbind
    }
}

Glow::~Glow() {
    glDeleteBuffers(1, &screen_quad);
    glDeleteVertexArraysOES(1, &screen_quad_array);

    glow.shader.version_free(glow.shader_version);

    singleton = nullptr;
}

void Glow::_draw_screen_quad() {
    glBindVertexArrayOES(screen_quad_array);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArrayOES(0);
}

void Glow::process_glow(GLuint p_source_color, Size2i p_size, const Glow::GLOWLEVEL *p_glow_buffers) {
    ERR_FAIL_COND(p_source_color == 0);
    ERR_FAIL_COND(p_glow_buffers[3].color == 0);

    // Reset some OpenGL state...
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    // Start with our filter pass
    {
        glBindFramebuffer(GL_FRAMEBUFFER, p_glow_buffers[0].fbo);
        glViewport(0, 0, p_glow_buffers[0].size.x, p_glow_buffers[0].size.y);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, p_source_color);

        bool success = glow.shader.version_bind_shader(glow.shader_version, GlowShaderGLES2::MODE_FILTER);
        if (!success) {
            return;
        }

        glow.shader.version_set_uniform(GlowShaderGLES2::PIXEL_SIZE, 1.0 / p_glow_buffers[0].size.x, 1.0 / p_glow_buffers[0].size.y, glow.shader_version, GlowShaderGLES2::MODE_FILTER);
        glow.shader.version_set_uniform(GlowShaderGLES2::LUMINANCE_MULTIPLIER, luminance_multiplier, glow.shader_version, GlowShaderGLES2::MODE_FILTER);
        glow.shader.version_set_uniform(GlowShaderGLES2::GLOW_BLOOM, glow_bloom, glow.shader_version, GlowShaderGLES2::MODE_FILTER);
        glow.shader.version_set_uniform(GlowShaderGLES2::GLOW_HDR_THRESHOLD, glow_hdr_bleed_threshold, glow.shader_version, GlowShaderGLES2::MODE_FILTER);
        glow.shader.version_set_uniform(GlowShaderGLES2::GLOW_HDR_SCALE, glow_hdr_bleed_scale, glow.shader_version, GlowShaderGLES2::MODE_FILTER);
        glow.shader.version_set_uniform(GlowShaderGLES2::GLOW_LUMINANCE_CAP, glow_hdr_luminance_cap, glow.shader_version, GlowShaderGLES2::MODE_FILTER);

        _draw_screen_quad();
    }

    // Continue with downsampling
    {
        bool success = glow.shader.version_bind_shader(glow.shader_version, GlowShaderGLES2::MODE_DOWNSAMPLE);
        if (!success) {
            return;
        }

        for (int i = 1; i < 4; i++) {
            glBindFramebuffer(GL_FRAMEBUFFER, p_glow_buffers[i].fbo);
            glViewport(0, 0, p_glow_buffers[i].size.x, p_glow_buffers[i].size.y);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, p_glow_buffers[i - 1].color);

            glow.shader.version_set_uniform(GlowShaderGLES2::PIXEL_SIZE, 1.0 / p_glow_buffers[i].size.x, 1.0 / p_glow_buffers[i].size.y, glow.shader_version, GlowShaderGLES2::MODE_DOWNSAMPLE);

            _draw_screen_quad();
        }
    }

    // Now upsample
    {
        bool success = glow.shader.version_bind_shader(glow.shader_version, GlowShaderGLES2::MODE_UPSAMPLE);
        if (!success) {
            return;
        }

        for (int i = 2; i >= 0; i--) {
            glBindFramebuffer(GL_FRAMEBUFFER, p_glow_buffers[i].fbo);
            glViewport(0, 0, p_glow_buffers[i].size.x, p_glow_buffers[i].size.y);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, p_glow_buffers[i + 1].color);

            glow.shader.version_set_uniform(GlowShaderGLES2::PIXEL_SIZE, 1.0 / p_glow_buffers[i].size.x, 1.0 / p_glow_buffers[i].size.y, glow.shader_version, GlowShaderGLES2::MODE_UPSAMPLE);

            _draw_screen_quad();
        }
    }

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glUseProgram(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, GLES2::TextureStorage::system_fbo);
}

#endif // GLES2_ENABLED
