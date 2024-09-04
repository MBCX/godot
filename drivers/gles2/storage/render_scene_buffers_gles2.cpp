/**************************************************************************/
/*  render_scene_buffers_gles2.cpp                                        */
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

#include "render_scene_buffers_gles2.h"
#include "config.h"
#include "texture_storage.h"
#include "utilities.h"

using namespace GLES2;

RenderSceneBuffersGLES2::RenderSceneBuffersGLES2() {
    for (int i = 0; i < 4; i++) {
        glow.levels[i].color = 0;
        glow.levels[i].fbo = 0;
    }
}

RenderSceneBuffersGLES2::~RenderSceneBuffersGLES2() {
    free_render_buffer_data();
}

void RenderSceneBuffersGLES2::configure(const RenderSceneBuffersConfiguration *p_config) {
    GLES2::TextureStorage *texture_storage = GLES2::TextureStorage::get_singleton();
    GLES2::Config *config = GLES2::Config::get_singleton();

    free_render_buffer_data();

    internal_size = p_config->get_internal_size();
    target_size = p_config->get_target_size();
    scaling_3d_mode = p_config->get_scaling_3d_mode();
    render_target = p_config->get_render_target();
    view_count = 1; // GLES2 doesn't support multiview

    // Get color format data from our render target so we match those
    if (render_target.is_valid()) {
        color_internal_format = texture_storage->render_target_get_color_internal_format(render_target);
        color_format = texture_storage->render_target_get_color_format(render_target);
        color_type = texture_storage->render_target_get_color_type(render_target);
        color_format_size = texture_storage->render_target_get_color_format_size(render_target);
    } else {
        // reflection probe? or error?
        color_internal_format = GL_RGBA;
        color_format = GL_RGBA;
        color_type = GL_UNSIGNED_BYTE;
        color_format_size = 4;
    }

    // Check our scaling mode
    if (scaling_3d_mode != RS::VIEWPORT_SCALING_3D_MODE_OFF && internal_size.x == 0 && internal_size.y == 0) {
        // Disable, no size set.
        scaling_3d_mode = RS::VIEWPORT_SCALING_3D_MODE_OFF;
    } else if (scaling_3d_mode != RS::VIEWPORT_SCALING_3D_MODE_OFF && internal_size == target_size) {
        // If size matches, we won't use scaling.
        scaling_3d_mode = RS::VIEWPORT_SCALING_3D_MODE_OFF;
    }

    // We don't create our buffers right away because post effects
	// can be made active at any time and change our buffer configuration.
}

void RenderSceneBuffersGLES2::_check_render_buffers() {
    GLES2::TextureStorage *texture_storage = GLES2::TextureStorage::get_singleton();

    ERR_FAIL_COND(view_count == 0);

    bool use_internal_buffer = scaling_3d_mode != RS::VIEWPORT_SCALING_3D_MODE_OFF;

    if ((!use_internal_buffer || internal3d.color != 0)) {
        // already setup!
        return;
    }

    if (use_internal_buffer && internal3d.color == 0) {
        // Setup our internal buffer.
        glGenTextures(1, &internal3d.color);
        glBindTexture(GL_TEXTURE_2D, internal3d.color);

        glTexImage2D(GL_TEXTURE_2D, 0, color_internal_format, internal_size.x, internal_size.y, 0, color_format, color_type, nullptr);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        GLES2::Utilities::get_singleton()->texture_allocated_data(internal3d.color, internal_size.x * internal_size.y * color_format_size, "3D color texture");

        // Create our depth buffer.
        glGenRenderbuffers(1, &internal3d.depth);
        glBindRenderbuffer(GL_RENDERBUFFER, internal3d.depth);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, internal_size.x, internal_size.y);

        GLES2::Utilities::get_singleton()->render_buffer_allocated_data(internal3d.depth, internal_size.x * internal_size.y * 2, "3D depth buffer");

        // Create our internal 3D FBO.
        glGenFramebuffers(1, &internal3d.fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, internal3d.fbo);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, internal3d.color, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, internal3d.depth);

        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            _clear_intermediate_buffers();
            WARN_PRINT("Could not create 3D internal buffers, status: " + texture_storage->get_framebuffer_error(status));
        }

        glBindTexture(GL_TEXTURE_2D, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, GLES2::TextureStorage::system_fbo);
    }
}

void RenderSceneBuffersGLES2::configure_for_probe(Size2i p_size) {
    internal_size = p_size;
    target_size = p_size;
    scaling_3d_mode = RS::VIEWPORT_SCALING_3D_MODE_OFF;
    view_count = 1;
}

void RenderSceneBuffersGLES2::_clear_intermediate_buffers() {
    if (internal3d.fbo) {
        glDeleteFramebuffers(1, &internal3d.fbo);
        internal3d.fbo = 0;
    }

    if (internal3d.color != 0) {
        GLES2::Utilities::get_singleton()->texture_free_data(internal3d.color);
        internal3d.color = 0;
    }

    if (internal3d.depth != 0) {
        GLES2::Utilities::get_singleton()->render_buffer_free_data(internal3d.depth);
        internal3d.depth = 0;
    }
}

void RenderSceneBuffersGLES2::check_backbuffer(bool p_need_color, bool p_need_depth) {
    GLES2::TextureStorage *texture_storage = GLES2::TextureStorage::get_singleton();

    // Setup our back buffer

    if (backbuffer3d.fbo == 0) {
        glGenFramebuffers(1, &backbuffer3d.fbo);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, backbuffer3d.fbo);

    if (backbuffer3d.color == 0 && p_need_color) {
        glGenTextures(1, &backbuffer3d.color);
        glBindTexture(GL_TEXTURE_2D, backbuffer3d.color);

        glTexImage2D(GL_TEXTURE_2D, 0, color_internal_format, internal_size.x, internal_size.y, 0, color_format, color_type, nullptr);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        GLES2::Utilities::get_singleton()->texture_allocated_data(backbuffer3d.color, internal_size.x * internal_size.y * color_format_size, "3D Back buffer color texture");

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, backbuffer3d.color, 0);
    }

    if (backbuffer3d.depth == 0 && p_need_depth) {
        glGenRenderbuffers(1, &backbuffer3d.depth);
        glBindRenderbuffer(GL_RENDERBUFFER, backbuffer3d.depth);

        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, internal_size.x, internal_size.y);

        GLES2::Utilities::get_singleton()->render_buffer_allocated_data(backbuffer3d.depth, internal_size.x * internal_size.y * 2, "3D back buffer depth buffer");

        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, backbuffer3d.depth);
    }

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        _clear_back_buffers();
        WARN_PRINT("Could not create 3D back buffers, status: " + texture_storage->get_framebuffer_error(status));
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, GLES2::TextureStorage::system_fbo);
}

void RenderSceneBuffersGLES2::_clear_back_buffers() {
    if (backbuffer3d.fbo) {
        glDeleteFramebuffers(1, &backbuffer3d.fbo);
        backbuffer3d.fbo = 0;
    }

    if (backbuffer3d.color != 0) {
        GLES2::Utilities::get_singleton()->texture_free_data(backbuffer3d.color);
        backbuffer3d.color = 0;
    }

    if (backbuffer3d.depth != 0) {
        GLES2::Utilities::get_singleton()->render_buffer_free_data(backbuffer3d.depth);
        backbuffer3d.depth = 0;
    }
}

void RenderSceneBuffersGLES2::check_glow_buffers() {
    if (glow.levels[0].color != 0) {
        // already have these setup..
        return;
    }

    GLES2::TextureStorage *texture_storage = GLES2::TextureStorage::get_singleton();
    Size2i level_size = internal_size;
    for (int i = 0; i < 4; i++) {
        level_size = Size2i(level_size.x >> 1, level_size.y >> 1).max(Size2i(4, 4));

        glow.levels[i].size = level_size;

        // Create our texture
        glGenTextures(1, &glow.levels[i].color);
        glBindTexture(GL_TEXTURE_2D, glow.levels[i].color);

        glTexImage2D(GL_TEXTURE_2D, 0, color_internal_format, level_size.x, level_size.y, 0, color_format, color_type, nullptr);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        GLES2::Utilities::get_singleton()->texture_allocated_data(glow.levels[i].color, level_size.x * level_size.y * color_format_size, String("Glow buffer ") + String::num_int64(i));

        // Create our FBO
        glGenFramebuffers(1, &glow.levels[i].fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, glow.levels[i].fbo);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, glow.levels[i].color, 0);

        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            WARN_PRINT("Could not create glow buffers, status: " + texture_storage->get_framebuffer_error(status));
            _clear_glow_buffers();
            break;
        }
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, GLES2::TextureStorage::system_fbo);
}

void RenderSceneBuffersGLES2::_clear_glow_buffers() {
    for (int i = 0; i < 4; i++) {
        if (glow.levels[i].fbo != 0) {
            glDeleteFramebuffers(1, &glow.levels[i].fbo);
            glow.levels[i].fbo = 0;
        }

        if (glow.levels[i].color != 0) {
            GLES2::Utilities::get_singleton()->texture_free_data(glow.levels[i].color);
            glow.levels[i].color = 0;
        }
    }
}

#endif // GLES2_ENABLED
