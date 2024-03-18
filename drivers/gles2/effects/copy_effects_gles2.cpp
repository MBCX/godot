/**************************************************************************/
/*  copy_effects_gles2.cpp                                                */
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

#if defined(GLES2_ENABLED)

#include "copy_effects_gles2.h"
#include "servers/rendering_server.h"

using namespace GLES;

//TODO: add the missing cases

CopyShaderGLES2::ShaderVariant CopyEffectsGLES2::_to_shader_variant(ShaderVariants p_variant) {
	switch (p_variant) {
		case ShaderVariants::MODE_COPY_SECTION_SOURCE:
		case ShaderVariants::MODE_COPY_SECTION_3D:
		case ShaderVariants::MODE_COPY_SECTION_2D_ARRAY:

		case ShaderVariants::MODE_DEFAULT:
			return CopyShaderGLES2::ShaderVariant::MODE_DEFAULT;
		case ShaderVariants::MODE_COPY_SECTION:
			return CopyShaderGLES2::ShaderVariant::MODE_COPY_SECTION;
		case ShaderVariants::MODE_CUBE_TO_OCTAHEDRAL:
			return CopyShaderGLES2::ShaderVariant::MODE_CUBE_TO_OCTAHEDRAL;
		case ShaderVariants::MODE_GAUSSIAN_BLUR:
			return CopyShaderGLES2::ShaderVariant::MODE_GAUSSIAN_BLUR;
		case ShaderVariants::MODE_MIPMAP:
			return CopyShaderGLES2::ShaderVariant::MODE_MIPMAP;
		case ShaderVariants::MODE_SIMPLE_COLOR:
			return CopyShaderGLES2::ShaderVariant::MODE_SIMPLE_COLOR;
		case ShaderVariants::MODE_SCREEN:
			return CopyShaderGLES2::ShaderVariant::MODE_SCREEN;
		case ShaderVariants::MODE_CUBE_TO_PANORAMA:
			return CopyShaderGLES2::ShaderVariant::MODE_CUBE_TO_PANORAMA;
	};

	return CopyShaderGLES2::ShaderVariant::MODE_DEFAULT;
}

CopyShaderGLES2::Uniforms CopyEffectsGLES2::_to_shader_uniform(ShaderUniforms p_uniform) {
	switch (p_uniform) {
		case ShaderUniforms::COPY_SECTION:
			return CopyShaderGLES2::Uniforms::COPY_SECTION;
		case ShaderUniforms::COLOR_IN:
			return CopyShaderGLES2::Uniforms::COLOR_IN;
		case ShaderUniforms::PIXEL_SIZE:
			return CopyShaderGLES2::Uniforms::PIXEL_SIZE;
		case ShaderUniforms::SOURCE_SECTION:
			return CopyShaderGLES2::Uniforms::SOURCE_SECTION;
		case ShaderUniforms::MULTIPLY:
			return CopyShaderGLES2::Uniforms::MULTIPLY;
		case ShaderUniforms::MIP_LEVEL:
			return CopyShaderGLES2::Uniforms::MIP_LEVEL;
		case ShaderUniforms::LAYER:
		case ShaderUniforms::LOD: {
			//TODO: implement me
			ERR_FAIL_V_MSG(CopyShaderGLES2::Uniforms::COLOR_IN, "ShaderUniforms::LAYER and ShaderUniforms::LOD haven't been implemented for GLES2 yet.");
		}
	}
}

bool CopyEffectsGLES2::_bind_shader(ShaderVariants p_variant) {
	return copy.shader.version_bind_shader(copy.shader_version, _to_shader_variant(p_variant));
}

void CopyEffectsGLES2::_shader_set_uniform(ShaderUniforms p_uniform, float p_a, ShaderVariants p_variant) {
	copy.shader.version_set_uniform(_to_shader_uniform(p_uniform), p_a, copy.shader_version, _to_shader_variant(p_variant));
}
void CopyEffectsGLES2::_shader_set_uniform(ShaderUniforms p_uniform, float p_a, float p_b, ShaderVariants p_variant) {
	copy.shader.version_set_uniform(_to_shader_uniform(p_uniform), p_a, p_b, copy.shader_version, _to_shader_variant(p_variant));
}
void CopyEffectsGLES2::_shader_set_uniform(ShaderUniforms p_uniform, float p_a, float p_b, float p_c, float p_d, ShaderVariants p_variant) {
	copy.shader.version_set_uniform(_to_shader_uniform(p_uniform), p_a, p_b, p_c, p_d, copy.shader_version, _to_shader_variant(p_variant));
}
void CopyEffectsGLES2::_shader_set_uniform(ShaderUniforms p_uniform, const Color &p_color, ShaderVariants p_variant) {
	copy.shader.version_set_uniform(_to_shader_uniform(p_uniform), p_color, copy.shader_version, _to_shader_variant(p_variant));
}

CopyEffectsGLES2::CopyEffectsGLES2() {
	copy.shader.initialize();
	copy.shader_version = copy.shader.version_create();
	copy.shader.version_bind_shader(copy.shader_version, CopyShaderGLES2::MODE_DEFAULT);

	//create vao in gles3 only
	/*
		//Triangle
		glGenVertexArrays(1, &screen_triangle_array);
		glBindVertexArray(screen_triangle_array);
		glBindBuffer(GL_ARRAY_BUFFER, screen_triangle);
		glVertexAttribPointer(RS::ARRAY_VERTEX, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, nullptr);
		glEnableVertexAttribArray(RS::ARRAY_VERTEX);
		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0); //unbind

		//Quad
		glGenVertexArrays(1, &quad_array);
		glBindVertexArray(quad_array);
		glBindBuffer(GL_ARRAY_BUFFER, quad);
		glVertexAttribPointer(RS::ARRAY_VERTEX, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, nullptr);
		glEnableVertexAttribArray(RS::ARRAY_VERTEX);
		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0); //unbind
	*/
}
CopyEffectsGLES2::~CopyEffectsGLES2() {
	copy.shader.version_free(copy.shader_version);

	// Delete VAO in gles3
	/*
		glDeleteVertexArrays(1, &screen_triangle_array);
		glDeleteVertexArrays(1, &quad_array);
	*/
}

void CopyEffectsGLES2::draw_screen_triangle() {
	// in gles3 use vao instead
	/*
		glBindVertexArray(screen_triangle_array);
		glDrawArrays(GL_TRIANGLES, 0, 3);
		glBindVertexArray(0);
	*/

	glBindBuffer(GL_ARRAY_BUFFER, screen_triangle);
	glVertexAttribPointer(RS::ARRAY_VERTEX, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, nullptr);
	glEnableVertexAttribArray(RS::ARRAY_VERTEX);

	glDrawArrays(GL_TRIANGLES, 0, 3);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void CopyEffectsGLES2::draw_screen_quad() {
	// in gles3 use vao instead
	/*
		glBindVertexArray(quad_array);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		glBindVertexArray(0);
	*/

	glBindBuffer(GL_ARRAY_BUFFER, quad);
	glVertexAttribPointer(RS::ARRAY_VERTEX, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 2, nullptr);
	glEnableVertexAttribArray(RS::ARRAY_VERTEX);

	glDrawArrays(GL_TRIANGLES, 0, 6);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

#endif // defined(GLES2_ENABLED)