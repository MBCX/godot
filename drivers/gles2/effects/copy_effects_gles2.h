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

#if defined(GLES2_ENABLED)

#include "drivers/gles2/shaders/effects/copy.glsl.gen.h"
#include "drivers/gles_common/effects/copy_effects.h"

namespace GLES {

class CopyEffectsGLES2 : public GLES::CopyEffects {
private:
	struct Copy {
		CopyShaderGLES2 shader;
		RID shader_version;
	} copy;

	CopyShaderGLES2::ShaderVariant _to_shader_variant(ShaderVariants p_variant);

	CopyShaderGLES2::Uniforms _to_shader_uniform(ShaderUniforms p_uniform);

protected:
	virtual bool _bind_shader(ShaderVariants p_variant);
	virtual void _shader_set_uniform(ShaderUniforms p_uniform, float p_a, ShaderVariants p_variant);
	virtual void _shader_set_uniform(ShaderUniforms p_uniform, float p_a, float p_b, ShaderVariants p_variant);
	virtual void _shader_set_uniform(ShaderUniforms p_uniform, float p_a, float p_b, float p_c, float p_d, ShaderVariants p_variant);
	virtual void _shader_set_uniform(ShaderUniforms p_uniform, const Color &p_color, ShaderVariants p_variant);

public:
	CopyEffectsGLES2();
	~CopyEffectsGLES2();

	virtual void draw_screen_triangle();
	virtual void draw_screen_quad();
};
} //namespace GLES

#endif // GLES2_ENABLED
#endif // COPY_EFFECTS_GLES2_H