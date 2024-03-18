/**************************************************************************/
/*  shader_gles2.cpp                                                      */
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

#include "shader_gles2.h"

#if defined(GLES2_ENABLED)
void ShaderGLES2::_get_ubo_locations() {
	//we only need this in gles3
	/*
		for (int i = 0; i < ubo_count; i++) {
			GLint loc = glGetUniformBlockIndex(spec.id, ubo_pairs[i].name);
			if (loc >= 0) {
				glUniformBlockBinding(spec.id, loc, ubo_pairs[i].index);
			}
		}
	*/
}
void ShaderGLES2::_pre_link_program(Version::Specialization &spec) {
	// bind the attribute locations. This has to be done before linking so that the
	// linker doesn't assign some random indices

	for (int i = 0; i < attribute_pair_count; i++) {
		glBindAttribLocation(spec.id, attribute_pairs[i].index, attribute_pairs[i].name);
	}

	//Transfor feedback is only possible in gles3
	/*
		if (feedback_count) {
			Vector<const char *> feedback;
			for (int i = 0; i < feedback_count; i++) {
				if (feedbacks[i].specialization == 0 || (feedbacks[i].specialization & p_specialization)) {
					// Specialization for this feedback is enabled
					feedback.push_back(feedbacks[i].name);
				}
			}

			if (feedback.size()) {
				glTransformFeedbackVaryings(spec.id, feedback.size(), feedback.ptr(), GL_INTERLEAVED_ATTRIBS);
			}
		}
	*/
}

int ShaderGLES2::_get_glsl_version(bool p_gles_over_gl) {
	if (p_gles_over_gl) {
		return 120;
	} else {
		return 100;
	}
};

void ShaderGLES2::_build_multiview_code(StringBuilder &p_builder, bool p_is_vertex_stage){
	/*
		// Insert multiview extension loading, because it needs to appear before
		// any non-preprocessor code (like the "precision highp..." lines below).
		builder.append("#ifdef USE_MULTIVIEW\n");
		builder.append("#if defined(GL_OVR_multiview2)\n");
		builder.append("#extension GL_OVR_multiview2 : require\n");
		builder.append("#elif defined(GL_OVR_multiview)\n");
		builder.append("#extension GL_OVR_multiview : require\n");
		builder.append("#endif\n");
		if (p_is_vertex_stage) {
			builder.append("layout(num_views=2) in;\n");
		}
		builder.append("#define ViewIndex gl_ViewID_OVR\n");
		builder.append("#define MAX_VIEWS 2\n");
		builder.append("#else\n");
		builder.append("#define ViewIndex uint(0)\n");
		builder.append("#define MAX_VIEWS 1\n");
		builder.append("#endif\n");
	*/
};

void ShaderGLES2::_setup(const char *p_vertex_code, const char *p_fragment_code, const char *p_name, int p_uniform_count, const char **p_uniform_names, int p_attribute_pair_count, const AttributePair *p_attribute_pairs, int p_texture_count, const TexUnitPair *p_tex_units, int p_specialization_count, const Specialization *p_specializations, int p_variant_count, const char **p_variants) {
	attribute_pairs = p_attribute_pairs;
	attribute_pair_count = p_attribute_pair_count;

	ShaderGLES::_setup(p_vertex_code, p_fragment_code, p_name, p_uniform_count, p_uniform_names, p_texture_count, p_tex_units, p_specialization_count, p_specializations, p_variant_count, p_variants);
}
#endif