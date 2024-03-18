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

#ifndef COPY_EFFECTS_GLES_H
#define COPY_EFFECTS_GLES_H

#if defined(GLES3_ENABLED) || defined(GLES2_ENABLED)

#include "core/math/color.h"
#include "core/math/rect2.h"
#include "core/math/vector2i.h"
#include "core/templates/rid.h"
#include "core/typedefs.h"
#include "platform_gl.h"

//TODO: remove me!
//#include "drivers/gles3/shaders/copy.glsl.gen.h"

namespace GLES {

class CopyEffects {
private:
	static CopyEffects *singleton;

protected:
	// Use for full-screen effects. Slightly more efficient than screen_quad as this eliminates pixel overdraw along the diagonal.
	GLuint screen_triangle = 0;
	GLuint screen_triangle_array = 0;

	// Use for rect-based effects.
	GLuint quad = 0;
	GLuint quad_array = 0;

	// Shader stuff
	enum ShaderUniforms {
		COPY_SECTION,
		SOURCE_SECTION,
		LAYER,
		LOD,
		COLOR_IN,
		MULTIPLY,
		PIXEL_SIZE,
		MIP_LEVEL,
	};

	enum ShaderVariants {
		MODE_DEFAULT,
		MODE_COPY_SECTION,
		MODE_COPY_SECTION_SOURCE,
		MODE_COPY_SECTION_3D,
		MODE_COPY_SECTION_2D_ARRAY,
		MODE_SCREEN,
		MODE_GAUSSIAN_BLUR,
		MODE_MIPMAP,
		MODE_SIMPLE_COLOR,
		MODE_CUBE_TO_OCTAHEDRAL,
		MODE_CUBE_TO_PANORAMA,
	};

	virtual bool _bind_shader(ShaderVariants p_variant) = 0;
	virtual void _shader_set_uniform(ShaderUniforms p_uniform, float p_a, ShaderVariants p_variant) = 0;
	virtual void _shader_set_uniform(ShaderUniforms p_uniform, float p_a, float p_b, ShaderVariants p_variant) = 0;
	virtual void _shader_set_uniform(ShaderUniforms p_uniform, float p_a, float p_b, float p_c, float p_d, ShaderVariants p_variant) = 0;
	virtual void _shader_set_uniform(ShaderUniforms p_uniform, const Color &p_color, ShaderVariants p_variant) = 0;

public:
	static CopyEffects *get_singleton();

	CopyEffects();
	~CopyEffects();

	// These functions assume that a framebuffer and texture are bound already. They only manage the shader, uniforms, and vertex array.
	void copy_to_rect(const Rect2 &p_rect);
	void copy_to_rect_3d(const Rect2 &p_rect, float p_layer, int p_type, float p_lod = 0.0f);
	void copy_to_and_from_rect(const Rect2 &p_rect);
	void copy_screen(float p_multiply = 1.0);
	void copy_cube_to_rect(const Rect2 &p_rect);
	void copy_cube_to_panorama(float p_mip_level);
	void bilinear_blur(GLuint p_source_texture, int p_mipmap_count, const Rect2i &p_region);
	void gaussian_blur(GLuint p_source_texture, int p_mipmap_count, const Rect2i &p_region, const Size2i &p_size);
	void set_color(const Color &p_color, const Rect2i &p_region);

	virtual void draw_screen_triangle() = 0;
	virtual void draw_screen_quad() = 0;
};

} //namespace GLES

#endif // GLES3_ENABLED || GLES2_ENABLED

#endif // COPY_EFFECTS_GLES_H
