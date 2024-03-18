/**************************************************************************/
/*  shader_gles2.h                                                         */
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

#ifndef SHADER_GLES2_H
#define SHADER_GLES2_H

#if defined GLES2_ENABLED

#include "drivers/gles_common/shader_gles.h"

class ShaderGLES2 : public ShaderGLES {
protected:
	struct AttributePair {
		const char *name;
		int index;
	};

private:
	const AttributePair *attribute_pairs = nullptr;
	int attribute_pair_count = 0;

protected:
	virtual void _get_ubo_locations() override;
	virtual void _pre_link_program(Version::Specialization &spec) override;
	virtual int _get_glsl_version(bool p_gles_over_gl) override;

	virtual void _build_multiview_code(StringBuilder &p_builder, bool p_is_vertex_stage) override;

	void _setup(const char *p_vertex_code, const char *p_fragment_code, const char *p_name, int p_uniform_count, const char **p_uniform_names, int p_attribute_pair_count, const AttributePair *p_attribute_pairs, int p_texture_count, const TexUnitPair *p_tex_units, int p_specialization_count, const Specialization *p_specializations, int p_variant_count, const char **p_variants);
};

#endif //GLES2_ENABLED
#endif
