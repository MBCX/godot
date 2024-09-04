/**************************************************************************/
/*  copy_effects.cpp                                                      */
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

#include "copy_effects.h"
#include "../storage/texture_storage.h"

using namespace GLES2;

CopyEffects *CopyEffects::singleton = nullptr;

CopyEffects *CopyEffects::get_singleton() {
    return singleton;
}

CopyEffects::CopyEffects() {
    singleton = this;

    copy.shader.initialize();
    copy.shader_version = copy.shader.version_create();
    copy.shader.version_bind_shader(copy.shader_version, CopyShaderGLES2::MODE_DEFAULT);

    { // Screen Quad
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
        glBindBuffer(GL_ARRAY_BUFFER, 0); //unbind
    }
}

void CopyEffects::draw_screen_quad() {
    glBindVertexArrayOES(screen_quad_array);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArrayOES(0);
}

void CopyEffects::copy_to_rect(const Rect2 &p_rect) {
    bool success = copy.shader.version_bind_shader(copy.shader_version, CopyShaderGLES2::MODE_COPY_SECTION);
    if (!success) {
        return;
    }

    copy.shader.version_set_uniform(CopyShaderGLES2::COPY_SECTION, p_rect.position.x, p_rect.position.y, p_rect.size.x, p_rect.size.y, copy.shader_version, CopyShaderGLES2::MODE_COPY_SECTION);
    draw_screen_quad();
}

void CopyEffects::copy_to_rect_2d(const Rect2 &p_rect, float p_lod) {
    bool success = copy.shader.version_bind_shader(copy.shader_version, CopyShaderGLES2::MODE_COPY_SECTION);
    if (!success) {
        return;
    }

    copy.shader.version_set_uniform(CopyShaderGLES2::COPY_SECTION, p_rect.position.x, p_rect.position.y, p_rect.size.x, p_rect.size.y, copy.shader_version, CopyShaderGLES2::MODE_COPY_SECTION);
    copy.shader.version_set_uniform(CopyShaderGLES2::LOD, p_lod, copy.shader_version, CopyShaderGLES2::MODE_COPY_SECTION);
    draw_screen_quad();
}

void CopyEffects::copy_to_and_from_rect(const Rect2 &p_rect) {
    bool success = copy.shader.version_bind_shader(copy.shader_version, CopyShaderGLES2::MODE_COPY_SECTION_SOURCE);
    if (!success) {
        return;
    }

    copy.shader.version_set_uniform(CopyShaderGLES2::COPY_SECTION, p_rect.position.x, p_rect.position.y, p_rect.size.x, p_rect.size.y, copy.shader_version, CopyShaderGLES2::MODE_COPY_SECTION_SOURCE);
    copy.shader.version_set_uniform(CopyShaderGLES2::SOURCE_SECTION, p_rect.position.x, p_rect.position.y, p_rect.size.x, p_rect.size.y, copy.shader_version, CopyShaderGLES2::MODE_COPY_SECTION_SOURCE);

    draw_screen_quad();
}

void CopyEffects::copy_screen(float p_multiply) {
    bool success = copy.shader.version_bind_shader(copy.shader_version, CopyShaderGLES2::MODE_SCREEN);
    if (!success) {
        return;
    }

    copy.shader.version_set_uniform(CopyShaderGLES2::MULTIPLY, p_multiply, copy.shader_version, CopyShaderGLES2::MODE_SCREEN);

    draw_screen_quad();
}

void CopyEffects::copy_cube_to_rect(const Rect2 &p_rect) {
    bool success = copy.shader.version_bind_shader(copy.shader_version, CopyShaderGLES2::MODE_CUBE_TO_OCTAHEDRAL);
    if (!success) {
        return;
    }

    copy.shader.version_set_uniform(CopyShaderGLES2::COPY_SECTION, p_rect.position.x, p_rect.position.y, p_rect.size.x, p_rect.size.y, copy.shader_version, CopyShaderGLES2::MODE_CUBE_TO_OCTAHEDRAL);
    draw_screen_quad();
}

void CopyEffects::copy_cube_to_panorama(float p_mip_level) {
    bool success = copy.shader.version_bind_shader(copy.shader_version, CopyShaderGLES2::MODE_CUBE_TO_PANORAMA);
    if (!success) {
        return;
    }

    copy.shader.version_set_uniform(CopyShaderGLES2::MIP_LEVEL, p_mip_level, copy.shader_version, CopyShaderGLES2::MODE_CUBE_TO_PANORAMA);
    draw_screen_quad();
}

void CopyEffects::bilinear_blur(GLuint p_source_texture, int p_mipmap_count, const Rect2i &p_region) {
    GLuint framebuffers[2];
    glGenFramebuffers(2, framebuffers);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffers[0]);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, p_source_texture, 0);

    Rect2i source_region = p_region;
    Rect2i dest_region = p_region;
    for (int i = 1; i < p_mipmap_count; i++) {
        dest_region.position.x >>= 1;
        dest_region.position.y >>= 1;
        dest_region.size = Size2i(dest_region.size.x >> 1, dest_region.size.y >> 1).max(Size2i(1, 1));
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffers[i % 2]);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, p_source_texture, i);

        // GLES2 doesn't support glBlitFramebuffer, so we need to use a shader-based approach
        glViewport(dest_region.position.x, dest_region.position.y, dest_region.size.x, dest_region.size.y);

        bool success = copy.shader.version_bind_shader(copy.shader_version, CopyShaderGLES2::MODE_BILINEAR_BLUR);
        if (!success) {
            break;
        }

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, p_source_texture);

        copy.shader.version_set_uniform(CopyShaderGLES2::SOURCE_REGION,
            float(source_region.position.x) / float(p_region.size.x),
            float(source_region.position.y) / float(p_region.size.y),
            float(source_region.size.x) / float(p_region.size.x),
            float(source_region.size.y) / float(p_region.size.y),
            copy.shader_version, CopyShaderGLES2::MODE_BILINEAR_BLUR);

        draw_screen_quad();

        glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffers[i % 2]);
        source_region = dest_region;
    }
    glBindFramebuffer(GL_READ_FRAMEBUFFER, GLES2::TextureStorage::system_fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, GLES2::TextureStorage::system_fbo);
    glDeleteFramebuffers(2, framebuffers);
}

void CopyEffects::gaussian_blur(GLuint p_source_texture, int p_mipmap_count, const Rect2i &p_region, const Size2i &p_size) {
    GLuint framebuffer;
    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, p_source_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    Size2i base_size = p_size;

    Rect2i source_region = p_region;
    Rect2i dest_region = p_region;

    Size2 float_size = Size2(p_size);
    Rect2 normalized_source_region = Rect2(p_region);
    normalized_source_region.position = normalized_source_region.position / float_size;
    normalized_source_region.size = normalized_source_region.size / float_size;
    Rect2 normalized_dest_region = Rect2(p_region);
    for (int i = 1; i < p_mipmap_count; i++) {
        dest_region.position.x >>= 1;
        dest_region.position.y >>= 1;
        dest_region.size = Size2i(dest_region.size.x >> 1, dest_region.size.y >> 1).max(Size2i(1, 1));
        base_size.x >>= 1;
        base_size.y >>= 1;

        glBindTexture(GL_TEXTURE_2D, p_source_texture);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, p_source_texture, i);
#ifdef DEV_ENABLED
        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            WARN_PRINT("Could not bind Gaussian blur framebuffer, status: " + GLES2::TextureStorage::get_singleton()->get_framebuffer_error(status));
        }
#endif

        glViewport(0, 0, base_size.x, base_size.y);

        bool success = copy.shader.version_bind_shader(copy.shader_version, CopyShaderGLES2::MODE_GAUSSIAN_BLUR);
        if (!success) {
            break;
        }

        float_size = Size2(base_size);
        normalized_dest_region.position = Size2(dest_region.position) / float_size;
        normalized_dest_region.size = Size2(dest_region.size) / float_size;

        copy.shader.version_set_uniform(CopyShaderGLES2::COPY_SECTION, normalized_dest_region.position.x, normalized_dest_region.position.y, normalized_dest_region.size.x, normalized_dest_region.size.y, copy.shader_version, CopyShaderGLES2::MODE_GAUSSIAN_BLUR);
        copy.shader.version_set_uniform(CopyShaderGLES2::SOURCE_SECTION, normalized_source_region.position.x, normalized_source_region.position.y, normalized_source_region.size.x, normalized_source_region.size.y, copy.shader_version, CopyShaderGLES2::MODE_GAUSSIAN_BLUR);
        copy.shader.version_set_uniform(CopyShaderGLES2::PIXEL_SIZE, 1.0 / float_size.x, 1.0 / float_size.y, copy.shader_version, CopyShaderGLES2::MODE_GAUSSIAN_BLUR);

        draw_screen_quad();

        source_region = dest_region;
        normalized_source_region = normalized_dest_region;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, GLES2::TextureStorage::system_fbo);
    glDeleteFramebuffers(1, &framebuffer);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    glViewport(0, 0, p_size.x, p_size.y);
}

void CopyEffects::set_color(const Color &p_color, const Rect2i &p_region) {
    bool success = copy.shader.version_bind_shader(copy.shader_version, CopyShaderGLES2::MODE_SIMPLE_COLOR);
    if (!success) {
        return;
    }

    copy.shader.version_set_uniform(CopyShaderGLES2::COPY_SECTION, p_region.position.x, p_region.position.y, p_region.size.x, p_region.size.y, copy.shader_version, CopyShaderGLES2::MODE_SIMPLE_COLOR);
    copy.shader.version_set_uniform(CopyShaderGLES2::COLOR_IN, p_color, copy.shader_version, CopyShaderGLES2::MODE_SIMPLE_COLOR);
    draw_screen_quad();
}

CopyEffects::~CopyEffects() {
    singleton = nullptr;
    glDeleteBuffers(1, &screen_quad);
    glDeleteVertexArraysOES(1, &screen_quad_array);
    copy.shader.version_free(copy.shader_version);
}

#endif // GLES2_ENABLED
