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

#if defined(GLES3_ENABLED) || defined(GLES2_ENABLED)

#include "copy_effects.h"
#include "../storage/texture_storage.h"

using namespace GLES;

CopyEffects *CopyEffects::singleton = nullptr;

CopyEffects *CopyEffects::get_singleton() {
	return singleton;
}

CopyEffects::CopyEffects() {
	singleton = this;

	//TODO:
	{ // Screen Triangle.
		glGenBuffers(1, &screen_triangle);
		glBindBuffer(GL_ARRAY_BUFFER, screen_triangle);

		const float qv[6] = {
			-1.0f,
			-1.0f,
			3.0f,
			-1.0f,
			-1.0f,
			3.0f,
		};

		glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6, qv, GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0); //unbind
	}

	{ // Screen Quad

		glGenBuffers(1, &quad);
		glBindBuffer(GL_ARRAY_BUFFER, quad);

		const float qv[12] = {
			-1.0f,
			-1.0f,
			1.0f,
			-1.0f,
			1.0f,
			1.0f,
			-1.0f,
			-1.0f,
			1.0f,
			1.0f,
			-1.0f,
			1.0f,
		};

		glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 12, qv, GL_STATIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0); //unbind
	}

	// Child classes need to:
	//	set up VAO (if applicable)
	//
}

CopyEffects::~CopyEffects() {
	singleton = nullptr;
	glDeleteBuffers(1, &screen_triangle);
	glDeleteBuffers(1, &quad);
}

void CopyEffects::copy_to_rect(const Rect2 &p_rect) {
	bool success = _bind_shader(ShaderVariants::MODE_COPY_SECTION);
	ERR_FAIL_COND_MSG(!success, "Failed to bind CopyEffect Shader during CopyEffects::copy_to_rect!");

	_shader_set_uniform(ShaderUniforms::COPY_SECTION, p_rect.position.x, p_rect.position.y, p_rect.size.x, p_rect.size.y, ShaderVariants::MODE_COPY_SECTION);
	draw_screen_quad();
}

void CopyEffects::copy_to_rect_3d(const Rect2 &p_rect, float p_layer, int p_type, float p_lod) {
	ERR_FAIL_COND(p_type != Texture::TYPE_LAYERED && p_type != Texture::TYPE_3D);

	ShaderVariants variant = p_type == Texture::TYPE_LAYERED
			? ShaderVariants::MODE_COPY_SECTION_2D_ARRAY
			: ShaderVariants::MODE_COPY_SECTION_3D;

	bool success = _bind_shader(variant);
	ERR_FAIL_COND_MSG(!success, "Failed to bind CopyEffect Shader during CopyEffects::copy_to_rect_3d!");

	_shader_set_uniform(ShaderUniforms::COPY_SECTION, p_rect.position.x, p_rect.position.y, p_rect.size.x, p_rect.size.y, variant);
	_shader_set_uniform(ShaderUniforms::LAYER, p_layer, variant);
	_shader_set_uniform(ShaderUniforms::LOD, p_lod, variant);
	draw_screen_quad();
}

void CopyEffects::copy_to_and_from_rect(const Rect2 &p_rect) {
	bool success = _bind_shader(ShaderVariants::MODE_COPY_SECTION_SOURCE);
	ERR_FAIL_COND_MSG(!success, "Failed to bind CopyEffect Shader during CopyEffects::copy_to_and_from_rect!");

	_shader_set_uniform(ShaderUniforms::COPY_SECTION, p_rect.position.x, p_rect.position.y, p_rect.size.x, p_rect.size.y, ShaderVariants::MODE_COPY_SECTION_SOURCE);
	_shader_set_uniform(ShaderUniforms::SOURCE_SECTION, p_rect.position.x, p_rect.position.y, p_rect.size.x, p_rect.size.y, ShaderVariants::MODE_COPY_SECTION_SOURCE);

	draw_screen_quad();
}

void CopyEffects::copy_screen(float p_multiply) {
	bool success = _bind_shader(ShaderVariants::MODE_SCREEN);
	ERR_FAIL_COND_MSG(!success, "Failed to bind CopyEffect Shader during CopyEffects::copy_screen!");

	_shader_set_uniform(ShaderUniforms::MULTIPLY, p_multiply, ShaderVariants::MODE_SCREEN);

	draw_screen_triangle();
}

void CopyEffects::copy_cube_to_rect(const Rect2 &p_rect) {
	bool success = _bind_shader(ShaderVariants::MODE_CUBE_TO_OCTAHEDRAL);
	ERR_FAIL_COND_MSG(!success, "Failed to bind CopyEffect Shader during CopyEffects::copy_cube_to_rect!");

	_shader_set_uniform(ShaderUniforms::COPY_SECTION, p_rect.position.x, p_rect.position.y, p_rect.size.x, p_rect.size.y, ShaderVariants::MODE_CUBE_TO_OCTAHEDRAL);
	draw_screen_quad();
}

void CopyEffects::copy_cube_to_panorama(float p_mip_level) {
	bool success = _bind_shader(ShaderVariants::MODE_CUBE_TO_PANORAMA);
	ERR_FAIL_COND_MSG(!success, "Failed to bind CopyEffect Shader during CopyEffects::copy_cube_to_panorama!");

	_shader_set_uniform(ShaderUniforms::MIP_LEVEL, p_mip_level, ShaderVariants::MODE_CUBE_TO_PANORAMA);
	draw_screen_quad();
}

// Intended for efficiently mipmapping textures.
void CopyEffects::bilinear_blur(GLuint p_source_texture, int p_mipmap_count, const Rect2i &p_region) {
	//TODO: revive this for gles3 only
	/*
	GLuint framebuffers[2];
	glGenFramebuffers(2, framebuffers);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffers[0]);
	glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, p_source_texture, 0);

	Rect2i source_region = p_region;
	Rect2i dest_region = p_region;
	for (int i = 1; i < p_mipmap_count; i++) {
		dest_region.position.x >>= 1;
		dest_region.position.y >>= 1;
		dest_region.size.x = MAX(1, dest_region.size.x >> 1);
		dest_region.size.y = MAX(1, dest_region.size.y >> 1);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffers[i % 2]);
		glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, p_source_texture, i);
		glBlitFramebuffer(source_region.position.x, source_region.position.y, source_region.position.x + source_region.size.x, source_region.position.y + source_region.size.y,
				dest_region.position.x, dest_region.position.y, dest_region.position.x + dest_region.size.x, dest_region.position.y + dest_region.size.y, GL_COLOR_BUFFER_BIT, GL_LINEAR);
		glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffers[i % 2]);
		source_region = dest_region;
	}
	glBindFramebuffer(GL_READ_FRAMEBUFFER, TextureStorage::system_fbo);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, TextureStorage::system_fbo);
	glDeleteFramebuffers(2, framebuffers);
	*/
}

// Intended for approximating a gaussian blur. Used for 2D backbuffer mipmaps. Slightly less efficient than bilinear_blur().
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
		dest_region.size.x = MAX(1, dest_region.size.x >> 1);
		dest_region.size.y = MAX(1, dest_region.size.y >> 1);
		base_size.x >>= 1;
		base_size.y >>= 1;

		glBindTexture(GL_TEXTURE_2D, p_source_texture);
		//TODO: renable me for gles3
		//		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, i - 1);
		//		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, i - 1);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, p_source_texture, i);
#ifdef DEV_ENABLED
		GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (status != GL_FRAMEBUFFER_COMPLETE) {
			WARN_PRINT("Could not bind Gaussian blur framebuffer, status: " + GLES::TextureStorage::get_singleton()->get_framebuffer_error(status));
		}
#endif

		glViewport(0, 0, base_size.x, base_size.y);

		bool success = _bind_shader(ShaderVariants::MODE_GAUSSIAN_BLUR);
		if (!success) {
			return;
		}

		float_size = Size2(base_size);
		normalized_dest_region.position = Size2(dest_region.position) / float_size;
		normalized_dest_region.size = Size2(dest_region.size) / float_size;

		_shader_set_uniform(ShaderUniforms::COPY_SECTION, normalized_dest_region.position.x, normalized_dest_region.position.y, normalized_dest_region.size.x, normalized_dest_region.size.y, ShaderVariants::MODE_GAUSSIAN_BLUR);
		_shader_set_uniform(ShaderUniforms::SOURCE_SECTION, normalized_source_region.position.x, normalized_source_region.position.y, normalized_source_region.size.x, normalized_source_region.size.y, ShaderVariants::MODE_GAUSSIAN_BLUR);
		_shader_set_uniform(ShaderUniforms::PIXEL_SIZE, 1.0 / float_size.x, 1.0 / float_size.y, ShaderVariants::MODE_GAUSSIAN_BLUR);

		draw_screen_quad();

		source_region = dest_region;
		normalized_source_region = normalized_dest_region;
	}
	glBindFramebuffer(GL_FRAMEBUFFER, TextureStorage::system_fbo);
	glDeleteFramebuffers(1, &framebuffer);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	//TODO: renable me for gles3
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, p_mipmap_count - 1);
	glBindTexture(GL_TEXTURE_2D, 0);

	glViewport(0, 0, p_size.x, p_size.y);
}

void CopyEffects::set_color(const Color &p_color, const Rect2i &p_region) {
	bool success = _bind_shader(ShaderVariants::MODE_SIMPLE_COLOR);
	ERR_FAIL_COND_MSG(!success, "Failed to bind CopyEffect Shader during CopyEffects::set_color!");

	_shader_set_uniform(ShaderUniforms::COPY_SECTION, p_region.position.x, p_region.position.y, p_region.size.x, p_region.size.y, ShaderVariants::MODE_SIMPLE_COLOR);
	_shader_set_uniform(ShaderUniforms::COLOR_IN, p_color, ShaderVariants::MODE_SIMPLE_COLOR);
	draw_screen_quad();
}

#endif // GLES3_ENABLED
