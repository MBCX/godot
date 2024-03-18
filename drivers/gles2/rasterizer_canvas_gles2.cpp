/**************************************************************************/
/*  rasterizer_canvas_gle2.cpp                                            */
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

#include "rasterizer_canvas_gles2.h"
#ifdef GLES2_ENABLED

#include "core/os/os.h"

#include "core/config/project_settings.h"
#include "servers/rendering/rendering_server_default.h"

#include "drivers/gles_common/storage/material_storage.h"
#include "storage/texture_storage_gles2.h"

#include "../../scene/resources/canvas_item_material.h"
#include "batcher/batcher_enums.h"

//static const GLenum gl_primitive[] = {
//	GL_POINTS,
//	GL_LINES,
//	GL_LINE_STRIP,
//	GL_LINE_LOOP,
//	GL_TRIANGLES,
//	GL_TRIANGLE_STRIP,
//	GL_TRIANGLE_FAN
//};

void RasterizerCanvasGLES2::_set_shadow_filer(RS::CanvasLightShadowFilter p_filter) {
	//disable all
	state.specialization &= ~(CanvasShaderGLES2::SHADOW_FILTER_NEAREST);
	state.specialization &= ~(CanvasShaderGLES2::SHADOW_FILTER_PCF3);
	state.specialization &= ~(CanvasShaderGLES2::SHADOW_FILTER_PCF5);
	state.specialization &= ~(CanvasShaderGLES2::SHADOW_FILTER_PCF7);
	state.specialization &= ~(CanvasShaderGLES2::SHADOW_FILTER_PCF9);
	state.specialization &= ~(CanvasShaderGLES2::SHADOW_FILTER_PCF13);

	switch (p_filter) {
		case RS::CANVAS_LIGHT_FILTER_PCF5:
			state.specialization |= CanvasShaderGLES2::SHADOW_FILTER_PCF5;
			break;
		case RS::CANVAS_LIGHT_FILTER_PCF13:
			state.specialization |= CanvasShaderGLES2::SHADOW_FILTER_PCF13;
			break;
		case RS::CANVAS_LIGHT_FILTER_NONE:
			state.specialization |= CanvasShaderGLES2::SHADOW_FILTER_NEAREST;
		case RS::CANVAS_LIGHT_FILTER_MAX:
			break;
	}
}

void RasterizerCanvasGLES2::_set_texture_rect_mode(bool p_texture_rect, bool p_light_angle, bool p_modulate, bool p_large_vertex) {
	if (p_texture_rect) {
		state.mode_variant = CanvasShaderGLES2::MODE_TEXTURE_RECT;
	} else {
		state.mode_variant = CanvasShaderGLES2::MODE_QUAD;
	}

	if (p_light_angle) {
		state.specialization |= CanvasShaderGLES2::USE_ATTRIB_LIGHT_ANGLE;
	}
	if (p_modulate) {
		state.specialization |= CanvasShaderGLES2::USE_ATTRIB_MODULATE;
	}
	if (p_large_vertex) {
		state.specialization |= CanvasShaderGLES2::USE_ATTRIB_LARGE_VERTEX;
	}
}

void RasterizerCanvasGLES2::_bind_canvas_texture(RID p_texture, RS::CanvasItemTextureFilter p_filter, RS::CanvasItemTextureRepeat p_repeat) {
	if (p_texture.is_null()) {
		p_texture = default_canvas_texture;
	}

	if (state.current_tex == p_texture && state.current_filter_mode == p_filter && state.current_repeat_mode == p_repeat) {
		return;
	}

	GLES::CanvasTexture *ct = nullptr;

	GLES::Texture *t = texture_storage->get_texture(p_texture);

	if (t) {
		if (!t->canvas_texture) {
			// This is based on the GLES3 code, but tbh I find it somewhat icky, since we don't create a RID and I also can't see how or when this is destroyed.
			t->canvas_texture = memnew(GLES::CanvasTexture);
			t->canvas_texture->diffuse = p_texture;
		}

		ct = t->canvas_texture;
		if (t->render_target) {
			t->render_target->used_in_frame = true;
		}
	} else {
		ct = texture_storage->get_canvas_texture(p_texture);
	}

	if (!ct) {
		// Invalid Texture RID.
		_bind_canvas_texture(default_canvas_texture, p_filter, p_repeat);
		return;
	}

	state.current_tex_is_rt = false;

	RS::CanvasItemTextureFilter filter = ct->texture_filter != RS::CANVAS_ITEM_TEXTURE_FILTER_DEFAULT ? ct->texture_filter : p_filter;
	ERR_FAIL_COND(filter == RS::CANVAS_ITEM_TEXTURE_FILTER_DEFAULT);

	RS::CanvasItemTextureRepeat repeat = ct->texture_repeat != RS::CANVAS_ITEM_TEXTURE_REPEAT_DEFAULT ? ct->texture_repeat : p_repeat;
	ERR_FAIL_COND(repeat == RS::CANVAS_ITEM_TEXTURE_REPEAT_DEFAULT);

	GLES::Texture *texture = texture_storage->get_texture(ct->diffuse);
	if (!texture) {
		GLES::Texture *tex = texture_storage->get_texture(texture_storage->texture_gl_get_default(GLES::DEFAULT_GL_TEXTURE_WHITE));
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, tex->tex_id);
		state.texpixel_size = Size2(1.0 / tex->width, 1.0 / tex->height);
		state.texture_size = Size2i(tex->width, tex->height);
	} else {
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, texture->tex_id);

		if (!config->support_npot_repeat_mipmap) {
			// workaround for when setting tiling does not work for non power of two sized textures due to hardware limitation
			if (next_power_of_2(texture->alloc_width) != (unsigned int)texture->alloc_width && next_power_of_2(texture->alloc_height) != (unsigned int)texture->alloc_height) {
				state.specialization |= CanvasShaderGLES2::USE_FORCE_REPEAT;
				p_repeat = RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED;
			} else {
				state.specialization &= ~CanvasShaderGLES2::USE_FORCE_REPEAT;
			}
		}

		texture->gl_set_filter(filter);
		texture->gl_set_repeat(repeat);
		if (texture->render_target) {
			texture->render_target->used_in_frame = true;
			state.current_tex_is_rt = true;
		}
		state.texpixel_size = Size2(1.0 / texture->width, 1.0 / texture->height);
		state.texture_size = Size2i(texture->width, texture->height);
	}

	GLES::Texture *normal_map = texture_storage->get_texture(ct->normal_map);
	if (!normal_map) {
		glActiveTexture(GL_TEXTURE0 + config->max_texture_image_units - 6);
		GLES::Texture *tex = texture_storage->get_texture(texture_storage->texture_gl_get_default(GLES::DEFAULT_GL_TEXTURE_NORMAL));
		glBindTexture(GL_TEXTURE_2D, tex->tex_id);
		state.normal_used = false;
	} else {
		glActiveTexture(GL_TEXTURE0 + config->max_texture_image_units - 6);
		glBindTexture(GL_TEXTURE_2D, normal_map->tex_id);
		normal_map->gl_set_filter(filter);
		normal_map->gl_set_repeat(repeat);
		if (normal_map->render_target) {
			normal_map->render_target->used_in_frame = true;
			state.current_tex_is_rt = true;
		}
		state.normal_used = true;
	}

	GLES::Texture *specular_map = texture_storage->get_texture(ct->specular);
	if (!specular_map) {
		glActiveTexture(GL_TEXTURE0 + config->max_texture_image_units - 7);
		GLES::Texture *tex = texture_storage->get_texture(texture_storage->texture_gl_get_default(GLES::DEFAULT_GL_TEXTURE_WHITE));
		glBindTexture(GL_TEXTURE_2D, tex->tex_id);
	} else {
		glActiveTexture(GL_TEXTURE0 + config->max_texture_image_units - 7);
		glBindTexture(GL_TEXTURE_2D, specular_map->tex_id);
		specular_map->gl_set_filter(filter);
		specular_map->gl_set_repeat(repeat);
		if (specular_map->render_target) {
			specular_map->render_target->used_in_frame = true;
			state.current_tex_is_rt = true;
		}
	}

	state.current_tex = p_texture;
	state.current_filter_mode = filter;
	state.current_repeat_mode = repeat;
}

void RasterizerCanvasGLES2::_set_canvas_uniforms() {
	state.canvas_shader->version_set_uniform(CanvasShaderGLES2::PROJECTION_MATRIX, state.uniforms.projection_matrix, state.shader_version, state.mode_variant, state.specialization);
	state.canvas_shader->version_set_uniform(CanvasShaderGLES2::MODELVIEW_MATRIX, state.uniforms.modelview_matrix, state.shader_version, state.mode_variant, state.specialization);
	state.canvas_shader->version_set_uniform(CanvasShaderGLES2::EXTRA_MATRIX, state.uniforms.extra_matrix, state.shader_version, state.mode_variant, state.specialization);

	state.canvas_shader->version_set_uniform(CanvasShaderGLES2::FINAL_MODULATE, state.uniforms.final_modulate, state.shader_version, state.mode_variant, state.specialization);

	state.canvas_shader->version_set_uniform(CanvasShaderGLES2::TIME, state.uniforms.time, state.shader_version, state.mode_variant, state.specialization);

	if (state.render_target != RID()) {
		Vector2 screen_pixel_size;
		screen_pixel_size.x = 1.0 / texture_storage->get_render_target(state.render_target)->size.width;
		screen_pixel_size.y = 1.0 / texture_storage->get_render_target(state.render_target)->size.height;

		state.canvas_shader->version_set_uniform(CanvasShaderGLES2::SCREEN_PIXEL_SIZE, screen_pixel_size, state.shader_version, state.mode_variant, state.specialization);
	}

	if (state.using_skeleton) {
		state.canvas_shader->version_set_uniform(CanvasShaderGLES2::SKELETON_TRANSFORM, state.skeleton_transform, state.shader_version, state.mode_variant, state.specialization);
		state.canvas_shader->version_set_uniform(CanvasShaderGLES2::SKELETON_TRANSFORM_INVERSE, state.skeleton_transform_inverse, state.shader_version, state.mode_variant, state.specialization);
		state.canvas_shader->version_set_uniform(CanvasShaderGLES2::SKELETON_TEXTURE_SIZE, state.skeleton_texture_size, state.shader_version, state.mode_variant, state.specialization);
	}

	if (state.using_light) {
		Light *light = state.using_light;
		state.canvas_shader->version_set_uniform(CanvasShaderGLES2::LIGHT_MATRIX, light->light_shader_xform, state.shader_version, state.mode_variant, state.specialization);
		Transform2D basis_inverse = light->light_shader_xform.affine_inverse().orthonormalized();
		basis_inverse[2] = Vector2();
		state.canvas_shader->version_set_uniform(CanvasShaderGLES2::LIGHT_MATRIX_INVERSE, basis_inverse, state.shader_version, state.mode_variant, state.specialization);
		state.canvas_shader->version_set_uniform(CanvasShaderGLES2::LIGHT_LOCAL_MATRIX, light->xform_cache.affine_inverse(), state.shader_version, state.mode_variant, state.specialization);
		state.canvas_shader->version_set_uniform(CanvasShaderGLES2::LIGHT_COLOR, light->color * light->energy, state.shader_version, state.mode_variant, state.specialization);
		//		state.canvas_shader->version_set_uniform(CanvasShaderGLES2::LIGHT_POS, light->light_shader_pos, state.shader_version, state.mode_variant, state.specialization);
		// FTODO
		state.canvas_shader->version_set_uniform(CanvasShaderGLES2::LIGHT_POS, light->light_shader_xform[2], state.shader_version, state.mode_variant, state.specialization);
		state.canvas_shader->version_set_uniform(CanvasShaderGLES2::LIGHT_HEIGHT, light->height, state.shader_version, state.mode_variant, state.specialization);

		// FTODO
		//state.canvas_shader->version_set_uniform(CanvasShaderGLES2::LIGHT_OUTSIDE_ALPHA, light->mode == RS::CANVAS_LIGHT_MODE_MASK ? 1.0 : 0.0, state.shader_version, state.mode_variant, state.specialization);
		state.canvas_shader->version_set_uniform(CanvasShaderGLES2::LIGHT_OUTSIDE_ALPHA, 0.0f, state.shader_version, state.mode_variant, state.specialization);

		if (state.using_shadow) {
			// FTODO
#if 0
			RasterizerStorageGLES2::CanvasLightShadow *cls = storage->canvas_light_shadow_owner.get(light->shadow_buffer);
			glActiveTexture(GL_TEXTURE0 + storage->config.max_texture_image_units - 5);
			glBindTexture(GL_TEXTURE_2D, cls->distance);
			state.canvas_shader->version_set_uniform(CanvasShaderGLES2::SHADOW_MATRIX, light->shadow_matrix_cache, state.shader_version, state.mode_variant, state.specialization);
			state.canvas_shader->version_set_uniform(CanvasShaderGLES2::LIGHT_SHADOW_COLOR, light->shadow_color, state.shader_version, state.mode_variant, state.specialization);

			state.canvas_shader->version_set_uniform(CanvasShaderGLES2::SHADOWPIXEL_SIZE, (1.0 / light->shadow_buffer_size) * (1.0 + light->shadow_smooth), state.shader_version, state.mode_variant, state.specialization);
			if (light->radius_cache == 0) {
				state.canvas_shader->version_set_uniform(CanvasShaderGLES2::SHADOW_GRADIENT, 0.0, state.shader_version, state.mode_variant, state.specialization);
			} else {
				state.canvas_shader->version_set_uniform(CanvasShaderGLES2::SHADOW_GRADIENT, light->shadow_gradient_length / (light->radius_cache * 1.1), state.shader_version, state.mode_variant, state.specialization);
			}
			state.canvas_shader->version_set_uniform(CanvasShaderGLES2::SHADOW_DISTANCE_MULT, light->radius_cache * 1.1, state.shader_version, state.mode_variant, state.specialization);
#endif
		}
	}
}

void RasterizerCanvasGLES2::_bind_quad_buffer() {
	//TODO: make sure we create and destroy canvas_quad_vertices
	glBindBuffer(GL_ARRAY_BUFFER, data.canvas_quad_vertices);
	glEnableVertexAttribArray(RS::ARRAY_VERTEX);
	glVertexAttribPointer(RS::ARRAY_VERTEX, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
}

inline void RasterizerCanvasGLES2::_buffer_orphan_and_upload(unsigned int p_buffer_size_bytes, unsigned int p_offset_bytes, unsigned int p_data_size_bytes, const void *p_data, GLenum p_target, GLenum p_usage, bool p_optional_orphan) const {
	// Orphan the buffer to avoid CPU/GPU sync points caused by glBufferSubData
	// Was previously #ifndef GLES_OVER_GL however this causes stalls on desktop mac also (and possibly other)
	if (!p_optional_orphan) {
		glBufferData(p_target, p_buffer_size_bytes, nullptr, p_usage);
#ifdef RASTERIZER_EXTRA_CHECKS
		// fill with garbage off the end of the array
		if (p_buffer_size_bytes) {
			unsigned int start = p_offset_bytes + p_data_size_bytes;
			unsigned int end = start + 1024;
			if (end < p_buffer_size_bytes) {
				uint8_t *garbage = (uint8_t *)alloca(1024);
				for (int n = 0; n < 1024; n++) {
					garbage[n] = Math::random(0, 255);
				}
				glBufferSubData(p_target, start, 1024, garbage);
			}
		}
#endif
	}
	ERR_FAIL_COND((p_offset_bytes + p_data_size_bytes) > p_buffer_size_bytes);
	glBufferSubData(p_target, p_offset_bytes, p_data_size_bytes, p_data);
}

void RasterizerCanvasGLES2::_copy_texscreen(const Rect2 &p_rect) {
	state.canvas_texscreen_used = true;

	//TODO: look into this again, the original implementation supported alpha and a copy color
	//_copy_screen(p_rect);
	texture_storage->render_target_copy_to_back_buffer(state.render_target, p_rect, false);

	// back to canvas, force rebind
	state.canvas_shader->version_bind_shader(state.shader_version, state.mode_variant, state.specialization);
	_bind_canvas_texture(state.current_tex, state.current_filter_mode, state.current_repeat_mode);
	_set_canvas_uniforms();
}

void RasterizerCanvasGLES2::_legacy_draw_primitive(Item::CommandPrimitive *p_pr, GLES::Material *p_material) {
	//	return;

	if (p_pr->point_count != 4)
		return; // not sure if supported

	_set_texture_rect_mode(false);

	if (state.canvas_shader->version_bind_shader(state.shader_version, state.mode_variant, state.specialization)) {
		_set_canvas_uniforms();
		if (p_material) {
			p_material->data->bind_uniforms();
		}
	}

	_bind_canvas_texture(RID(), RS::CANVAS_ITEM_TEXTURE_FILTER_NEAREST, RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED);

	glDisableVertexAttribArray(RS::ARRAY_COLOR);
	glVertexAttrib4fv(RS::ARRAY_COLOR, p_pr->colors[0].components);

	state.canvas_shader->version_set_uniform(CanvasShaderGLES2::MODELVIEW_MATRIX, state.uniforms.modelview_matrix, state.shader_version, state.mode_variant, state.specialization);

	_draw_gui_primitive(p_pr->point_count, p_pr->points, NULL, NULL);
}

void RasterizerCanvasGLES2::_legacy_draw_line(Item::CommandPrimitive *p_pr, GLES::Material *p_material) {
	_set_texture_rect_mode(false);

	if (state.canvas_shader->version_bind_shader(state.shader_version, state.mode_variant, state.specialization)) {
		_set_canvas_uniforms();
		if (p_material) {
			p_material->data->bind_uniforms();
		}
	}

	_bind_canvas_texture(RID(), RS::CANVAS_ITEM_TEXTURE_FILTER_NEAREST, RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED);

	glDisableVertexAttribArray(RS::ARRAY_COLOR);
	glVertexAttrib4fv(RS::ARRAY_COLOR, p_pr->colors[0].components);

	state.canvas_shader->version_set_uniform(CanvasShaderGLES2::MODELVIEW_MATRIX, state.uniforms.modelview_matrix, state.shader_version, state.mode_variant, state.specialization);

#ifdef GLES_OVER_GL
//		if (line->antialiased)
//			glEnable(GL_LINE_SMOOTH);
#endif
	_draw_gui_primitive(2, p_pr->points, NULL, NULL);

#ifdef GLES_OVER_GL
//		if (line->antialiased)
//			glDisable(GL_LINE_SMOOTH);
#endif
}

void RasterizerCanvasGLES2::_legacy_draw_poly_triangles(Item::CommandPolygon *p_poly, GLES::Material *p_material) {
	//	return;

	const PolyData &pd = _polydata[p_poly->polygon.polygon_id];
	//PolygonBuffers *pb = polygon_buffers.polygons.getptr(p_poly->polygon.polygon_id);

	_set_texture_rect_mode(false);

	if (state.canvas_shader->version_bind_shader(state.shader_version, state.mode_variant, state.specialization)) {
		_set_canvas_uniforms();
		if (p_material) {
			p_material->data->bind_uniforms();
		}
	}

	// FTODO
	//RasterizerStorageGLES2::Texture *texture = _bind_canvas_texture(polygon->texture, polygon->normal_map);
	_bind_canvas_texture(p_poly->texture, state.current_filter_mode, state.current_repeat_mode);
	state.canvas_shader->version_set_uniform(CanvasShaderGLES2::COLOR_TEXPIXEL_SIZE, state.texpixel_size, state.shader_version, state.mode_variant, state.specialization);

	_draw_polygon(pd.indices.ptr(), pd.indices.size(), pd.points.size(), pd.points.ptr(), pd.uvs.ptr(), pd.colors.ptr(), pd.colors.size() == 1, nullptr, nullptr);

//	 _draw_polygon(polygon->indices.ptr(), polygon->count, polygon->points.size(), polygon->points.ptr(), polygon->uvs.ptr(), polygon->colors.ptr(), polygon->colors.size() == 1, polygon->weights.ptr(), polygon->bones.ptr());
#ifdef GLES_OVER_GL
#if 0
	if (polygon->antialiased) {
		glEnable(GL_LINE_SMOOTH);
		if (polygon->antialiasing_use_indices) {
			_draw_generic_indices(GL_LINE_STRIP, polygon->indices.ptr(), polygon->count, polygon->points.size(), polygon->points.ptr(), polygon->uvs.ptr(), polygon->colors.ptr(), polygon->colors.size() == 1);
		} else {
			_draw_generic(GL_LINE_LOOP, polygon->points.size(), polygon->points.ptr(), polygon->uvs.ptr(), polygon->colors.ptr(), polygon->colors.size() == 1);
		}
		glDisable(GL_LINE_SMOOTH);
	}
#endif
#endif
}

void RasterizerCanvasGLES2::_draw_gui_primitive(int p_points, const Vector2 *p_vertices, const Color *p_colors, const Vector2 *p_uvs, const float *p_light_angles) {
	static const GLenum prim[5] = { GL_POINTS, GL_POINTS, GL_LINES, GL_TRIANGLES, GL_TRIANGLE_FAN };

	int color_offset = 0;
	int uv_offset = 0;
	int light_angle_offset = 0;
	int stride = 2;

	if (p_colors) {
		color_offset = stride;
		stride += 4;
	}

	if (p_uvs) {
		uv_offset = stride;
		stride += 2;
	}

	if (p_light_angles) { //light_angles
		light_angle_offset = stride;
		stride += 1;
	}

	RAST_DEV_DEBUG_ASSERT(p_points <= 4);
	float buffer_data[(2 + 2 + 4 + 1) * 4];

	for (int i = 0; i < p_points; i++) {
		buffer_data[stride * i + 0] = p_vertices[i].x;
		buffer_data[stride * i + 1] = p_vertices[i].y;
	}

	if (p_colors) {
		for (int i = 0; i < p_points; i++) {
			buffer_data[stride * i + color_offset + 0] = p_colors[i].r;
			buffer_data[stride * i + color_offset + 1] = p_colors[i].g;
			buffer_data[stride * i + color_offset + 2] = p_colors[i].b;
			buffer_data[stride * i + color_offset + 3] = p_colors[i].a;
		}
	}

	if (p_uvs) {
		for (int i = 0; i < p_points; i++) {
			buffer_data[stride * i + uv_offset + 0] = p_uvs[i].x;
			buffer_data[stride * i + uv_offset + 1] = p_uvs[i].y;
		}
	}

	if (p_light_angles) {
		for (int i = 0; i < p_points; i++) {
			buffer_data[stride * i + light_angle_offset + 0] = p_light_angles[i];
		}
	}

	glBindBuffer(GL_ARRAY_BUFFER, data.polygon_buffer);
	_buffer_orphan_and_upload(data.polygon_buffer_size, 0, p_points * stride * 4 * sizeof(float), buffer_data, GL_ARRAY_BUFFER, GL_DYNAMIC_DRAW, true);

	glVertexAttribPointer(RS::ARRAY_VERTEX, 2, GL_FLOAT, GL_FALSE, stride * sizeof(float), NULL);

	if (p_colors) {
		glVertexAttribPointer(RS::ARRAY_COLOR, 4, GL_FLOAT, GL_FALSE, stride * sizeof(float), CAST_INT_TO_UCHAR_PTR(color_offset * sizeof(float)));
		glEnableVertexAttribArray(RS::ARRAY_COLOR);
	}

	if (p_uvs) {
		glVertexAttribPointer(RS::ARRAY_TEX_UV, 2, GL_FLOAT, GL_FALSE, stride * sizeof(float), CAST_INT_TO_UCHAR_PTR(uv_offset * sizeof(float)));
		glEnableVertexAttribArray(RS::ARRAY_TEX_UV);
	}

	if (p_light_angles) {
		glVertexAttribPointer(RS::ARRAY_TANGENT, 1, GL_FLOAT, GL_FALSE, stride * sizeof(float), CAST_INT_TO_UCHAR_PTR(light_angle_offset * sizeof(float)));
		glEnableVertexAttribArray(RS::ARRAY_TANGENT);
	}

	glDrawArrays(prim[p_points], 0, p_points);
	//TODO: add profiling information storage->info.render._2d_draw_call_count++;

	if (p_light_angles) {
		// may not be needed
		glDisableVertexAttribArray(RS::ARRAY_TANGENT);
	}

	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

inline bool _safe_buffer_sub_data(unsigned int p_total_buffer_size, GLenum p_target, unsigned int p_offset, unsigned int p_data_size, const void *p_data, unsigned int &r_offset_after) {
	r_offset_after = p_offset + p_data_size;
#ifdef DEBUG_ENABLED
	// we are trying to write across the edge of the buffer
	if (r_offset_after > p_total_buffer_size) {
		return false;
	}
#endif
	glBufferSubData(p_target, p_offset, p_data_size, p_data);
	return true;
}

void RasterizerCanvasGLES2::_draw_polygon(const int *p_indices, int p_index_count, int p_vertex_count, const Vector2 *p_vertices, const Vector2 *p_uvs, const Color *p_colors, bool p_singlecolor, const float *p_weights, const int *p_bones) {
	glBindBuffer(GL_ARRAY_BUFFER, data.polygon_buffer);

	uint32_t buffer_ofs = 0;
	uint32_t buffer_ofs_after = buffer_ofs + (sizeof(Vector2) * p_vertex_count);
#ifdef DEBUG_ENABLED
	ERR_FAIL_COND(buffer_ofs_after > data.polygon_buffer_size);
#endif

	_buffer_orphan_and_upload(data.polygon_buffer_size, 0, sizeof(Vector2) * p_vertex_count, p_vertices, GL_ARRAY_BUFFER, GL_DYNAMIC_DRAW, true);

	glEnableVertexAttribArray(RS::ARRAY_VERTEX);
	glVertexAttribPointer(RS::ARRAY_VERTEX, 2, GL_FLOAT, GL_FALSE, sizeof(Vector2), NULL);
	buffer_ofs = buffer_ofs_after;

	if (p_singlecolor) {
		glDisableVertexAttribArray(RS::ARRAY_COLOR);
		Color m = *p_colors;
		glVertexAttrib4f(RS::ARRAY_COLOR, m.r, m.g, m.b, m.a);
	} else if (!p_colors) {
		glDisableVertexAttribArray(RS::ARRAY_COLOR);
		glVertexAttrib4f(RS::ARRAY_COLOR, 1, 1, 1, 1);
	} else {
		RAST_FAIL_COND(!_safe_buffer_sub_data(data.polygon_buffer_size, GL_ARRAY_BUFFER, buffer_ofs, sizeof(Color) * p_vertex_count, p_colors, buffer_ofs_after));
		glEnableVertexAttribArray(RS::ARRAY_COLOR);
		glVertexAttribPointer(RS::ARRAY_COLOR, 4, GL_FLOAT, GL_FALSE, sizeof(Color), CAST_INT_TO_UCHAR_PTR(buffer_ofs));
		buffer_ofs = buffer_ofs_after;
	}

	if (p_uvs) {
		RAST_FAIL_COND(!_safe_buffer_sub_data(data.polygon_buffer_size, GL_ARRAY_BUFFER, buffer_ofs, sizeof(Vector2) * p_vertex_count, p_uvs, buffer_ofs_after));
		glEnableVertexAttribArray(RS::ARRAY_TEX_UV);
		glVertexAttribPointer(RS::ARRAY_TEX_UV, 2, GL_FLOAT, GL_FALSE, sizeof(Vector2), CAST_INT_TO_UCHAR_PTR(buffer_ofs));
		buffer_ofs = buffer_ofs_after;
	} else {
		glDisableVertexAttribArray(RS::ARRAY_TEX_UV);
	}

	if (p_weights && p_bones) {
		RAST_FAIL_COND(!_safe_buffer_sub_data(data.polygon_buffer_size, GL_ARRAY_BUFFER, buffer_ofs, sizeof(float) * 4 * p_vertex_count, p_weights, buffer_ofs_after));
		glEnableVertexAttribArray(RS::ARRAY_WEIGHTS);
		glVertexAttribPointer(RS::ARRAY_WEIGHTS, 4, GL_FLOAT, GL_FALSE, sizeof(float) * 4, CAST_INT_TO_UCHAR_PTR(buffer_ofs));
		buffer_ofs = buffer_ofs_after;

		RAST_FAIL_COND(!_safe_buffer_sub_data(data.polygon_buffer_size, GL_ARRAY_BUFFER, buffer_ofs, sizeof(int) * 4 * p_vertex_count, p_bones, buffer_ofs_after));
		glEnableVertexAttribArray(RS::ARRAY_BONES);
		glVertexAttribPointer(RS::ARRAY_BONES, 4, GL_UNSIGNED_INT, GL_FALSE, sizeof(int) * 4, CAST_INT_TO_UCHAR_PTR(buffer_ofs));
		buffer_ofs = buffer_ofs_after;

	} else {
		glDisableVertexAttribArray(RS::ARRAY_WEIGHTS);
		glDisableVertexAttribArray(RS::ARRAY_BONES);
	}

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, data.polygon_index_buffer);

	if (config->support_32_bits_indices) { //should check for
#ifdef DEBUG_ENABLED
		ERR_FAIL_COND((sizeof(int) * p_index_count) > data.polygon_index_buffer_size);
#endif
		_buffer_orphan_and_upload(data.polygon_index_buffer_size, 0, sizeof(int) * p_index_count, p_indices, GL_ELEMENT_ARRAY_BUFFER, GL_DYNAMIC_DRAW, true);

		glDrawElements(GL_TRIANGLES, p_index_count, GL_UNSIGNED_INT, 0);
		//storage->info.render._2d_draw_call_count++;
	} else {
#ifdef DEBUG_ENABLED
		ERR_FAIL_COND((sizeof(uint16_t) * p_index_count) > data.polygon_index_buffer_size);
#endif
		uint16_t *index16 = (uint16_t *)alloca(sizeof(uint16_t) * p_index_count);
		for (int i = 0; i < p_index_count; i++) {
			index16[i] = uint16_t(p_indices[i]);
		}
		_buffer_orphan_and_upload(data.polygon_index_buffer_size, 0, sizeof(uint16_t) * p_index_count, index16, GL_ELEMENT_ARRAY_BUFFER, GL_DYNAMIC_DRAW, true);
		glDrawElements(GL_TRIANGLES, p_index_count, GL_UNSIGNED_SHORT, 0);
		//storage->info.render._2d_draw_call_count++;
	}

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void RasterizerCanvasGLES2::_batch_upload_buffers() {
	// noop?
	if (!bdata.vertices.size())
		return;

	glBindBuffer(GL_ARRAY_BUFFER, bdata.gl_vertex_buffer);

	// usage flag is a project setting
	GLenum buffer_usage_flag = GL_DYNAMIC_DRAW;
	if (bdata.buffer_mode_batch_upload_flag_stream) {
		buffer_usage_flag = GL_STREAM_DRAW;
	}

	// orphan the old (for now)
	if (bdata.buffer_mode_batch_upload_send_null) {
		glBufferData(GL_ARRAY_BUFFER, 0, 0, buffer_usage_flag); // GL_DYNAMIC_DRAW);
	}

	switch (bdata.fvf) {
		case BatcherEnums::FVF_UNBATCHED: // should not happen
			break;
		case BatcherEnums::FVF_REGULAR: // no change
			glBufferData(GL_ARRAY_BUFFER, sizeof(BatchVertex) * bdata.vertices.size(), bdata.vertices.get_data(), buffer_usage_flag);
			break;
		case BatcherEnums::FVF_COLOR:
			glBufferData(GL_ARRAY_BUFFER, sizeof(BatchVertexColored) * bdata.unit_vertices.size(), bdata.unit_vertices.get_unit(0), buffer_usage_flag);
			break;
		case BatcherEnums::FVF_LIGHT_ANGLE:
			glBufferData(GL_ARRAY_BUFFER, sizeof(BatchVertexLightAngled) * bdata.unit_vertices.size(), bdata.unit_vertices.get_unit(0), buffer_usage_flag);
			break;
		case BatcherEnums::FVF_MODULATED:
			glBufferData(GL_ARRAY_BUFFER, sizeof(BatchVertexModulated) * bdata.unit_vertices.size(), bdata.unit_vertices.get_unit(0), buffer_usage_flag);
			break;
		case BatcherEnums::FVF_LARGE:
			glBufferData(GL_ARRAY_BUFFER, sizeof(BatchVertexLarge) * bdata.unit_vertices.size(), bdata.unit_vertices.get_unit(0), buffer_usage_flag);
			break;
	}

	// might not be necessary
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}
#if 0

void RasterizerCanvasGLES2::_batch_render_lines(const Batch &p_batch, RasterizerStorageGLES2::Material *p_material, bool p_anti_alias) {
	_set_texture_rect_mode(false);

	if (state.canvas_shader->bind()) {
		_set_canvas_uniforms();
		state.canvas_shader->use_material((void *)p_material);
	}

	_bind_canvas_texture(RID(), RID());

	glDisableVertexAttribArray(RS::ARRAY_COLOR);
	glVertexAttrib4fv(RS::ARRAY_COLOR, (float *)&p_batch.color);

#ifdef GLES_OVER_GL
	if (p_anti_alias)
		glEnable(GL_LINE_SMOOTH);
#endif

	int sizeof_vert = sizeof(BatchVertex);

	// bind the index and vertex buffer
	glBindBuffer(GL_ARRAY_BUFFER, bdata.gl_vertex_buffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bdata.gl_index_buffer);

	uint64_t pointer = 0;
	glVertexAttribPointer(RS::ARRAY_VERTEX, 2, GL_FLOAT, GL_FALSE, sizeof_vert, (const void *)pointer);

	glDisableVertexAttribArray(RS::ARRAY_TEX_UV);

	int64_t offset = p_batch.first_vert; // 6 inds per quad at 2 bytes each

	int num_elements = p_batch.num_commands * 2;
	glDrawArrays(GL_LINES, offset, num_elements);

	//TODO: add profiling info storage->info.render._2d_draw_call_count++;

	// may not be necessary .. state change optimization still TODO
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

#ifdef GLES_OVER_GL
	if (p_anti_alias)
		glDisable(GL_LINE_SMOOTH);
#endif
}

void RasterizerCanvasGLES2::_batch_render_generic(const Batch &p_batch, RasterizerStorageGLES2::Material *p_material) {
	ERR_FAIL_COND(p_batch.num_commands <= 0);

	const bool &use_light_angles = bdata.use_light_angles;
	const bool &use_modulate = bdata.use_modulate;
	const bool &use_large_verts = bdata.use_large_verts;
	const bool &colored_verts = bdata.use_colored_vertices | use_light_angles | use_modulate | use_large_verts;

	int sizeof_vert;

	switch (bdata.fvf) {
		default:
			sizeof_vert = 0; // prevent compiler warning - this should never happen
			break;
		case BatcherEnums::FVF_UNBATCHED: {
			sizeof_vert = 0; // prevent compiler warning - this should never happen
			return;
		} break;
		case BatcherEnums::FVF_REGULAR: // no change
			sizeof_vert = sizeof(BatchVertex);
			break;
		case BatcherEnums::FVF_COLOR:
			sizeof_vert = sizeof(BatchVertexColored);
			break;
		case BatcherEnums::FVF_LIGHT_ANGLE:
			sizeof_vert = sizeof(BatchVertexLightAngled);
			break;
		case BatcherEnums::FVF_MODULATED:
			sizeof_vert = sizeof(BatchVertexModulated);
			break;
		case BatcherEnums::FVF_LARGE:
			sizeof_vert = sizeof(BatchVertexLarge);
			break;
	}

	// make sure to set all conditionals BEFORE binding the shader
	_set_texture_rect_mode(false, use_light_angles, use_modulate, use_large_verts);

	// batch tex
	const BatchTex &tex = bdata.batch_textures[p_batch.batch_texture_id];
	//VSG::rasterizer->gl_check_for_error();

	// force repeat is set if non power of 2 texture, and repeat is needed if hardware doesn't support npot
	if (tex.tile_mode == BatchTex::TILE_FORCE_REPEAT) {
		state.specialization |= CanvasShaderGLES2::USE_FORCE_REPEAT;
	}
	//TODO: check if we need to unset specialization after this batch

	if (state.canvas_shader->bind()) {
		_set_canvas_uniforms();
		state.canvas_shader->use_material((void *)p_material);
	}

	_bind_canvas_texture(tex.RID_texture, tex.RID_normal);

	// bind the index and vertex buffer
	glBindBuffer(GL_ARRAY_BUFFER, bdata.gl_vertex_buffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bdata.gl_index_buffer);

	uint64_t pointer = 0;
	glVertexAttribPointer(RS::ARRAY_VERTEX, 2, GL_FLOAT, GL_FALSE, sizeof_vert, (const void *)pointer);

	// always send UVs, even within a texture specified because a shader can still use UVs
	glEnableVertexAttribArray(RS::ARRAY_TEX_UV);
	glVertexAttribPointer(RS::ARRAY_TEX_UV, 2, GL_FLOAT, GL_FALSE, sizeof_vert, CAST_INT_TO_UCHAR_PTR(pointer + (2 * 4)));

	// color
	if (!colored_verts) {
		glDisableVertexAttribArray(RS::ARRAY_COLOR);
		glVertexAttrib4fv(RS::ARRAY_COLOR, p_batch.color.get_data());
	} else {
		glEnableVertexAttribArray(RS::ARRAY_COLOR);
		glVertexAttribPointer(RS::ARRAY_COLOR, 4, GL_FLOAT, GL_FALSE, sizeof_vert, CAST_INT_TO_UCHAR_PTR(pointer + (4 * 4)));
	}

	if (use_light_angles) {
		glEnableVertexAttribArray(RS::ARRAY_TANGENT);
		glVertexAttribPointer(RS::ARRAY_TANGENT, 1, GL_FLOAT, GL_FALSE, sizeof_vert, CAST_INT_TO_UCHAR_PTR(pointer + (8 * 4)));
	}

	if (use_modulate) {
		glEnableVertexAttribArray(RS::ARRAY_TEX_UV2);
		glVertexAttribPointer(RS::ARRAY_TEX_UV2, 4, GL_FLOAT, GL_FALSE, sizeof_vert, CAST_INT_TO_UCHAR_PTR(pointer + (9 * 4)));
	}

	if (use_large_verts) {
		glEnableVertexAttribArray(RS::ARRAY_BONES);
		glVertexAttribPointer(RS::ARRAY_BONES, 2, GL_FLOAT, GL_FALSE, sizeof_vert, CAST_INT_TO_UCHAR_PTR(pointer + (13 * 4)));
		glEnableVertexAttribArray(RS::ARRAY_WEIGHTS);
		glVertexAttribPointer(RS::ARRAY_WEIGHTS, 4, GL_FLOAT, GL_FALSE, sizeof_vert, CAST_INT_TO_UCHAR_PTR(pointer + (15 * 4)));
	}

	// We only want to set the GL wrapping mode if the texture is not already tiled (i.e. set in Import).
	// This  is an optimization left over from the legacy renderer.
	// If we DID set tiling in the API, and reverted to clamped, then the next draw using this texture
	// may use clamped mode incorrectly.
	bool tex_is_already_tiled = tex.flags & RasterizerStorageGLES2::TEXTURE_FLAG_REPEAT;

	if (tex.tile_mode == BatchTex::TILE_NORMAL) {
		// if the texture is imported as tiled, no need to set GL state, as it will already be bound with repeat
		if (!tex_is_already_tiled) {
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		}
	}

	// we need to convert explicitly from pod Vec2 to Vector2 ...
	// could use a cast but this might be unsafe in future
	Vector2 tps;
	tex.tex_pixel_size.to(tps);
	state.canvas_shader->set_uniform(CanvasShaderGLES2::COLOR_TEXPIXEL_SIZE, tps);

	switch (p_batch.type) {
		default: {
			// prevent compiler warning
		} break;
		case BatcherEnums::BT_RECT: {
			int64_t offset = p_batch.first_vert * 3;

			int num_elements = p_batch.num_commands * 6;
			glDrawElements(GL_TRIANGLES, num_elements, GL_UNSIGNED_SHORT, (void *)offset);
		} break;
		case BatcherEnums::BT_POLY: {
			int64_t offset = p_batch.first_vert;

			int num_elements = p_batch.num_commands;
			glDrawArrays(GL_TRIANGLES, offset, num_elements);
		} break;
	}

	//TODO: add profiling info storage->info.render._2d_draw_call_count++;

	switch (tex.tile_mode) {
		case BatchTex::TILE_FORCE_REPEAT: {
			//state.canvas_shader->set_conditional(CanvasShaderGLES2::USE_FORCE_REPEAT, false);
			state.specialization &= ~(CanvasShaderGLES2::USE_FORCE_REPEAT)
		} break;
		case BatchTex::TILE_NORMAL: {
			// if the texture is imported as tiled, no need to revert GL state
			if (!tex_is_already_tiled) {
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			}
		} break;
		default: {
		} break;
	}

	// could these have ifs?
	glDisableVertexAttribArray(RS::ARRAY_TEX_UV);
	glDisableVertexAttribArray(RS::ARRAY_COLOR);
	glDisableVertexAttribArray(RS::ARRAY_TANGENT);
	glDisableVertexAttribArray(RS::ARRAY_TEX_UV2);
	glDisableVertexAttribArray(RS::ARRAY_BONES);
	glDisableVertexAttribArray(RS::ARRAY_WEIGHTS);

	// may not be necessary .. state change optimization still TODO
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}
#endif
void RasterizerCanvasGLES2::render_batches(Item::Command *const *p_commands, Item *p_current_clip, bool &r_reclip, GLES::Material *p_material) {
	int num_batches = bdata.batches.size();

	for (int batch_num = 0; batch_num < num_batches; batch_num++) {
		const Batch &batch = bdata.batches[batch_num];

		GLES::CanvasMaterialData *material_data = nullptr;
		if (p_material) {
			material_data = static_cast<GLES::CanvasMaterialData *>(p_material->data);
		}
		//TODO: reset the current specialisation to default
		state.specialization = 0;
		state.mode_variant = CanvasShaderGLES2::ShaderVariant::MODE_QUAD;
		state.shader_version = data.canvas_shader_default_version;
		if (material_data) {
			if (material_data->shader_data->version.is_valid() && material_data->shader_data->valid) {
				state.shader_version = material_data->shader_data->version;
			}
		}

		switch (batch.type) {
			case BatcherEnums::BT_RECT: {
				//_batch_render_generic(batch, p_material);
			} break;
			case BatcherEnums::BT_POLY: {
				//_batch_render_generic(batch, p_material);
			} break;
			case BatcherEnums::BT_LINE: {
				//_batch_render_lines(batch, p_material, false);
			} break;
			case BatcherEnums::BT_LINE_AA: {
				//_batch_render_lines(batch, p_material, true);
			} break;
			default: {
				//int end_command = batch.first_command + batch.num_commands;

				Item::Command *command = batch.first_command;
				for (uint32_t i = 0; i < batch.num_commands; i++, command = command->next) {
					switch (command->type) {
#if 0
						case Item::Command::TYPE_LINE: {
							Item::CommandLine *line = static_cast<Item::CommandLine *>(command);

							_set_texture_rect_mode(false);

							if (state.canvas_shader->bind()) {
								_set_canvas_uniforms();
								state.canvas_shader->use_material((void *)p_material);
							}

							_bind_canvas_texture(RID(), RID());

							glDisableVertexAttribArray(RS::ARRAY_COLOR);
							glVertexAttrib4fv(RS::ARRAY_COLOR, line->color.components);

							state.canvas_shader->set_uniform(CanvasShaderGLES2::MODELVIEW_MATRIX, state.uniforms.modelview_matrix);

							if (line->width <= 1) {
								Vector2 verts[2] = {
									Vector2(line->from.x, line->from.y),
									Vector2(line->to.x, line->to.y)
								};

#ifdef GLES_OVER_GL
								if (line->antialiased)
									glEnable(GL_LINE_SMOOTH);
#endif
								_draw_gui_primitive(2, verts, NULL, NULL);

#ifdef GLES_OVER_GL
								if (line->antialiased)
									glDisable(GL_LINE_SMOOTH);
#endif
							} else {
								Vector2 t = (line->from - line->to).normalized().tangent() * line->width * 0.5;

								Vector2 verts[4] = {
									line->from - t,
									line->from + t,
									line->to + t,
									line->to - t
								};

								_draw_gui_primitive(4, verts, NULL, NULL);
#ifdef GLES_OVER_GL
								if (line->antialiased) {
									glEnable(GL_LINE_SMOOTH);
									for (int j = 0; j < 4; j++) {
										Vector2 vertsl[2] = {
											verts[j],
											verts[(j + 1) % 4],
										};
										_draw_gui_primitive(2, vertsl, NULL, NULL);
									}
									glDisable(GL_LINE_SMOOTH);
								}
#endif
							}
						} break;
#endif
						case Item::Command::TYPE_PRIMITIVE: {
							Item::CommandPrimitive *pr = static_cast<Item::CommandPrimitive *>(command);

							switch (pr->point_count) {
								case 2: {
									_legacy_draw_line(pr, p_material);
								} break;
								default: {
									_legacy_draw_primitive(pr, p_material);
								} break;
							}

						} break;

						case Item::Command::TYPE_RECT: {
							Item::CommandRect *r = static_cast<Item::CommandRect *>(command);

							glDisableVertexAttribArray(RS::ARRAY_COLOR);
							glVertexAttrib4fv(RS::ARRAY_COLOR, r->modulate.components);

							// Bind textures early so we can access state.current_tex_is_rt in time to invert the UVs
							RS::CanvasItemTextureRepeat repeat = state.default_repeat;
							if (p_current_clip && p_current_clip->texture_repeat != RS::CANVAS_ITEM_TEXTURE_REPEAT_DEFAULT) {
								repeat = p_current_clip->texture_repeat;
							}
							if (r->flags & CANVAS_RECT_TILE) {
								repeat = RS::CANVAS_ITEM_TEXTURE_REPEAT_ENABLED;
							}
							RS::CanvasItemTextureFilter filter = state.default_filter;
							if (p_current_clip && p_current_clip->texture_filter != RS::CANVAS_ITEM_TEXTURE_FILTER_DEFAULT) {
								filter = p_current_clip->texture_filter;
							}
							_bind_canvas_texture(r->texture, filter, repeat);

							// we will take account of render target textures which need to be drawn upside down
							// quirk of opengl
							bool upside_down = r->flags & CANVAS_RECT_FLIP_V;
							if (state.current_tex_is_rt) {
								upside_down = true;
							}

							// On some widespread Nvidia cards, the normal draw method can produce some
							// flickering in draw_rect and especially TileMap rendering (tiles randomly flicker).
							// See GH-9913.
							// To work it around, we use a simpler draw method which does not flicker, but gives
							// a non negligible performance hit, so it's opt-in (GH-24466).
							if (use_nvidia_rect_workaround) {
								// are we using normal maps, if so we want to use light angle
								bool send_light_angles = false;

								// TODO
								// only need to use light angles when normal mapping
								// otherwise we can use the default shader
								if (state.normal_used) {
									send_light_angles = true;
								}

								_set_texture_rect_mode(false, send_light_angles);

								if (state.canvas_shader->version_bind_shader(state.shader_version, state.mode_variant, state.specialization)) {
									_set_canvas_uniforms();
									if (p_material) {
										p_material->data->bind_uniforms();
									}
								}

								Vector2 points[4] = {
									r->rect.position,
									r->rect.position + Vector2(r->rect.size.x, 0.0),
									r->rect.position + r->rect.size,
									r->rect.position + Vector2(0.0, r->rect.size.y),
								};

								if (r->rect.size.x < 0) {
									SWAP(points[0], points[1]);
									SWAP(points[2], points[3]);
								}
								if (r->rect.size.y < 0) {
									SWAP(points[0], points[3]);
									SWAP(points[1], points[2]);
								}

								/* We do this before the if nvidia_workaround
								RS::CanvasItemTextureRepeat repeat = state.default_repeat;
								if (p_current_clip->texture_repeat != RS::CANVAS_ITEM_TEXTURE_REPEAT_DEFAULT) {
									repeat = p_current_clip->texture_repeat;
								}
								if (r->flags & CANVAS_RECT_TILE) {
									repeat = RS::CANVAS_ITEM_TEXTURE_REPEAT_ENABLED;
								}
															RS::CanvasItemTextureFilter filter = state.default_filter;;
								if (p_current_clip->texture_filter != RS::CANVAS_ITEM_TEXTURE_FILTER_DEFAULT) {
									filter = p_current_clip->texture_filter;
								}
								_bind_canvas_texture(r->texture, filter, repeat);

								*/

								Rect2 src_rect = (r->flags & CANVAS_RECT_REGION) ? Rect2(r->source.position * state.texpixel_size, r->source.size * state.texpixel_size) : Rect2(0, 0, 1, 1);

								Vector2 uvs[4] = {
									src_rect.position,
									src_rect.position + Vector2(src_rect.size.x, 0.0),
									src_rect.position + src_rect.size,
									src_rect.position + Vector2(0.0, src_rect.size.y),
								};

								// for encoding in light angle
								bool flip_h = false;
								bool flip_v = false;

								if (r->flags & CANVAS_RECT_TRANSPOSE) {
									SWAP(uvs[1], uvs[3]);
								}

								if (r->flags & CANVAS_RECT_FLIP_H) {
									SWAP(uvs[0], uvs[1]);
									SWAP(uvs[2], uvs[3]);
									flip_h = true;
									flip_v = !flip_v;
								}
								if (upside_down) {
									SWAP(uvs[0], uvs[3]);
									SWAP(uvs[1], uvs[2]);
									flip_v = !flip_v;
								}

								state.canvas_shader->version_set_uniform(CanvasShaderGLES2::COLOR_TEXPIXEL_SIZE, state.texpixel_size, state.shader_version, state.mode_variant, state.specialization);

								if (send_light_angles) {
									// for single rects, there is no need to fully utilize the light angle,
									// we only need it to encode flips (horz and vert). But the shader can be reused with
									// batching in which case the angle encodes the transform as well as
									// the flips.
									// Note transpose is NYI. I don't think it worked either with the non-nvidia method.

									// if horizontal flip, angle is 180
									float angle = 0.0f;
									if (flip_h)
										angle = Math_PI;

									// add 1 (to take care of zero floating point error with sign)
									angle += 1.0f;

									// flip if necessary
									if (flip_v)
										angle *= -1.0f;

									// light angle must be sent for each vert, instead as a single uniform in the uniform draw method
									// this has the benefit of enabling batching with light angles.
									float light_angles[4] = { angle, angle, angle, angle };

									_draw_gui_primitive(4, points, NULL, uvs, light_angles);
								} else {
									_draw_gui_primitive(4, points, NULL, uvs);
								}

							} else {
								// This branch is better for performance, but can produce flicker on Nvidia, see above comment.

								_set_texture_rect_mode(true);

								if (state.canvas_shader->version_bind_shader(state.shader_version, state.mode_variant, state.specialization)) {
									_set_canvas_uniforms();
									if (p_material) {
										p_material->data->bind_uniforms();
									}
								}

								_bind_quad_buffer();

								/* We do this before the if nvidia_workaround
									RS::CanvasItemTextureRepeat repeat = state.default_repeat;
									if (p_current_clip->texture_repeat != RS::CANVAS_ITEM_TEXTURE_REPEAT_DEFAULT) {
										repeat = p_current_clip->texture_repeat;
									}
									if (r->flags & CANVAS_RECT_TILE) {
										repeat = RS::CANVAS_ITEM_TEXTURE_REPEAT_ENABLED;
									}
									//TODO: instead of just setting this to linear check what the default is
									//TODO: do the same for repeat
																RS::CanvasItemTextureFilter filter = state.default_filter;;
									if (p_current_clip->texture_filter != RS::CANVAS_ITEM_TEXTURE_FILTER_DEFAULT) {
										filter = p_current_clip->texture_filter;
									}

									_bind_canvas_texture(r->texture, filter, repeat);
								*/

								Rect2 src_rect = (r->flags & CANVAS_RECT_REGION) ? Rect2(r->source.position * state.texpixel_size, r->source.size * state.texpixel_size) : Rect2(0, 0, 1, 1);

								Rect2 dst_rect = Rect2(r->rect.position, r->rect.size);

								if (dst_rect.size.width < 0) {
									dst_rect.position.x += dst_rect.size.width;
									dst_rect.size.width *= -1;
								}
								if (dst_rect.size.height < 0) {
									dst_rect.position.y += dst_rect.size.height;
									dst_rect.size.height *= -1;
								}

								if (r->flags & CANVAS_RECT_FLIP_H) {
									src_rect.size.x *= -1;
								}

								if (upside_down) {
									src_rect.size.y *= -1;
								}

								if (r->flags & CANVAS_RECT_TRANSPOSE) {
									dst_rect.size.x *= -1; // Encoding in the dst_rect.z uniform
								}

								state.canvas_shader->version_set_uniform(CanvasShaderGLES2::COLOR_TEXPIXEL_SIZE, state.texpixel_size, state.shader_version, state.mode_variant, state.specialization);

								state.canvas_shader->version_set_uniform(CanvasShaderGLES2::DST_RECT, Color(dst_rect.position.x, dst_rect.position.y, dst_rect.size.x, dst_rect.size.y), state.shader_version, state.mode_variant, state.specialization);
								state.canvas_shader->version_set_uniform(CanvasShaderGLES2::SRC_RECT, Color(src_rect.position.x, src_rect.position.y, src_rect.size.x, src_rect.size.y), state.shader_version, state.mode_variant, state.specialization);

								glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
								//TODO: add profiling info storage->info.render._2d_draw_call_count++;

								glBindBuffer(GL_ARRAY_BUFFER, 0);
								glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
							}

							//state.canvas_shader->set_conditional(CanvasShaderGLES2::USE_FORCE_REPEAT, false);
							state.specialization &= ~(CanvasShaderGLES2::USE_FORCE_REPEAT);

						} break;
						case Item::Command::TYPE_NINEPATCH: {
							Item::CommandNinePatch *np = static_cast<Item::CommandNinePatch *>(command);

							_set_texture_rect_mode(false);
							if (state.canvas_shader->version_bind_shader(state.shader_version, state.mode_variant, state.specialization)) {
								_set_canvas_uniforms();
								if (p_material) {
									p_material->data->bind_uniforms();
								}
							}

							glDisableVertexAttribArray(RS::ARRAY_COLOR);
							glVertexAttrib4fv(RS::ARRAY_COLOR, np->color.components);

							RS::CanvasItemTextureRepeat repeat = state.default_repeat;
							if (p_current_clip->texture_repeat != RS::CANVAS_ITEM_TEXTURE_REPEAT_DEFAULT) {
								repeat = p_current_clip->texture_repeat;
							}
							RS::CanvasItemTextureFilter filter = state.default_filter;
							;
							if (p_current_clip->texture_filter != RS::CANVAS_ITEM_TEXTURE_FILTER_DEFAULT) {
								filter = p_current_clip->texture_filter;
							}

							_bind_canvas_texture(np->texture, filter, repeat);

							if (state.texpixel_size == Size2(0.0, 0.0)) {
								WARN_PRINT("Cannot set empty texture to NinePatch.");
								continue;
							}

							// state.canvas_shader->set_uniform(CanvasShaderGLES2::MODELVIEW_MATRIX, state.uniforms.modelview_matrix);
							state.canvas_shader->version_set_uniform(CanvasShaderGLES2::COLOR_TEXPIXEL_SIZE, state.texpixel_size, state.shader_version, state.mode_variant, state.specialization);

							Rect2 source = np->source;
							if (source.size.x == 0 && source.size.y == 0) {
								source.size.x = state.texture_size.x;
								source.size.y = state.texture_size.y;
							}

							float screen_scale = 1.0;

							if ((bdata.settings_ninepatch_mode == 1) && (source.size.x != 0) && (source.size.y != 0)) {
								screen_scale = MIN(np->rect.size.x / source.size.x, np->rect.size.y / source.size.y);
								screen_scale = MIN(1.0, screen_scale);
							}

							// prepare vertex buffer

							// this buffer contains [ POS POS UV UV ] *

							float buffer[16 * 2 + 16 * 2];

							{
								// first row

								buffer[(0 * 4 * 4) + 0] = np->rect.position.x;
								buffer[(0 * 4 * 4) + 1] = np->rect.position.y;

								buffer[(0 * 4 * 4) + 2] = source.position.x * state.texpixel_size.x;
								buffer[(0 * 4 * 4) + 3] = source.position.y * state.texpixel_size.y;

								buffer[(0 * 4 * 4) + 4] = np->rect.position.x + np->margin[SIDE_LEFT] * screen_scale;
								buffer[(0 * 4 * 4) + 5] = np->rect.position.y;

								buffer[(0 * 4 * 4) + 6] = (source.position.x + np->margin[SIDE_LEFT]) * state.texpixel_size.x;
								buffer[(0 * 4 * 4) + 7] = source.position.y * state.texpixel_size.y;

								buffer[(0 * 4 * 4) + 8] = np->rect.position.x + np->rect.size.x - np->margin[SIDE_RIGHT] * screen_scale;
								buffer[(0 * 4 * 4) + 9] = np->rect.position.y;

								buffer[(0 * 4 * 4) + 10] = (source.position.x + source.size.x - np->margin[SIDE_RIGHT]) * state.texpixel_size.x;
								buffer[(0 * 4 * 4) + 11] = source.position.y * state.texpixel_size.y;

								buffer[(0 * 4 * 4) + 12] = np->rect.position.x + np->rect.size.x;
								buffer[(0 * 4 * 4) + 13] = np->rect.position.y;

								buffer[(0 * 4 * 4) + 14] = (source.position.x + source.size.x) * state.texpixel_size.x;
								buffer[(0 * 4 * 4) + 15] = source.position.y * state.texpixel_size.y;

								// second row

								buffer[(1 * 4 * 4) + 0] = np->rect.position.x;
								buffer[(1 * 4 * 4) + 1] = np->rect.position.y + np->margin[SIDE_TOP] * screen_scale;

								buffer[(1 * 4 * 4) + 2] = source.position.x * state.texpixel_size.x;
								buffer[(1 * 4 * 4) + 3] = (source.position.y + np->margin[SIDE_TOP]) * state.texpixel_size.y;

								buffer[(1 * 4 * 4) + 4] = np->rect.position.x + np->margin[SIDE_LEFT] * screen_scale;
								buffer[(1 * 4 * 4) + 5] = np->rect.position.y + np->margin[SIDE_TOP] * screen_scale;

								buffer[(1 * 4 * 4) + 6] = (source.position.x + np->margin[SIDE_LEFT]) * state.texpixel_size.x;
								buffer[(1 * 4 * 4) + 7] = (source.position.y + np->margin[SIDE_TOP]) * state.texpixel_size.y;

								buffer[(1 * 4 * 4) + 8] = np->rect.position.x + np->rect.size.x - np->margin[SIDE_RIGHT] * screen_scale;
								buffer[(1 * 4 * 4) + 9] = np->rect.position.y + np->margin[SIDE_TOP] * screen_scale;

								buffer[(1 * 4 * 4) + 10] = (source.position.x + source.size.x - np->margin[SIDE_RIGHT]) * state.texpixel_size.x;
								buffer[(1 * 4 * 4) + 11] = (source.position.y + np->margin[SIDE_TOP]) * state.texpixel_size.y;

								buffer[(1 * 4 * 4) + 12] = np->rect.position.x + np->rect.size.x;
								buffer[(1 * 4 * 4) + 13] = np->rect.position.y + np->margin[SIDE_TOP] * screen_scale;

								buffer[(1 * 4 * 4) + 14] = (source.position.x + source.size.x) * state.texpixel_size.x;
								buffer[(1 * 4 * 4) + 15] = (source.position.y + np->margin[SIDE_TOP]) * state.texpixel_size.y;

								// third row

								buffer[(2 * 4 * 4) + 0] = np->rect.position.x;
								buffer[(2 * 4 * 4) + 1] = np->rect.position.y + np->rect.size.y - np->margin[SIDE_BOTTOM] * screen_scale;

								buffer[(2 * 4 * 4) + 2] = source.position.x * state.texpixel_size.x;
								buffer[(2 * 4 * 4) + 3] = (source.position.y + source.size.y - np->margin[SIDE_BOTTOM]) * state.texpixel_size.y;

								buffer[(2 * 4 * 4) + 4] = np->rect.position.x + np->margin[SIDE_LEFT] * screen_scale;
								buffer[(2 * 4 * 4) + 5] = np->rect.position.y + np->rect.size.y - np->margin[SIDE_BOTTOM] * screen_scale;

								buffer[(2 * 4 * 4) + 6] = (source.position.x + np->margin[SIDE_LEFT]) * state.texpixel_size.x;
								buffer[(2 * 4 * 4) + 7] = (source.position.y + source.size.y - np->margin[SIDE_BOTTOM]) * state.texpixel_size.y;

								buffer[(2 * 4 * 4) + 8] = np->rect.position.x + np->rect.size.x - np->margin[SIDE_RIGHT] * screen_scale;
								buffer[(2 * 4 * 4) + 9] = np->rect.position.y + np->rect.size.y - np->margin[SIDE_BOTTOM] * screen_scale;

								buffer[(2 * 4 * 4) + 10] = (source.position.x + source.size.x - np->margin[SIDE_RIGHT]) * state.texpixel_size.x;
								buffer[(2 * 4 * 4) + 11] = (source.position.y + source.size.y - np->margin[SIDE_BOTTOM]) * state.texpixel_size.y;

								buffer[(2 * 4 * 4) + 12] = np->rect.position.x + np->rect.size.x;
								buffer[(2 * 4 * 4) + 13] = np->rect.position.y + np->rect.size.y - np->margin[SIDE_BOTTOM] * screen_scale;

								buffer[(2 * 4 * 4) + 14] = (source.position.x + source.size.x) * state.texpixel_size.x;
								buffer[(2 * 4 * 4) + 15] = (source.position.y + source.size.y - np->margin[SIDE_BOTTOM]) * state.texpixel_size.y;

								// fourth row

								buffer[(3 * 4 * 4) + 0] = np->rect.position.x;
								buffer[(3 * 4 * 4) + 1] = np->rect.position.y + np->rect.size.y;

								buffer[(3 * 4 * 4) + 2] = source.position.x * state.texpixel_size.x;
								buffer[(3 * 4 * 4) + 3] = (source.position.y + source.size.y) * state.texpixel_size.y;

								buffer[(3 * 4 * 4) + 4] = np->rect.position.x + np->margin[SIDE_LEFT] * screen_scale;
								buffer[(3 * 4 * 4) + 5] = np->rect.position.y + np->rect.size.y;

								buffer[(3 * 4 * 4) + 6] = (source.position.x + np->margin[SIDE_LEFT]) * state.texpixel_size.x;
								buffer[(3 * 4 * 4) + 7] = (source.position.y + source.size.y) * state.texpixel_size.y;

								buffer[(3 * 4 * 4) + 8] = np->rect.position.x + np->rect.size.x - np->margin[SIDE_RIGHT] * screen_scale;
								buffer[(3 * 4 * 4) + 9] = np->rect.position.y + np->rect.size.y;

								buffer[(3 * 4 * 4) + 10] = (source.position.x + source.size.x - np->margin[SIDE_RIGHT]) * state.texpixel_size.x;
								buffer[(3 * 4 * 4) + 11] = (source.position.y + source.size.y) * state.texpixel_size.y;

								buffer[(3 * 4 * 4) + 12] = np->rect.position.x + np->rect.size.x;
								buffer[(3 * 4 * 4) + 13] = np->rect.position.y + np->rect.size.y;

								buffer[(3 * 4 * 4) + 14] = (source.position.x + source.size.x) * state.texpixel_size.x;
								buffer[(3 * 4 * 4) + 15] = (source.position.y + source.size.y) * state.texpixel_size.y;
							}

							glBindBuffer(GL_ARRAY_BUFFER, data.ninepatch_vertices);
							glBufferData(GL_ARRAY_BUFFER, sizeof(float) * (16 + 16) * 2, buffer, GL_DYNAMIC_DRAW);

							glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, data.ninepatch_elements);

							glEnableVertexAttribArray(RS::ARRAY_VERTEX);
							glEnableVertexAttribArray(RS::ARRAY_TEX_UV);

							glVertexAttribPointer(RS::ARRAY_VERTEX, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), NULL);
							glVertexAttribPointer(RS::ARRAY_TEX_UV, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), CAST_INT_TO_UCHAR_PTR((sizeof(float) * 2)));

							glDrawElements(GL_TRIANGLES, 18 * 3 - (np->draw_center ? 0 : 6), GL_UNSIGNED_BYTE, NULL);

							glBindBuffer(GL_ARRAY_BUFFER, 0);
							glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
							//TODO: add profiling info storage->info.render._2d_draw_call_count++;

						} break;
#if 0
						case Item::Command::TYPE_CIRCLE: {
							Item::CommandCircle *circle = static_cast<Item::CommandCircle *>(command);

							_set_texture_rect_mode(false);

							if (state.canvas_shader->bind()) {
								_set_canvas_uniforms();
								state.canvas_shader->use_material((void *)p_material);
							}

							static const int num_points = 32;

							Vector2 points[num_points + 1];
							points[num_points] = circle->pos;

							int indices[num_points * 3];

							for (int j = 0; j < num_points; j++) {
								points[j] = circle->pos + Vector2(Math::sin(j * Math_PI * 2.0 / num_points), Math::cos(j * Math_PI * 2.0 / num_points)) * circle->radius;
								indices[j * 3 + 0] = j;
								indices[j * 3 + 1] = (j + 1) % num_points;
								indices[j * 3 + 2] = num_points;
							}

							RS::CanvasItemTextureRepeat repeat = state.default_repeat;
							if (p_current_clip->texture_repeat != RS::CANVAS_ITEM_TEXTURE_REPEAT_DEFAULT) {
								repeat = p_current_clip->texture_repeat;
							}
														RS::CanvasItemTextureFilter filter = state.default_filter;;
							if (p_current_clip->texture_filter != RS::CANVAS_ITEM_TEXTURE_FILTER_DEFAULT) {
								filter = p_current_clip->texture_filter;
							}

							_bind_canvas_texture(r->texture, filter, repeat);

							_draw_polygon(indices, num_points * 3, num_points + 1, points, NULL, &circle->color, true);
						} break;
#endif
						case Item::Command::TYPE_POLYGON: {
							Item::CommandPolygon *polygon = static_cast<Item::CommandPolygon *>(command);
							//const PolyData &pd = _polydata[polygon->polygon.polygon_id];

							switch (polygon->primitive) {
								case RS::PRIMITIVE_TRIANGLES: {
									_legacy_draw_poly_triangles(polygon, p_material);
								} break;
								default:
									break;
							}

						} break;
#if 0
						case Item::Command::TYPE_MESH: {
							Item::CommandMesh *mesh = static_cast<Item::CommandMesh *>(command);

							_set_texture_rect_mode(false);

							if (state.canvas_shader->bind()) {
								_set_canvas_uniforms();
								state.canvas_shader->use_material((void *)p_material);
							}

							RasterizerStorageGLES2::Texture *texture = _bind_canvas_texture(mesh->texture, mesh->normal_map);

							if (texture) {
								Size2 texpixel_size(1.0 / texture->width, 1.0 / texture->height);
								state.canvas_shader->set_uniform(CanvasShaderGLES2::COLOR_TEXPIXEL_SIZE, texpixel_size);
							}

							RasterizerStorageGLES2::Mesh *mesh_data = storage->mesh_owner.getornull(mesh->mesh);
							if (mesh_data) {
								for (int j = 0; j < mesh_data->surfaces.size(); j++) {
									RasterizerStorageGLES2::Surface *s = mesh_data->surfaces[j];
									// materials are ignored in 2D meshes, could be added but many things (ie, lighting mode, reading from screen, etc) would break as they are not meant be set up at this point of drawing

									glBindBuffer(GL_ARRAY_BUFFER, s->vertex_id);

									if (s->index_array_len > 0) {
										glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s->index_id);
									}

									for (int k = 0; k < RS::ARRAY_MAX - 1; k++) {
										if (s->attribs[k].enabled) {
											glEnableVertexAttribArray(k);
											glVertexAttribPointer(s->attribs[k].index, s->attribs[k].size, s->attribs[k].type, s->attribs[k].normalized, s->attribs[k].stride, CAST_INT_TO_UCHAR_PTR(s->attribs[k].offset));
										} else {
											glDisableVertexAttribArray(k);
											switch (k) {
												case RS::ARRAY_NORMAL: {
													glVertexAttrib4f(RS::ARRAY_NORMAL, 0.0, 0.0, 1, 1);
												} break;
												case RS::ARRAY_COLOR: {
													glVertexAttrib4f(RS::ARRAY_COLOR, 1, 1, 1, 1);

												} break;
												default: {
												}
											}
										}
									}

									if (s->index_array_len > 0) {
										glDrawElements(gl_primitive[s->primitive], s->index_array_len, (s->array_len >= (1 << 16)) ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT, 0);
									} else {
										glDrawArrays(gl_primitive[s->primitive], 0, s->array_len);
									}
								}

								for (int j = 1; j < RS::ARRAY_MAX - 1; j++) {
									glDisableVertexAttribArray(j);
								}
							}

							//TODO: add profiling info storage->info.render._2d_draw_call_count++;
						} break;
						case Item::Command::TYPE_MULTIMESH: {
							Item::CommandMultiMesh *mmesh = static_cast<Item::CommandMultiMesh *>(command);

							RasterizerStorageGLES2::MultiMesh *multi_mesh = storage->multimesh_owner.getornull(mmesh->multimesh);

							if (!multi_mesh)
								break;

							RasterizerStorageGLES2::Mesh *mesh_data = storage->mesh_owner.getornull(multi_mesh->mesh);

							if (!mesh_data)
								break;

							//state.canvas_shader->set_conditional(CanvasShaderGLES2::USE_INSTANCE_CUSTOM, multi_mesh->custom_data_format != RS::MULTIMESH_CUSTOM_DATA_NONE);
							if(multi_mesh->custom_data_format != RS::MULTIMESH_CUSTOM_DATA_NONE) {
								state.specialization |= CanvasShaderGLES2::USE_INSTANCE_CUSTOM;
							} else {
								state.specialization &= ~(CanvasShaderGLES2::USE_INSTANCE_CUSTOM);
							}
							//state.canvas_shader->set_conditional(CanvasShaderGLES2::USE_INSTANCING, true);
							state.specialization |= CanvasShaderGLES2::USE_INSTANCING;
							_set_texture_rect_mode(false);

							if (state.canvas_shader->bind()) {
								_set_canvas_uniforms();
								state.canvas_shader->use_material((void *)p_material);
							}

							RasterizerStorageGLES2::Texture *texture = _bind_canvas_texture(mmesh->texture, mmesh->normal_map);

							if (texture) {
								Size2 texpixel_size(1.0 / texture->width, 1.0 / texture->height);
								state.canvas_shader->set_uniform(CanvasShaderGLES2::COLOR_TEXPIXEL_SIZE, texpixel_size);
							}

							//reset shader and force rebind

							int amount = MIN(multi_mesh->size, multi_mesh->visible_instances);

							if (amount == -1) {
								amount = multi_mesh->size;
							}

							int stride = multi_mesh->color_floats + multi_mesh->custom_data_floats + multi_mesh->xform_floats;

							int color_ofs = multi_mesh->xform_floats;
							int custom_data_ofs = color_ofs + multi_mesh->color_floats;

							// drawing

							const float *base_buffer = multi_mesh->data.ptr();

							for (int j = 0; j < mesh_data->surfaces.size(); j++) {
								RasterizerStorageGLES2::Surface *s = mesh_data->surfaces[j];
								// materials are ignored in 2D meshes, could be added but many things (ie, lighting mode, reading from screen, etc) would break as they are not meant be set up at this point of drawing

								//bind buffers for mesh surface
								glBindBuffer(GL_ARRAY_BUFFER, s->vertex_id);

								if (s->index_array_len > 0) {
									glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s->index_id);
								}

								for (int k = 0; k < RS::ARRAY_MAX - 1; k++) {
									if (s->attribs[k].enabled) {
										glEnableVertexAttribArray(k);
										glVertexAttribPointer(s->attribs[k].index, s->attribs[k].size, s->attribs[k].type, s->attribs[k].normalized, s->attribs[k].stride, CAST_INT_TO_UCHAR_PTR(s->attribs[k].offset));
									} else {
										glDisableVertexAttribArray(k);
										switch (k) {
											case RS::ARRAY_NORMAL: {
												glVertexAttrib4f(RS::ARRAY_NORMAL, 0.0, 0.0, 1, 1);
											} break;
											case RS::ARRAY_COLOR: {
												glVertexAttrib4f(RS::ARRAY_COLOR, 1, 1, 1, 1);

											} break;
											default: {
											}
										}
									}
								}

								for (int k = 0; k < amount; k++) {
									const float *buffer = base_buffer + k * stride;

									{
										glVertexAttrib4fv(INSTANCE_ATTRIB_BASE + 0, &buffer[0]);
										glVertexAttrib4fv(INSTANCE_ATTRIB_BASE + 1, &buffer[4]);
										if (multi_mesh->transform_format == RS::MULTIMESH_TRANSFORM_3D) {
											glVertexAttrib4fv(INSTANCE_ATTRIB_BASE + 2, &buffer[8]);
										} else {
											glVertexAttrib4f(INSTANCE_ATTRIB_BASE + 2, 0.0, 0.0, 1.0, 0.0);
										}
									}

									if (multi_mesh->color_floats) {
										if (multi_mesh->color_format == RS::MULTIMESH_COLOR_8BIT) {
											uint8_t *color_data = (uint8_t *)(buffer + color_ofs);
											glVertexAttrib4f(INSTANCE_ATTRIB_BASE + 3, color_data[0] / 255.0, color_data[1] / 255.0, color_data[2] / 255.0, color_data[3] / 255.0);
										} else {
											glVertexAttrib4fv(INSTANCE_ATTRIB_BASE + 3, buffer + color_ofs);
										}
									} else {
										glVertexAttrib4f(INSTANCE_ATTRIB_BASE + 3, 1.0, 1.0, 1.0, 1.0);
									}

									if (multi_mesh->custom_data_floats) {
										if (multi_mesh->custom_data_format == RS::MULTIMESH_CUSTOM_DATA_8BIT) {
											uint8_t *custom_data = (uint8_t *)(buffer + custom_data_ofs);
											glVertexAttrib4f(INSTANCE_ATTRIB_BASE + 4, custom_data[0] / 255.0, custom_data[1] / 255.0, custom_data[2] / 255.0, custom_data[3] / 255.0);
										} else {
											glVertexAttrib4fv(INSTANCE_ATTRIB_BASE + 4, buffer + custom_data_ofs);
										}
									}

									if (s->index_array_len > 0) {
										glDrawElements(gl_primitive[s->primitive], s->index_array_len, (s->array_len >= (1 << 16)) ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT, 0);
									} else {
										glDrawArrays(gl_primitive[s->primitive], 0, s->array_len);
									}
								}
							}

							// LIGHT ANGLE PR replaced USE_INSTANCE_CUSTOM line with below .. think it was a typo,
							// but just in case, made this note.
							//_set_texture_rect_mode(false);
							//state.canvas_shader->set_conditional(CanvasShaderGLES2::USE_INSTANCE_CUSTOM, false);
							state.specialization &= ~CanvasShaderGLES2::USE_INSTANCE_CUSTOM;
							//state.canvas_shader->set_conditional(CanvasShaderGLES2::USE_INSTANCING, false);
							state.specialization &= ~CanvasShaderGLES2::USE_INSTANCE_CUSTOM;

							//TODO: add profiling info storage->info.render._2d_draw_call_count++;
						} break;
						case Item::Command::TYPE_POLYLINE: {
							Item::CommandPolyLine *pline = static_cast<Item::CommandPolyLine *>(command);

							_set_texture_rect_mode(false);

							if (state.canvas_shader->bind()) {
								_set_canvas_uniforms();
								state.canvas_shader->use_material((void *)p_material);
							}

							_bind_canvas_texture(RID(), RID());

							if (pline->triangles.size()) {
								_draw_generic(GL_TRIANGLE_STRIP, pline->triangles.size(), pline->triangles.ptr(), NULL, pline->triangle_colors.ptr(), pline->triangle_colors.size() == 1);
#ifdef GLES_OVER_GL
								glEnable(GL_LINE_SMOOTH);
								if (pline->multiline) {
									//needs to be different
								} else {
									_draw_generic(GL_LINE_LOOP, pline->lines.size(), pline->lines.ptr(), NULL, pline->line_colors.ptr(), pline->line_colors.size() == 1);
								}
								glDisable(GL_LINE_SMOOTH);
#endif
							} else {
#ifdef GLES_OVER_GL
								if (pline->antialiased)
									glEnable(GL_LINE_SMOOTH);
#endif

								if (pline->multiline) {
									int todo = pline->lines.size() / 2;
									int max_per_call = data.polygon_buffer_size / (sizeof(real_t) * 4);
									int offset = 0;

									while (todo) {
										int to_draw = MIN(max_per_call, todo);
										_draw_generic(GL_LINES, to_draw * 2, &pline->lines.ptr()[offset], NULL, pline->line_colors.size() == 1 ? pline->line_colors.ptr() : &pline->line_colors.ptr()[offset], pline->line_colors.size() == 1);
										todo -= to_draw;
										offset += to_draw * 2;
									}
								} else {
									_draw_generic(GL_LINE_STRIP, pline->lines.size(), pline->lines.ptr(), NULL, pline->line_colors.ptr(), pline->line_colors.size() == 1);
								}

#ifdef GLES_OVER_GL
								if (pline->antialiased)
									glDisable(GL_LINE_SMOOTH);
#endif
							}
						} break;

						case Item::Command::TYPE_PRIMITIVE: {
							Item::CommandPrimitive *primitive = static_cast<Item::CommandPrimitive *>(command);
							_set_texture_rect_mode(false);

							if (state.canvas_shader->bind()) {
								_set_canvas_uniforms();
								state.canvas_shader->use_material((void *)p_material);
							}

							ERR_CONTINUE(primitive->points.size() < 1);

							RasterizerStorageGLES2::Texture *texture = _bind_canvas_texture(primitive->texture, primitive->normal_map);

							if (texture) {
								Size2 texpixel_size(1.0 / texture->width, 1.0 / texture->height);
								state.canvas_shader->set_uniform(CanvasShaderGLES2::COLOR_TEXPIXEL_SIZE, texpixel_size);
							}

							// we need a temporary because this must be nulled out
							// if only a single color specified
							const Color *colors = primitive->colors.ptr();
							if (primitive->colors.size() == 1 && primitive->points.size() > 1) {
								Color c = primitive->colors[0];
								glVertexAttrib4f(RS::ARRAY_COLOR, c.r, c.g, c.b, c.a);
								colors = nullptr;
							} else if (primitive->colors.empty()) {
								glVertexAttrib4f(RS::ARRAY_COLOR, 1, 1, 1, 1);
							}
#ifdef RASTERIZER_EXTRA_CHECKS
							else {
								RAST_DEV_DEBUG_ASSERT(primitive->colors.size() == primitive->points.size());
							}

							if (primitive->uvs.ptr()) {
								RAST_DEV_DEBUG_ASSERT(primitive->uvs.size() == primitive->points.size());
							}
#endif

							_draw_gui_primitive(primitive->points.size(), primitive->points.ptr(), colors, primitive->uvs.ptr());
						} break;
#endif
						case Item::Command::TYPE_TRANSFORM: {
							Item::CommandTransform *transform = static_cast<Item::CommandTransform *>(command);
							state.uniforms.extra_matrix = transform->xform;
							state.canvas_shader->version_set_uniform(CanvasShaderGLES2::EXTRA_MATRIX, state.uniforms.extra_matrix, state.shader_version, state.mode_variant, state.specialization);
						} break;

						case Item::Command::TYPE_PARTICLES: {
						} break;
						case Item::Command::TYPE_CLIP_IGNORE: {
							Item::CommandClipIgnore *ci = static_cast<Item::CommandClipIgnore *>(command);
							if (p_current_clip) {
								if (ci->ignore != r_reclip) {
									if (ci->ignore) {
										glDisable(GL_SCISSOR_TEST);
										r_reclip = true;
									} else {
										glEnable(GL_SCISSOR_TEST);

										int x = p_current_clip->final_clip_rect.position.x;

										Size2i rt_size = texture_storage->get_render_target(state.render_target)->size;

										int y = rt_size.height - (p_current_clip->final_clip_rect.position.y + p_current_clip->final_clip_rect.size.y);
										int w = p_current_clip->final_clip_rect.size.x;
										int h = p_current_clip->final_clip_rect.size.y;

										// FTODO
										//										if (storage->frame.current_rt->flags[RendererStorage::RENDER_TARGET_VFLIP])
										//											y = p_current_clip->final_clip_rect.position.y;

										glScissor(x, y, w, h);

										r_reclip = false;
									}
								}
							}

						} break;
						default: {
							// FIXME: Proper error handling if relevant
							WARN_PRINT("GLES2::render_batches unknown item command type!");
							//print_line("other");
						} break;
					}
				}

			} // default
			break;
		}
	}
}

void RasterizerCanvasGLES2::canvas_end() {
	batch_canvas_end();

	//RasterizerCanvasBaseGLES2::canvas_end();

	glBindBuffer(GL_ARRAY_BUFFER, 0);

	for (int i = 0; i < RS::ArrayType::ARRAY_MAX; i++) {
		glDisableVertexAttribArray(i);
	}

	Size2 render_target_size = texture_storage->render_target_get_size(state.render_target);
	glViewport(0, 0, render_target_size.x, render_target_size.y);
	glScissor(0, 0, render_target_size.x, render_target_size.y);

	/*
		if (storage->frame.current_rt && storage->frame.current_rt->flags[RendererStorage::RENDER_TARGET_DIRECT_TO_SCREEN]) {
			//reset viewport to full window size
			//		int viewport_width = OS::get_singleton()->get_window_size().width;
			//		int viewport_height = OS::get_singleton()->get_window_size().height;
			int viewport_width = storage->_dims.win_width;
			int viewport_height = storage->_dims.win_height;
			glViewport(0, 0, viewport_width, viewport_height);
			glScissor(0, 0, viewport_width, viewport_height);
		}
	*/

	state.using_skeleton = false;
	state.using_ninepatch = false;
}

void RasterizerCanvasGLES2::canvas_begin(RID p_to_render_target, bool p_to_backbuffer) {
	batch_canvas_begin();

	// always start with light_angle unset
	/*
	state.canvas_shader.set_conditional(CanvasShaderGLES2::USE_ATTRIB_LIGHT_ANGLE, false);
	state.canvas_shader.set_conditional(CanvasShaderGLES2::USE_ATTRIB_MODULATE, false);
	state.canvas_shader.set_conditional(CanvasShaderGLES2::USE_ATTRIB_LARGE_VERTEX, false);
	state.canvas_shader.bind();
*/
	state.specialization = 0;
	state.mode_variant = CanvasShaderGLES2::ShaderVariant::MODE_QUAD;

	GLES::RenderTarget *render_target = texture_storage->get_render_target(p_to_render_target);

	if (p_to_backbuffer) {
		glBindFramebuffer(GL_FRAMEBUFFER, render_target->backbuffer_fbo);
		glActiveTexture(GL_TEXTURE0 + config->max_texture_image_units - 4);
		GLES::Texture *tex = texture_storage->get_texture(texture_storage->texture_gl_get_default(GLES::DEFAULT_GL_TEXTURE_WHITE));
		glBindTexture(GL_TEXTURE_2D, tex->tex_id);
	} else {
		glBindFramebuffer(GL_FRAMEBUFFER, render_target->fbo);
		glActiveTexture(GL_TEXTURE0 + config->max_texture_image_units - 4);
		glBindTexture(GL_TEXTURE_2D, render_target->backbuffer);
	}

	if (render_target->is_transparent || p_to_backbuffer) {
		//state.transparent_render_target = true;
		glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	} else {
		//state.transparent_render_target = false;
		glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE);
	}

	if (render_target && render_target->clear_requested) {
		const Color &col = render_target->clear_color;
		glClearColor(col.r, col.g, col.b, render_target->is_transparent ? col.a : 1.0f);

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
		render_target->clear_requested = false;
	}

	Size2 render_target_size = texture_storage->render_target_get_size(p_to_render_target);
	glViewport(0, 0, render_target_size.x, render_target_size.y);

	reset_canvas();

	glActiveTexture(GL_TEXTURE0);
	GLES::Texture *tex_white = texture_storage->get_texture(texture_storage->texture_gl_get_default(GLES::DEFAULT_GL_TEXTURE_WHITE));
	glBindTexture(GL_TEXTURE_2D, tex_white->tex_id);

	glVertexAttrib4f(RS::ARRAY_COLOR, 1, 1, 1, 1);
	glDisableVertexAttribArray(RS::ARRAY_COLOR);

	// set up default uniforms

	Transform3D canvas_transform;

	if (state.render_target != RID()) {
		float csy = 1.0;

		// FTODO
		//		if (storage->frame.current_rt && storage->frame.current_rt->flags[RendererStorage::RENDER_TARGET_VFLIP]) {
		//			csy = -1.0;
		//		}
		Size2i rt_size = texture_storage->get_render_target(state.render_target)->size;
		canvas_transform.translate_local(-(rt_size.width / 2.0f), -(rt_size.height / 2.0f), 0.0f);
		canvas_transform.scale(Vector3(2.0f / rt_size.width, csy * -2.0f / rt_size.height, 1.0f));
	} else {
		// FTODO
		Vector2 ssize = DisplayServer::get_singleton()->window_get_size();

		canvas_transform.translate_local(-(ssize.width / 2.0f), -(ssize.height / 2.0f), 0.0f);
		canvas_transform.scale(Vector3(2.0f / ssize.width, -2.0f / ssize.height, 1.0f));
	}

	state.uniforms.projection_matrix = canvas_transform;

	state.uniforms.final_modulate = Color(1, 1, 1, 1);

	state.uniforms.modelview_matrix = Transform2D();
	state.uniforms.extra_matrix = Transform2D();

	// FIXME: This won't work here, at this point we don't have a shader version and thus can't really set the uniform. This was designed to work with UBOs, but since we don't have those we will need to do this every time.
	//_set_canvas_uniforms();
	//_bind_quad_buffer();
}

void RasterizerCanvasGLES2::reset_canvas() {
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_SCISSOR_TEST);
	glDisable(GL_DITHER);
	glEnable(GL_BLEND);

	bool transparent_rt = state.render_target != RID() && texture_storage->get_render_target(state.render_target)->is_transparent;

	if (transparent_rt) {
		glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	} else {
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}

	// bind the back buffer to a texture so shaders can use it.
	// It should probably use texture unit -3 (as GLES2 does as well) but currently that's buggy.
	// keeping this for now as there's nothing else that uses texture unit 2
	// TODO ^
	if (state.render_target != RID()) {
		// glActiveTexture(GL_TEXTURE0 + 2);
		// glBindTexture(GL_TEXTURE_2D, storage->frame.current_rt->copy_screen_effect.color);
	}

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void RasterizerCanvasGLES2::canvas_render_items_begin(const Color &p_modulate, Light *p_light, const Transform2D &p_base_transform) {
	batch_canvas_render_items_begin(p_modulate, p_light, p_base_transform);
}

void RasterizerCanvasGLES2::canvas_render_items_end() {
	batch_canvas_render_items_end();
}

void RasterizerCanvasGLES2::canvas_render_items_internal(Item *p_item_list, int p_z, const Color &p_modulate, Light *p_light, const Transform2D &p_base_transform) {
	batch_canvas_render_items(p_item_list, p_z, p_modulate, p_light, p_base_transform);

	//glClearColor(Math::randf(), 0, 1, 1);
}

void RasterizerCanvasGLES2::canvas_render_items_implementation(Item *p_item_list, int p_z, const Color &p_modulate, Light *p_light, const Transform2D &p_base_transform) {
	// parameters are easier to pass around in a structure
	RenderItemState ris;
	ris.item_group_z = p_z;
	ris.item_group_modulate = p_modulate;
	ris.item_group_light = p_light;
	ris.item_group_base_transform = p_base_transform;

	//state.canvas_shader->set_conditional(CanvasShaderGLES2::USE_SKELETON, false);
	state.specialization |= CanvasShaderGLES2::USE_SKELETON;

	state.current_tex = RID();
	state.canvas_texscreen_used = false;

	texture_storage->texture_bind(texture_storage->texture_gl_get_default(GLES::DEFAULT_GL_TEXTURE_WHITE), 0);

	if (bdata.settings_use_batching) {
#ifdef GODOT_3
		//render using the batched result, currently disabled
		for (int j = 0; j < bdata.items_joined.size(); j++) {
			render_joined_item(bdata.items_joined[j], ris);
		}
#endif
	} else {
		while (p_item_list) {
			Item *ci = p_item_list;

			_legacy_canvas_render_item(ci, ris);
			p_item_list = p_item_list->next;
		}
	}

	if (ris.current_clip) {
		glDisable(GL_SCISSOR_TEST);
	}

	if (ris.current_clip) {
		glDisable(GL_SCISSOR_TEST);
	}

	//state.canvas_shader->set_conditional(CanvasShaderGLES2::USE_SKELETON, false);
	state.specialization &= ~CanvasShaderGLES2::USE_SKELETON;
}

#ifdef GODOT_3
// This function is a dry run of the state changes when drawing the item.
// It should duplicate the logic in _canvas_render_item,
// to decide whether items are similar enough to join
// i.e. no state differences between the 2 items.
bool RasterizerCanvasGLES2::try_join_item(Item *p_ci, RenderItemState &r_ris, bool &r_batch_break) {
	// if we set max join items to zero we can effectively prevent any joining, so
	// none of the other logic needs to run. Good for testing regression bugs, and
	// could conceivably be faster in some games.
	if (!bdata.settings_max_join_item_commands) {
		return false;
	}

	// if there are any state changes we change join to false
	// we also set r_batch_break to true if we don't want this item joined to the next
	// (e.g. an item that must not be joined at all)
	r_batch_break = false;
	bool join = true;

	// light_masked may possibly need state checking here. Check for regressions!

	// we will now allow joining even if final modulate is different
	// we will instead bake the final modulate into the vertex colors
	//	if (p_ci->final_modulate != r_ris.final_modulate) {
	//		join = false;
	//		r_ris.final_modulate = p_ci->final_modulate;
	//	}

	if (r_ris.current_clip != p_ci->final_clip_owner) {
		r_ris.current_clip = p_ci->final_clip_owner;
		join = false;
	}

	// TODO: copy back buffer

	if (p_ci->copy_back_buffer) {
		join = false;
	}

	RasterizerStorageGLES2::Skeleton *skeleton = NULL;

	{
		//skeleton handling
		if (p_ci->skeleton.is_valid() && storage->skeleton_owner.owns(p_ci->skeleton)) {
			skeleton = storage->skeleton_owner.get(p_ci->skeleton);
			if (!skeleton->use_2d) {
				skeleton = NULL;
			}
		}

		bool skeleton_prevent_join = false;

		bool use_skeleton = skeleton != NULL;
		if (r_ris.prev_use_skeleton != use_skeleton) {
			if (!bdata.settings_use_software_skinning)
				r_ris.rebind_shader = true;

			r_ris.prev_use_skeleton = use_skeleton;
			//			join = false;
			skeleton_prevent_join = true;
		}

		if (skeleton) {
			//			join = false;
			skeleton_prevent_join = true;
			state.using_skeleton = true;
		} else {
			state.using_skeleton = false;
		}

		if (skeleton_prevent_join) {
			if (!bdata.settings_use_software_skinning)
				join = false;
		}
	}

	Item *material_owner = p_ci->material_owner ? p_ci->material_owner : p_ci;

	RID material = material_owner->material;
	RasterizerStorageGLES2::Material *material_ptr = storage->material_owner.getornull(material);

	if (material != r_ris.canvas_last_material || r_ris.rebind_shader) {
		join = false;
		RasterizerStorageGLES2::Shader *shader_ptr = NULL;

		if (material_ptr) {
			shader_ptr = material_ptr->shader;

			if (shader_ptr && shader_ptr->mode != RS::SHADER_CANVAS_ITEM) {
				shader_ptr = NULL; // not a canvas item shader, don't use.
			}
		}

		if (shader_ptr) {
			if (shader_ptr->canvas_item.uses_screen_texture) {
				if (!state.canvas_texscreen_used) {
					join = false;
				}
			}
		}

		r_ris.shader_cache = shader_ptr;

		r_ris.canvas_last_material = material;

		r_ris.rebind_shader = false;
	}

	int blend_mode = r_ris.shader_cache ? r_ris.shader_cache->canvas_item.blend_mode : RasterizerStorageGLES2::Shader::CanvasItem::BLEND_MODE_MIX;
	bool unshaded = r_ris.shader_cache && (r_ris.shader_cache->canvas_item.light_mode == RasterizerStorageGLES2::Shader::CanvasItem::LIGHT_MODE_UNSHADED || (blend_mode != RasterizerStorageGLES2::Shader::CanvasItem::BLEND_MODE_MIX && blend_mode != RasterizerStorageGLES2::Shader::CanvasItem::BLEND_MODE_PMALPHA));
	bool reclip = false;

	// we are precalculating the final_modulate ahead of time because we need this for baking of final modulate into vertex colors
	// (only in software transform mode)
	// This maybe inefficient storing it...
	r_ris.final_modulate = unshaded ? p_ci->final_modulate : (p_ci->final_modulate * r_ris.item_group_modulate);

	if (r_ris.last_blend_mode != blend_mode) {
		join = false;
		r_ris.last_blend_mode = blend_mode;
	}

	// does the shader contain BUILTINs which should break the batching?
	bdata.joined_item_batch_flags = 0;
	if (r_ris.shader_cache) {
		unsigned int and_flags = r_ris.shader_cache->canvas_item.batch_flags & (BatcherEnums::PREVENT_COLOR_BAKING | BatcherEnums::PREVENT_VERTEX_BAKING | BatcherEnums::PREVENT_ITEM_JOINING);
		if (and_flags) {
			// special case for preventing item joining altogether
			if (and_flags & BatcherEnums::PREVENT_ITEM_JOINING) {
				join = false;
				//r_batch_break = true; // don't think we need a batch break

				// save the flags so that they don't need to be recalculated in the 2nd pass
				bdata.joined_item_batch_flags |= r_ris.shader_cache->canvas_item.batch_flags;
			} else {
				bool use_larger_fvfs = true;

				if (and_flags == BatcherEnums::PREVENT_COLOR_BAKING) {
					// in some circumstances, if the modulate is identity, we still allow baking because reading modulate / color
					// will still be okay to do in the shader with no ill effects
					if (r_ris.final_modulate == Color(1, 1, 1, 1)) {
						use_larger_fvfs = false;
					}
				}

				// new .. always use large FVF
				if (use_larger_fvfs) {
					if (and_flags == BatcherEnums::PREVENT_COLOR_BAKING) {
						bdata.joined_item_batch_flags |= BatcherEnums::USE_MODULATE_FVF;
					} else {
						// we need to save on the joined item that it should use large fvf.
						// This info will then be used in filling and rendering
						bdata.joined_item_batch_flags |= BatcherEnums::USE_LARGE_FVF;
					}

					bdata.joined_item_batch_flags |= r_ris.shader_cache->canvas_item.batch_flags;
				}

#if 0
			if (and_flags == BatcherEnums::PREVENT_COLOR_BAKING) {
				// in some circumstances, if the modulate is identity, we still allow baking because reading modulate / color
				// will still be okay to do in the shader with no ill effects
				if (r_ris.final_modulate == Color(1, 1, 1, 1)) {
					break_batching = false;
				}
				else
				{
					// new .. large FVF
					break_batching = false;

					// we need to save on the joined item that it should use large fvf.
					// This info will then be used in filling and rendering
					bdata.joined_item_batch_flags |= BatcherEnums::USE_LARGE_FVF;
				}
			}

			if (break_batching) {
				join = false;
				r_batch_break = true;

				// save the flags so that they don't need to be recalculated in the 2nd pass
				bdata.joined_item_batch_flags |= r_ris.shader_cache->canvas_item.batch_flags;
			}
#endif
			} // if not prevent item joining
		}
	}

	if ((blend_mode == RasterizerStorageGLES2::Shader::CanvasItem::BLEND_MODE_MIX || blend_mode == RasterizerStorageGLES2::Shader::CanvasItem::BLEND_MODE_PMALPHA) && r_ris.item_group_light && !unshaded) {
		// we cannot join lit items easily.
		// it is possible, but not if they overlap, because
		// a + light_blend + b + light_blend IS NOT THE SAME AS
		// a + b + light_blend

		bool light_allow_join = true;

		// this is a quick getout if we have turned off light joining
		if ((bdata.settings_light_max_join_items == 0) || r_ris.light_region.too_many_lights) {
			light_allow_join = false;
		} else {
			// do light joining...

			// first calculate the light bitfield
			uint64_t light_bitfield = 0;
			uint64_t shadow_bitfield = 0;
			Light *light = r_ris.item_group_light;

			int light_count = -1;
			while (light) {
				light_count++;
				uint64_t light_bit = 1ULL << light_count;

				// note that as a cost of batching, the light culling will be less effective
				if (p_ci->light_mask & light->item_mask && r_ris.item_group_z >= light->z_min && r_ris.item_group_z <= light->z_max) {
					// Note that with the above test, it is possible to also include a bound check.
					// Tests so far have indicated better performance without it, but there may be reason to change this at a later stage,
					// so I leave the line here for reference:
					// && p_ci->global_rect_cache.intersects_transformed(light->xform_cache, light->rect_cache)) {
					light_bitfield |= light_bit;

					bool has_shadow = light->shadow_buffer.is_valid() && p_ci->light_mask & light->item_shadow_mask;

					if (has_shadow) {
						shadow_bitfield |= light_bit;
					}
				}

				light = light->next_ptr;
			}

			// now compare to previous
			if ((r_ris.light_region.light_bitfield != light_bitfield) || (r_ris.light_region.shadow_bitfield != shadow_bitfield)) {
				light_allow_join = false;

				r_ris.light_region.light_bitfield = light_bitfield;
				r_ris.light_region.shadow_bitfield = shadow_bitfield;
			} else {
				// only do these checks if necessary
				if (join && (!r_batch_break)) {
					// we still can't join, even if the lights are exactly the same, if there is overlap between the previous and this item
					if (r_ris.joined_item && light_bitfield) {
						if ((int)r_ris.joined_item->num_item_refs <= bdata.settings_light_max_join_items) {
							for (uint32_t r = 0; r < r_ris.joined_item->num_item_refs; r++) {
								Item *pRefItem = bdata.item_refs[r_ris.joined_item->first_item_ref + r].item;
								if (p_ci->global_rect_cache.intersects(pRefItem->global_rect_cache)) {
									light_allow_join = false;
									break;
								}
							}

#ifdef DEBUG_ENABLED
							if (light_allow_join) {
								bdata.stats_light_items_joined++;
							}
#endif

						} // if below max join items
						else {
							// just don't allow joining if above overlap check max items
							light_allow_join = false;
						}
					}

				} // if not batch broken already (no point in doing expensive overlap tests if not needed)
			} // if bitfields don't match
		} // if do light joining

		if (!light_allow_join) {
			// can't join
			join = false;
			// we also dont want to allow joining this item with the next item, because the next item could have no lights!
			r_batch_break = true;
		}

	} else {
		// if the last item had lights, we should not join it to this one (which has no lights)
		if (r_ris.light_region.light_bitfield || r_ris.light_region.shadow_bitfield) {
			join = false;

			// setting these to zero ensures that any following item with lights will, by definition,
			// be affected by a different set of lights, and thus prevent a join
			r_ris.light_region.light_bitfield = 0;
			r_ris.light_region.shadow_bitfield = 0;
		}
	}

	if (reclip) {
		join = false;
	}

	// non rects will break the batching anyway, we don't want to record item changes, detect this
	if (!r_batch_break && _detect_item_batch_break(r_ris, p_ci, r_batch_break)) {
		join = false;

		r_batch_break = true;
	}

	return join;
}
#endif // godot 3

// Legacy non-batched implementation for regression testing.
// TODO: Should be removed after testing phase to avoid duplicate codepaths.
void RasterizerCanvasGLES2::_legacy_canvas_render_item(Item *p_ci, RenderItemState &r_ris) {
	//TODO: add profiling info storage->info.render._2d_item_count++;

	// TODO: reset me in the non legacy method
	state.current_filter = RS::CANVAS_ITEM_TEXTURE_FILTER_MAX;
	state.current_repeat = RS::CANVAS_ITEM_TEXTURE_REPEAT_MAX;

	if (p_ci->texture_filter != RS::CANVAS_ITEM_TEXTURE_FILTER_DEFAULT) {
		state.current_filter = p_ci->texture_filter;
	}

	if (p_ci->texture_repeat != RS::CANVAS_ITEM_TEXTURE_REPEAT_DEFAULT) {
		state.current_repeat = p_ci->texture_repeat;
	}

	if (r_ris.current_clip != p_ci->final_clip_owner) {
		r_ris.current_clip = p_ci->final_clip_owner;

		if (r_ris.current_clip) {
			glEnable(GL_SCISSOR_TEST);
			Size2 render_target_size = texture_storage->render_target_get_size(state.render_target);

			int y = render_target_size.y - (r_ris.current_clip->final_clip_rect.position.y + r_ris.current_clip->final_clip_rect.size.y);
			//			int y = storage->frame.current_rt->height - (r_ris.current_clip->final_clip_rect.position.y + r_ris.current_clip->final_clip_rect.size.y);
			// FTODO
			//			if (storage->frame.current_rt->flags[RendererStorage::RENDER_TARGET_VFLIP])
			//			y = r_ris.current_clip->final_clip_rect.position.y;
			glScissor(r_ris.current_clip->final_clip_rect.position.x, y, r_ris.current_clip->final_clip_rect.size.width, r_ris.current_clip->final_clip_rect.size.height);

			// debug VFLIP
			//			if ((r_ris.current_clip->final_clip_rect.position.x == 223)
			//					&& (y == 54)
			//					&& (r_ris.current_clip->final_clip_rect.size.width == 1383))
			//			{
			//				glScissor(r_ris.current_clip->final_clip_rect.position.x, y, r_ris.current_clip->final_clip_rect.size.width, r_ris.current_clip->final_clip_rect.size.height);
			//			}

		} else {
			glDisable(GL_SCISSOR_TEST);
		}
	}

	// TODO: copy back buffer

	if (p_ci->copy_back_buffer) {
		if (p_ci->copy_back_buffer->full) {
			_copy_texscreen(Rect2());
		} else {
			_copy_texscreen(p_ci->copy_back_buffer->rect);
		}
	}

#if 0
	RasterizerStorageGLES2::Skeleton *skeleton = NULL;

	{
		//skeleton handling
		if (p_ci->skeleton.is_valid() && storage->skeleton_owner.owns(p_ci->skeleton)) {
			skeleton = storage->skeleton_owner.get(p_ci->skeleton);
			if (!skeleton->use_2d) {
				skeleton = NULL;
			} else {
				state.skeleton_transform = r_ris.item_group_base_transform * skeleton->base_transform_2d;
				state.skeleton_transform_inverse = state.skeleton_transform.affine_inverse();
				state.skeleton_texture_size = Vector2(skeleton->size * 2, 0);
			}
		}

		bool use_skeleton = skeleton != NULL;
/*
		if (r_ris.prev_use_skeleton != use_skeleton) {
			r_ris.rebind_shader = true;
			state.canvas_shader->set_conditional(CanvasShaderGLES2::USE_SKELETON, use_skeleton);
			r_ris.prev_use_skeleton = use_skeleton;
		}
*/
		if(use_skeleton){
			state.specialization |= CanvasShaderGLES2::USE_SKELETON;
		} else {
			state.specialization &= ~CanvasShaderGLES2::USE_SKELETON;
		}

		if (skeleton) {
			glActiveTexture(GL_TEXTURE0 + storage->config.max_texture_image_units - 3);
			glBindTexture(GL_TEXTURE_2D, skeleton->tex_id);
			state.using_skeleton = true;
		} else {
			state.using_skeleton = false;
		}
	}
#endif

	RID material = p_ci->material_owner == nullptr ? p_ci->material : p_ci->material_owner->material;
	GLES::Material *material_ptr = nullptr;
	//TODO: reset the current specialisation to default
	state.specialization = 0;
	state.mode_variant = CanvasShaderGLES2::ShaderVariant::MODE_QUAD;
	state.shader_version = data.canvas_shader_default_version;

	if (material.is_valid()) {
		GLES::CanvasMaterialData *md = static_cast<GLES::CanvasMaterialData *>(material_storage->material_get_data(material, RS::SHADER_CANVAS_ITEM));
		if (md && md->shader_data->valid) {
			state.shader_version = md->shader_data->version;
		}

		material_ptr = material_storage->get_material(material);

		if (material != r_ris.canvas_last_material || r_ris.rebind_shader) {
			GLES::Shader *shader_ptr = NULL;

			if (material_ptr) {
				shader_ptr = material_ptr->shader;

				if (shader_ptr && shader_ptr->mode != RS::SHADER_CANVAS_ITEM) {
					shader_ptr = NULL; // not a canvas item shader, don't use.
				}
			}

			if (shader_ptr) {
				if (md->shader_data->uses_screen_texture) {
					if (!state.canvas_texscreen_used) {
						//copy if not copied before
						_copy_texscreen(Rect2());

						// blend mode will have been enabled so make sure we disable it again later on
						//last_blend_mode = last_blend_mode != RasterizerStorageGLES2::Shader::CanvasItem::BLEND_MODE_DISABLED ? last_blend_mode : -1;
					}

					/* TODO
					if (storage->frame.current_rt->copy_screen_effect.color) {
						glActiveTexture(GL_TEXTURE0 + storage->config.max_texture_image_units - 4);
						glBindTexture(GL_TEXTURE_2D, storage->frame.current_rt->copy_screen_effect.color);
					}
					*/
				}

				if (shader_ptr != r_ris.shader_cache) {
					if (md->shader_data->uses_time) {
						RenderingServerDefault::redraw_request();
					}

					// This should be included in shader_version state.canvas_shader->set_custom_shader(shader_ptr->custom_code_id);
					state.canvas_shader->version_bind_shader(state.shader_version, state.mode_variant, state.specialization);
				}

				// bind all the textures
				// in 4.x we can probably just use material_data->bind_uniforms()
				/*
							int tc = material_ptr->textures.size();
							Pair<StringName, RID> *textures = material_ptr->textures.ptrw();

							ShaderLanguage::ShaderNode::Uniform::Hint *texture_hints = shader_ptr->texture_hints.ptrw();

							for (int i = 0; i < tc; i++) {
								glActiveTexture(GL_TEXTURE0 + i);

								GLES::Texture *t = storage->texture_owner.getornull(textures[i].second);

								if (!t) {
									switch (texture_hints[i]) {
										case ShaderLanguage::ShaderNode::Uniform::HINT_DEFAULT_BLACK: {
											glBindTexture(GL_TEXTURE_2D, storage->resources.black_tex);
										} break;
										case ShaderLanguage::ShaderNode::Uniform::HINT_ANISOTROPY: {
											glBindTexture(GL_TEXTURE_2D, storage->resources.aniso_tex);
										} break;
										case ShaderLanguage::ShaderNode::Uniform::HINT_NORMAL: {
											glBindTexture(GL_TEXTURE_2D, storage->resources.normal_tex);
										} break;
										default: {
											GLES::Texture *tex_white = texture_storage->get_texture(texture_storage->texture_gl_get_default(GLES::DEFAULT_GL_TEXTURE_WHITE));
											glBindTexture(GL_TEXTURE_2D, tex_white->tex_id);
										} break;
									}

									continue;
								}

								if (t->redraw_if_visible) {
									RenderingServerDefault::redraw_request();
								}

								t = t->get_ptr();

				#ifdef TOOLS_ENABLED
								if (t->detect_normal && texture_hints[i] == ShaderLanguage::ShaderNode::Uniform::HINT_NORMAL) {
									t->detect_normal(t->detect_normal_ud);
								}
				#endif
								if (t->render_target)
									t->render_target->used_in_frame = true;

								glBindTexture(t->target, t->tex_id);
							}
				*/
			} else {
				// This should be included in shader_version state.canvas_shader->set_custom_shader(0);
				state.canvas_shader->version_bind_shader(state.shader_version, state.mode_variant, state.specialization);
			}
			material_ptr->data->bind_uniforms();

			r_ris.shader_cache = shader_ptr;

			r_ris.canvas_last_material = material;

			r_ris.rebind_shader = false;
		}
	}

	//TODO: make sure the types of the blend mode enum match.
	// intellisense messes this up because RasterizerCanvasBatcher is confusding

	//	int blend_mode = r_ris.shader_cache ? r_ris.shader_cache->canvas_item.blend_mode : GLES::CanvasShaderData::BLEND_MODE_MIX;
	GLES::CanvasShaderData::BlendMode blend_mode = GLES::CanvasShaderData::BLEND_MODE_MIX;
	if (r_ris.shader_cache) {
		blend_mode = static_cast<GLES::CanvasShaderData *>(r_ris.shader_cache->data)->blend_mode;
	}

	//TODO: re-add light mode
	//bool unshaded = r_ris.shader_cache && (r_ris.shader_cache->canvas_item.light_mode == CanvasItemMaterial::LightMode::LIGHT_MODE_UNSHADED || (blend_mode != GLES::CanvasShaderData::BLEND_MODE_MIX && blend_mode != GLES::CanvasShaderData::BLEND_MODE_PMALPHA));
	bool unshaded = r_ris.shader_cache && ((blend_mode != GLES::CanvasShaderData::BLEND_MODE_MIX && blend_mode != GLES::CanvasShaderData::BLEND_MODE_PMALPHA));

	bool reclip = false;

	if (r_ris.last_blend_mode != blend_mode) {
		//TODO: this looks like it might be shared with gles3, extract it later
		bool transparent_rt = state.render_target != RID() && texture_storage->get_render_target(state.render_target)->is_transparent;

		switch (blend_mode) {
				//TODO: we can get rid of all of these flag look
				//	GLES3::RenderTarget *render_target = texture_storage->get_render_target();

			case GLES::CanvasShaderData::BLEND_MODE_MIX: {
				glBlendEquation(GL_FUNC_ADD);
				if (transparent_rt) {
					glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
				} else {
					glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE);
				}

			} break;
			case GLES::CanvasShaderData::BLEND_MODE_ADD: {
				glBlendEquation(GL_FUNC_ADD);
				if (transparent_rt) {
					glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE, GL_SRC_ALPHA, GL_ONE);
				} else {
					glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE, GL_ZERO, GL_ONE);
				}

			} break;
			case GLES::CanvasShaderData::BLEND_MODE_SUB: {
				glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
				if (transparent_rt) {
					glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE, GL_SRC_ALPHA, GL_ONE);
				} else {
					glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE, GL_ZERO, GL_ONE);
				}
			} break;
			case GLES::CanvasShaderData::BLEND_MODE_MUL: {
				glBlendEquation(GL_FUNC_ADD);
				if (transparent_rt) {
					glBlendFuncSeparate(GL_DST_COLOR, GL_ZERO, GL_DST_ALPHA, GL_ZERO);
				} else {
					glBlendFuncSeparate(GL_DST_COLOR, GL_ZERO, GL_ZERO, GL_ONE);
				}
			} break;
			case GLES::CanvasShaderData::BLEND_MODE_PMALPHA: {
				glBlendEquation(GL_FUNC_ADD);
				if (transparent_rt) {
					glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
				} else {
					glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE);
				}
			} break;
			default: {
				//TODO: handle remaining cases
			}
		}
	}

	state.uniforms.final_modulate = unshaded ? p_ci->final_modulate : Color(p_ci->final_modulate.r * r_ris.item_group_modulate.r, p_ci->final_modulate.g * r_ris.item_group_modulate.g, p_ci->final_modulate.b * r_ris.item_group_modulate.b, p_ci->final_modulate.a * r_ris.item_group_modulate.a);

	state.uniforms.modelview_matrix = p_ci->final_transform;
	state.uniforms.extra_matrix = Transform2D();

	// we haven't bound any shader version yet (unless we have a material), do we need this here?
	//_set_canvas_uniforms();

	//TODO: re-add light mode
	// if (unshaded || (state.uniforms.final_modulate.a > 0.001 && (!r_ris.shader_cache || r_ris.shader_cache->canvas_item.light_mode != CanvasItemMaterial::LightMode::LIGHT_MODE_LIGHT_ONLY) && !p_ci->light_masked))
	if (unshaded || (state.uniforms.final_modulate.a > 0.001 && (!r_ris.shader_cache) && !p_ci->light_masked))
		_legacy_canvas_item_render_commands(p_ci, NULL, reclip, material_ptr);

	r_ris.rebind_shader = true; // hacked in for now.

	if ((blend_mode == GLES::CanvasShaderData::BLEND_MODE_MIX || blend_mode == GLES::CanvasShaderData::BLEND_MODE_PMALPHA) && r_ris.item_group_light && !unshaded) {
		Light *light = r_ris.item_group_light;
		bool light_used = false;
		RS::CanvasLightBlendMode bmode = RS::CANVAS_LIGHT_BLEND_MODE_ADD;
		state.uniforms.final_modulate = p_ci->final_modulate; // remove the canvas modulate

		while (light) {
			if (p_ci->light_mask & light->item_mask && r_ris.item_group_z >= light->z_min && r_ris.item_group_z <= light->z_max && p_ci->global_rect_cache.intersects_transformed(light->xform_cache, light->rect_cache)) {
				//intersects this light

				if (!light_used || bmode != light->blend_mode) {
					bmode = light->blend_mode;

					switch (bmode) {
						case RS::CANVAS_LIGHT_BLEND_MODE_ADD: {
							glBlendEquation(GL_FUNC_ADD);
							glBlendFunc(GL_SRC_ALPHA, GL_ONE);

						} break;
						case RS::CANVAS_LIGHT_BLEND_MODE_SUB: {
							glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
							glBlendFunc(GL_SRC_ALPHA, GL_ONE);
						} break;
						case RS::CANVAS_LIGHT_BLEND_MODE_MIX: {
							//						case RS::CANVAS_LIGHT_MODE_MASK: {
							glBlendEquation(GL_FUNC_ADD);
							glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

						} break;
					}
				}

				if (!light_used) {
					//state.canvas_shader->set_conditional(CanvasShaderGLES2::USE_LIGHTING, true);
					light_used = true;
				}

				state.specialization |= CanvasShaderGLES2::USE_LIGHTING;

				// FTODO
				//bool has_shadow = light->shadow_buffer.is_valid() && p_ci->light_mask & light->item_shadow_mask;
				bool has_shadow = light->use_shadow && p_ci->light_mask & light->item_shadow_mask;

				if (has_shadow) {
					//state.canvas_shader->set_conditional(CanvasShaderGLES2::USE_SHADOWS, true);
					state.specialization |= CanvasShaderGLES2::USE_SHADOWS;
					_set_shadow_filer(light->shadow_filter);
				} else {
					//state.canvas_shader->set_conditional(CanvasShaderGLES2::USE_SHADOWS, false);
					state.specialization &= ~CanvasShaderGLES2::USE_SHADOWS;
					_set_shadow_filer(RS::CanvasLightShadowFilter::CANVAS_LIGHT_FILTER_MAX);
				}

				state.canvas_shader->version_bind_shader(state.shader_version, state.mode_variant, state.specialization);
				state.using_light = light;
				state.using_shadow = has_shadow;

				//always re-set uniforms, since light parameters changed
				_set_canvas_uniforms();
				if (material.is_valid()) {
					material_ptr->data->bind_uniforms();
				}

				glActiveTexture(GL_TEXTURE0 + config->max_texture_image_units - 6);
				GLES::Texture *t = texture_storage->get_texture(light->texture);
				if (!t) {
					GLES::Texture *tex_white = texture_storage->get_texture(texture_storage->texture_gl_get_default(GLES::DEFAULT_GL_TEXTURE_WHITE));
					glBindTexture(GL_TEXTURE_2D, tex_white->tex_id);
				} else {
					//t = t->get_ptr();

					glBindTexture(t->target, t->tex_id);
				}

				glActiveTexture(GL_TEXTURE0);
				_legacy_canvas_item_render_commands(p_ci, NULL, reclip, material_ptr); //redraw using light

				state.using_light = NULL;
			}

			light = light->next_ptr;
		}

		if (light_used) {
			//state.canvas_shader->set_conditional(CanvasShaderGLES2::USE_LIGHTING, false);
			state.specialization &= ~CanvasShaderGLES2::USE_LIGHTING;
			//state.canvas_shader->set_conditional(CanvasShaderGLES2::USE_SHADOWS, false);
			state.specialization &= ~CanvasShaderGLES2::USE_SHADOWS;

			_set_shadow_filer(RS::CanvasLightShadowFilter::CANVAS_LIGHT_FILTER_MAX);

			state.canvas_shader->version_bind_shader(state.shader_version, state.mode_variant, state.specialization);

			r_ris.last_blend_mode = -1;

#if 0
			//this is set again, so it should not be needed anyway?
			state.canvas_item_modulate = unshaded ? ci->final_modulate : Color(ci->final_modulate.r * p_modulate.r, ci->final_modulate.g * p_modulate.g, ci->final_modulate.b * p_modulate.b, ci->final_modulate.a * p_modulate.a);

			state.canvas_shader->set_uniform(CanvasShaderGLES2::MODELVIEW_MATRIX, state.final_transform);
			state.canvas_shader->set_uniform(CanvasShaderGLES2::EXTRA_MATRIX, Transform2D());
			state.canvas_shader->set_uniform(CanvasShaderGLES2::FINAL_MODULATE, state.canvas_item_modulate);

			glBlendEquation(GL_FUNC_ADD);

			if (storage->frame.current_rt->flags[RendererStorage::RENDER_TARGET_TRANSPARENT]) {
				glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
			} else {
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			}

			//@TODO RESET canvas_blend_mode
#endif
		}
	}

	if (reclip) {
		glEnable(GL_SCISSOR_TEST);
		Size2i rt_size = texture_storage->get_render_target(state.render_target)->size;
		int y = rt_size.height - (r_ris.current_clip->final_clip_rect.position.y + r_ris.current_clip->final_clip_rect.size.y);
		// FTODO
		//		if (storage->frame.current_rt->flags[RendererStorage::RENDER_TARGET_VFLIP])
		//		y = r_ris.current_clip->final_clip_rect.position.y;
		glScissor(r_ris.current_clip->final_clip_rect.position.x, y, r_ris.current_clip->final_clip_rect.size.width, r_ris.current_clip->final_clip_rect.size.height);
	}
}

#ifdef GODOT_3
void RasterizerCanvasGLES2::render_joined_item(const BItemJoined &p_bij, RenderItemState &r_ris) {
	//TODO: add profiling info storage->info.render._2d_item_count++;

#ifdef DEBUG_ENABLED
	if (bdata.diagnose_frame) {
		bdata.frame_string += "\tjoined_item " + itos(p_bij.num_item_refs) + " refs\n";
		if (p_bij.z_index != 0) {
			bdata.frame_string += "\t\t(z " + itos(p_bij.z_index) + ")\n";
		}
	}
#endif

	// all the joined items will share the same state with the first item
	Item *ci = bdata.item_refs[p_bij.first_item_ref].item;

	if (r_ris.current_clip != ci->final_clip_owner) {
		r_ris.current_clip = ci->final_clip_owner;

		if (r_ris.current_clip) {
			glEnable(GL_SCISSOR_TEST);
			int y = storage->frame.current_rt->height - (r_ris.current_clip->final_clip_rect.position.y + r_ris.current_clip->final_clip_rect.size.y);
			if (storage->frame.current_rt->flags[RendererStorage::RENDER_TARGET_VFLIP])
				y = r_ris.current_clip->final_clip_rect.position.y;
			glScissor(r_ris.current_clip->final_clip_rect.position.x, y, r_ris.current_clip->final_clip_rect.size.width, r_ris.current_clip->final_clip_rect.size.height);
		} else {
			glDisable(GL_SCISSOR_TEST);
		}
	}

	// TODO: copy back buffer

	if (ci->copy_back_buffer) {
		if (ci->copy_back_buffer->full) {
			_copy_texscreen(Rect2());
		} else {
			_copy_texscreen(ci->copy_back_buffer->rect);
		}
	}

	if (!bdata.settings_use_batching || !bdata.settings_use_software_skinning) {
		RasterizerStorageGLES2::Skeleton *skeleton = NULL;

		//skeleton handling
		if (ci->skeleton.is_valid() && storage->skeleton_owner.owns(ci->skeleton)) {
			skeleton = storage->skeleton_owner.get(ci->skeleton);
			if (!skeleton->use_2d) {
				skeleton = NULL;
			} else {
				state.skeleton_transform = r_ris.item_group_base_transform * skeleton->base_transform_2d;
				state.skeleton_transform_inverse = state.skeleton_transform.affine_inverse();
				state.skeleton_texture_size = Vector2(skeleton->size * 2, 0);
			}
		}

		bool use_skeleton = skeleton != NULL;
		if (r_ris.prev_use_skeleton != use_skeleton) {
			r_ris.rebind_shader = true;
			//state.canvas_shader->set_conditional(CanvasShaderGLES2::USE_SKELETON, use_skeleton);
			r_ris.prev_use_skeleton = use_skeleton;
		}

		if (skeleton) {
			state.specialization |= CanvasShaderGLES2::USE_SKELETON;
			glActiveTexture(GL_TEXTURE0 + storage->config.max_texture_image_units - 3);
			glBindTexture(GL_TEXTURE_2D, skeleton->tex_id);
			state.using_skeleton = true;
		} else {
			state.specialization &= ~CanvasShaderGLES2::USE_SHADOWS;
			state.using_skeleton = false;
		}

	} // if not using batching

	Item *material_owner = ci->material_owner ? ci->material_owner : ci;

	RID material = material_owner->material;
	RasterizerStorageGLES2::Material *material_ptr = storage->material_owner.getornull(material);

	if (material != r_ris.canvas_last_material || r_ris.rebind_shader) {
		RasterizerStorageGLES2::Shader *shader_ptr = NULL;

		if (material_ptr) {
			shader_ptr = material_ptr->shader;

			if (shader_ptr && shader_ptr->mode != RS::SHADER_CANVAS_ITEM) {
				shader_ptr = NULL; // not a canvas item shader, don't use.
			}
		}

		if (shader_ptr) {
			if (shader_ptr->canvas_item.uses_screen_texture) {
				if (!state.canvas_texscreen_used) {
					//copy if not copied before
					_copy_texscreen(Rect2());

					// blend mode will have been enabled so make sure we disable it again later on
					//last_blend_mode = last_blend_mode != RasterizerStorageGLES2::Shader::CanvasItem::BLEND_MODE_DISABLED ? last_blend_mode : -1;
				}

				if (storage->frame.current_rt->copy_screen_effect.color) {
					glActiveTexture(GL_TEXTURE0 + storage->config.max_texture_image_units - 4);
					glBindTexture(GL_TEXTURE_2D, storage->frame.current_rt->copy_screen_effect.color);
				}
			}

			if (shader_ptr != r_ris.shader_cache) {
				if (shader_ptr->canvas_item.uses_time) {
					VisualServerRaster::redraw_request();
				}

				state.canvas_shader->set_custom_shader(shader_ptr->custom_code_id);
				state.canvas_shader->bind();
			}

			// bind all the textures
			// in 4.x we can probably just use material_data->bind_uniforms()
			int tc = material_ptr->textures.size();
			Pair<StringName, RID> *textures = material_ptr->textures.ptrw();

			ShaderLanguage::ShaderNode::Uniform::Hint *texture_hints = shader_ptr->texture_hints.ptrw();

			for (int i = 0; i < tc; i++) {
				glActiveTexture(GL_TEXTURE0 + i);

				RasterizerStorageGLES2::Texture *t = storage->texture_owner.getornull(textures[i].second);

				if (!t) {
					switch (texture_hints[i]) {
						case ShaderLanguage::ShaderNode::Uniform::HINT_BLACK_ALBEDO:
						case ShaderLanguage::ShaderNode::Uniform::HINT_BLACK: {
							glBindTexture(GL_TEXTURE_2D, storage->resources.black_tex);
						} break;
						case ShaderLanguage::ShaderNode::Uniform::HINT_ANISO: {
							glBindTexture(GL_TEXTURE_2D, storage->resources.aniso_tex);
						} break;
						case ShaderLanguage::ShaderNode::Uniform::HINT_NORMAL: {
							glBindTexture(GL_TEXTURE_2D, storage->resources.normal_tex);
						} break;
						default: {
							glBindTexture(GL_TEXTURE_2D, storage->resources.white_tex);
						} break;
					}

					continue;
				}

				if (t->redraw_if_visible) {
					VisualServerRaster::redraw_request();
				}

				t = t->get_ptr();

#ifdef TOOLS_ENABLED
				if (t->detect_normal && texture_hints[i] == ShaderLanguage::ShaderNode::Uniform::HINT_NORMAL) {
					t->detect_normal(t->detect_normal_ud);
				}
#endif
				if (t->render_target)
					t->render_target->used_in_frame = true;

				glBindTexture(t->target, t->tex_id);
			}

		} else {
			state.canvas_shader->set_custom_shader(0);
			state.canvas_shader->bind();
		}
		state.canvas_shader->use_material((void *)material_ptr);

		r_ris.shader_cache = shader_ptr;

		r_ris.canvas_last_material = material;

		r_ris.rebind_shader = false;
	}

	int blend_mode = r_ris.shader_cache ? r_ris.shader_cache->canvas_item.blend_mode : RasterizerStorageGLES2::Shader::CanvasItem::BLEND_MODE_MIX;
	bool unshaded = r_ris.shader_cache && (r_ris.shader_cache->canvas_item.light_mode == RasterizerStorageGLES2::Shader::CanvasItem::LIGHT_MODE_UNSHADED || (blend_mode != RasterizerStorageGLES2::Shader::CanvasItem::BLEND_MODE_MIX && blend_mode != RasterizerStorageGLES2::Shader::CanvasItem::BLEND_MODE_PMALPHA));
	bool reclip = false;

	if (r_ris.last_blend_mode != blend_mode) {
		switch (blend_mode) {
			case RasterizerStorageGLES2::Shader::CanvasItem::BLEND_MODE_MIX: {
				glBlendEquation(GL_FUNC_ADD);
				if (storage->frame.current_rt && storage->frame.current_rt->flags[RendererStorage::RENDER_TARGET_TRANSPARENT]) {
					glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
				} else {
					glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE);
				}

			} break;
			case RasterizerStorageGLES2::Shader::CanvasItem::BLEND_MODE_ADD: {
				glBlendEquation(GL_FUNC_ADD);
				if (storage->frame.current_rt && storage->frame.current_rt->flags[RendererStorage::RENDER_TARGET_TRANSPARENT]) {
					glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE, GL_SRC_ALPHA, GL_ONE);
				} else {
					glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE, GL_ZERO, GL_ONE);
				}

			} break;
			case RasterizerStorageGLES2::Shader::CanvasItem::BLEND_MODE_SUB: {
				glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
				if (storage->frame.current_rt && storage->frame.current_rt->flags[RendererStorage::RENDER_TARGET_TRANSPARENT]) {
					glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE, GL_SRC_ALPHA, GL_ONE);
				} else {
					glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE, GL_ZERO, GL_ONE);
				}
			} break;
			case RasterizerStorageGLES2::Shader::CanvasItem::BLEND_MODE_MUL: {
				glBlendEquation(GL_FUNC_ADD);
				if (storage->frame.current_rt && storage->frame.current_rt->flags[RendererStorage::RENDER_TARGET_TRANSPARENT]) {
					glBlendFuncSeparate(GL_DST_COLOR, GL_ZERO, GL_DST_ALPHA, GL_ZERO);
				} else {
					glBlendFuncSeparate(GL_DST_COLOR, GL_ZERO, GL_ZERO, GL_ONE);
				}
			} break;
			case RasterizerStorageGLES2::Shader::CanvasItem::BLEND_MODE_PMALPHA: {
				glBlendEquation(GL_FUNC_ADD);
				if (storage->frame.current_rt && storage->frame.current_rt->flags[RendererStorage::RENDER_TARGET_TRANSPARENT]) {
					glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
				} else {
					glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE);
				}
			} break;
		}
	}

	// using software transform?
	// (i.e. don't send the transform matrix, send identity, and either use baked verts,
	// or large fvf where the transform is done in the shader from transform stored in the fvf.)
	if (!p_bij.use_hardware_transform()) {
		state.uniforms.modelview_matrix = Transform2D();
		// final_modulate will be baked per item ref so the final_modulate can be an identity color
		state.uniforms.final_modulate = Color(1, 1, 1, 1);
	} else {
		state.uniforms.modelview_matrix = ci->final_transform;
		// could use the stored version of final_modulate in item ref? Test which is faster NYI
		state.uniforms.final_modulate = unshaded ? ci->final_modulate : (ci->final_modulate * r_ris.item_group_modulate);
	}
	state.uniforms.extra_matrix = Transform2D();

	_set_canvas_uniforms();

	if (unshaded || (state.uniforms.final_modulate.a > 0.001 && (!r_ris.shader_cache || r_ris.shader_cache->canvas_item.light_mode != RasterizerStorageGLES2::Shader::CanvasItem::LIGHT_MODE_LIGHT_ONLY) && !ci->light_masked))
		render_joined_item_commands(p_bij, NULL, reclip, material_ptr, false);

	r_ris.rebind_shader = true; // hacked in for now.

	if ((blend_mode == RasterizerStorageGLES2::Shader::CanvasItem::BLEND_MODE_MIX || blend_mode == RasterizerStorageGLES2::Shader::CanvasItem::BLEND_MODE_PMALPHA) && r_ris.item_group_light && !unshaded) {
		Light *light = r_ris.item_group_light;
		bool light_used = false;
		VS::CanvasLightMode mode = RS::CANVAS_LIGHT_MODE_ADD;

		// we leave this set to 1, 1, 1, 1 if using software because the colors are baked into the vertices
		if (p_bij.use_hardware_transform()) {
			state.uniforms.final_modulate = ci->final_modulate; // remove the canvas modulate
		}

		while (light) {
			// use the bounding rect of the joined items, NOT only the bounding rect of the first item.
			// note this is a cost of batching, the light culling will be less effective

			// note that the r_ris.item_group_z will be out of date because we are using deferred rendering till canvas_render_items_end()
			// so we have to test z against the stored value in the joined item
			if (ci->light_mask & light->item_mask && p_bij.z_index >= light->z_min && p_bij.z_index <= light->z_max && p_bij.bounding_rect.intersects_transformed(light->xform_cache, light->rect_cache)) {
				//intersects this light

				if (!light_used || mode != light->mode) {
					mode = light->mode;

					switch (mode) {
						case RS::CANVAS_LIGHT_MODE_ADD: {
							glBlendEquation(GL_FUNC_ADD);
							glBlendFunc(GL_SRC_ALPHA, GL_ONE);

						} break;
						case RS::CANVAS_LIGHT_MODE_SUB: {
							glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
							glBlendFunc(GL_SRC_ALPHA, GL_ONE);
						} break;
						case RS::CANVAS_LIGHT_MODE_MIX:
						case RS::CANVAS_LIGHT_MODE_MASK: {
							glBlendEquation(GL_FUNC_ADD);
							glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

						} break;
					}
				}

				if (!light_used) {
					//state.canvas_shader->set_conditional(CanvasShaderGLES2::USE_LIGHTING, true);
					state.specialization |= CanvasShaderGLES2::USE_LIGHTING;
					light_used = true;
				}

				bool has_shadow = light->shadow_buffer.is_valid() && ci->light_mask & light->item_shadow_mask;

				//state.canvas_shader->set_conditional(CanvasShaderGLES2::USE_SHADOWS, has_shadow);
				if (has_shadow) {
					state.specialization |= CanvasShaderGLES2::USE_SHADOWS;
					_set_shadow_filer(light->shadow_filter);
				} else {
					state.specialization &= ~CanvasShaderGLES2::USE_SHADOWS;
					_set_shadow_filer(RS::CanvasLightShadowFilter::CANVAS_LIGHT_FILTER_MAX);
				}

				state.canvas_shader->bind();
				state.using_light = light;
				state.using_shadow = has_shadow;

				//always re-set uniforms, since light parameters changed
				_set_canvas_uniforms();
				state.canvas_shader->use_material((void *)material_ptr);

				glActiveTexture(GL_TEXTURE0 + storage->config.max_texture_image_units - 6);
				RasterizerStorageGLES2::Texture *t = storage->texture_owner.getornull(light->texture);
				if (!t) {
					glBindTexture(GL_TEXTURE_2D, storage->resources.white_tex);
				} else {
					t = t->get_ptr();

					glBindTexture(t->target, t->tex_id);
				}

				glActiveTexture(GL_TEXTURE0);

				// redraw using light.
				// if there is no clip item, we can consider scissoring to the intersection area between the light and the item
				// this can greatly reduce fill rate ..
				// at the cost of glScissor commands, so is optional
				if (!bdata.settings_scissor_lights || r_ris.current_clip) {
					render_joined_item_commands(p_bij, NULL, reclip, material_ptr, true);
				} else {
					bool scissor = _light_scissor_begin(p_bij.bounding_rect, light->xform_cache, light->rect_cache, rendertarget_height);
					render_joined_item_commands(p_bij, NULL, reclip, material_ptr, true);
					if (scissor) {
						glDisable(GL_SCISSOR_TEST);
					}
				}

				state.using_light = NULL;
			}

			light = light->next_ptr;
		}

		if (light_used) {
			//state.canvas_shader->set_conditional(CanvasShaderGLES2::USE_LIGHTING, false);
			state.specialization &= ~CanvasShaderGLES2::USE_SHADOWS;
			//state.canvas_shader->set_conditional(CanvasShaderGLES2::USE_SHADOWS, false);
			state.specialization &= ~CanvasShaderGLES2::USE_SHADOWS;
			_set_shadow_filer(RS::CanvasLightShadowFilter::CANVAS_LIGHT_FILTER_MAX);

			state.canvas_shader->bind();

			r_ris.last_blend_mode = -1;

#if 0
			//this is set again, so it should not be needed anyway?
			state.canvas_item_modulate = unshaded ? ci->final_modulate : Color(
						ci->final_modulate.r * p_modulate.r,
						ci->final_modulate.g * p_modulate.g,
						ci->final_modulate.b * p_modulate.b,
						ci->final_modulate.a * p_modulate.a );

			state.canvas_shader->set_uniform(CanvasShaderGLES2::MODELVIEW_MATRIX,state.final_transform);
			state.canvas_shader->set_uniform(CanvasShaderGLES2::EXTRA_MATRIX,Transform2D());
			state.canvas_shader->set_uniform(CanvasShaderGLES2::FINAL_MODULATE,state.canvas_item_modulate);

			glBlendEquation(GL_FUNC_ADD);

			if (storage->frame.current_rt->flags[RendererStorage::RENDER_TARGET_TRANSPARENT]) {
				glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
			} else {
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			}

			//@TODO RESET canvas_blend_mode
#endif
		}
	}

	if (reclip) {
		glEnable(GL_SCISSOR_TEST);
		int y = storage->frame.current_rt->height - (r_ris.current_clip->final_clip_rect.position.y + r_ris.current_clip->final_clip_rect.size.y);
		if (storage->frame.current_rt->flags[RendererStorage::RENDER_TARGET_VFLIP])
			y = r_ris.current_clip->final_clip_rect.position.y;
		glScissor(r_ris.current_clip->final_clip_rect.position.x, y, r_ris.current_clip->final_clip_rect.size.width, r_ris.current_clip->final_clip_rect.size.height);
	}
}
#endif // def GODOT 3

void RasterizerCanvasGLES2::gl_enable_scissor(int p_x, int p_y, int p_width, int p_height) const {
	glEnable(GL_SCISSOR_TEST);
	glScissor(p_x, p_y, p_width, p_height);
}

void RasterizerCanvasGLES2::gl_disable_scissor() const {
	glDisable(GL_SCISSOR_TEST);
}

void RasterizerCanvasGLES2::initialize() {
	print_line("RasterizerCanvasGLES2::initialize");
	//RasterizerCanvasBaseGLES2::initialize();
	// quad buffer
	{
		glGenBuffers(1, &data.canvas_quad_vertices);
		glBindBuffer(GL_ARRAY_BUFFER, data.canvas_quad_vertices);

		const float qv[8] = {
			0, 0,
			0, 1,
			1, 1,
			1, 0
		};

		glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 8, qv, GL_STATIC_DRAW);

		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	// polygon buffer
	{
		//TODO: check what this is called in other renderers
		uint32_t poly_size = GLOBAL_DEF("rendering/limits/buffers/canvas_polygon_buffer_size_kb", 128);
		//ProjectSettings::get_singleton()->set_custom_property_info("rendering/limits/buffers/canvas_polygon_buffer_size_kb", PropertyInfo(Variant::INT, "rendering/limits/buffers/canvas_polygon_buffer_size_kb", PROPERTY_HINT_RANGE, "0,256,1,or_greater"));
		poly_size = MAX(poly_size, (uint32_t)128); // minimum 2k, may still see anomalies in editor
		poly_size *= 1024;
		glGenBuffers(1, &data.polygon_buffer);
		glBindBuffer(GL_ARRAY_BUFFER, data.polygon_buffer);
		glBufferData(GL_ARRAY_BUFFER, poly_size, NULL, GL_DYNAMIC_DRAW);

		data.polygon_buffer_size = poly_size;

		glBindBuffer(GL_ARRAY_BUFFER, 0);

		//TODO: check what this is called in other renderers
		uint32_t index_size = GLOBAL_DEF("rendering/limits/buffers/canvas_polygon_index_buffer_size_kb", 128);
		//ProjectSettings::get_singleton()->set_custom_property_info("rendering/limits/buffers/canvas_polygon_index_buffer_size_kb", PropertyInfo(Variant::INT, "rendering/limits/buffers/canvas_polygon_index_buffer_size_kb", PROPERTY_HINT_RANGE, "0,256,1,or_greater"));
		index_size = MAX(index_size, (uint32_t)128);
		index_size *= 1024; // kb
		glGenBuffers(1, &data.polygon_index_buffer);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, data.polygon_index_buffer);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, index_size, NULL, GL_DYNAMIC_DRAW);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

		data.polygon_index_buffer_size = index_size;
	}

	// ninepatch buffers
	{
		// array buffer
		glGenBuffers(1, &data.ninepatch_vertices);
		glBindBuffer(GL_ARRAY_BUFFER, data.ninepatch_vertices);

		glBufferData(GL_ARRAY_BUFFER, sizeof(float) * (16 + 16) * 2, NULL, GL_DYNAMIC_DRAW);

		glBindBuffer(GL_ARRAY_BUFFER, 0);

		// element buffer
		glGenBuffers(1, &data.ninepatch_elements);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, data.ninepatch_elements);

#define _EIDX(y, x) (y * 4 + x)
		uint8_t elems[3 * 2 * 9] = {
			// first row

			_EIDX(0, 0), _EIDX(0, 1), _EIDX(1, 1),
			_EIDX(1, 1), _EIDX(1, 0), _EIDX(0, 0),

			_EIDX(0, 1), _EIDX(0, 2), _EIDX(1, 2),
			_EIDX(1, 2), _EIDX(1, 1), _EIDX(0, 1),

			_EIDX(0, 2), _EIDX(0, 3), _EIDX(1, 3),
			_EIDX(1, 3), _EIDX(1, 2), _EIDX(0, 2),

			// second row

			_EIDX(1, 0), _EIDX(1, 1), _EIDX(2, 1),
			_EIDX(2, 1), _EIDX(2, 0), _EIDX(1, 0),

			// the center one would be here, but we'll put it at the end
			// so it's easier to disable the center and be able to use
			// one draw call for both

			_EIDX(1, 2), _EIDX(1, 3), _EIDX(2, 3),
			_EIDX(2, 3), _EIDX(2, 2), _EIDX(1, 2),

			// third row

			_EIDX(2, 0), _EIDX(2, 1), _EIDX(3, 1),
			_EIDX(3, 1), _EIDX(3, 0), _EIDX(2, 0),

			_EIDX(2, 1), _EIDX(2, 2), _EIDX(3, 2),
			_EIDX(3, 2), _EIDX(3, 1), _EIDX(2, 1),

			_EIDX(2, 2), _EIDX(2, 3), _EIDX(3, 3),
			_EIDX(3, 3), _EIDX(3, 2), _EIDX(2, 2),

			// center field

			_EIDX(1, 1), _EIDX(1, 2), _EIDX(2, 2),
			_EIDX(2, 2), _EIDX(2, 1), _EIDX(1, 1)
		};
#undef _EIDX

		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(elems), elems, GL_STATIC_DRAW);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	}

	//state.canvas_shadow_shader.init();

	state.canvas_shader->initialize();

	_set_texture_rect_mode(true);
	//state.canvas_shader->set.set_conditional(CanvasShaderGLES2::USE_RGBA_SHADOWS, storage->config.use_rgba_2d_shadows);

	//state.canvas_shader->bind();

	//state.lens_shader.init();

	//state.canvas_shader.set_conditional(CanvasShaderGLES2::USE_PIXEL_SNAP, GLOBAL_DEF("rendering/quality/2d/use_pixel_snap", false));

	state.using_light = NULL;
	state.using_skeleton = false;

	batch_initialize();

	// just reserve some space (may not be needed as we are orphaning, but hey ho)
	glGenBuffers(1, &bdata.gl_vertex_buffer);

	if (bdata.vertex_buffer_size_bytes) {
		glBindBuffer(GL_ARRAY_BUFFER, bdata.gl_vertex_buffer);
		glBufferData(GL_ARRAY_BUFFER, bdata.vertex_buffer_size_bytes, NULL, GL_DYNAMIC_DRAW);
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		// pre fill index buffer, the indices never need to change so can be static
		glGenBuffers(1, &bdata.gl_index_buffer);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bdata.gl_index_buffer);

		Vector<uint16_t> indices;
		indices.resize(bdata.index_buffer_size_units);

		for (unsigned int q = 0; q < bdata.max_quads; q++) {
			int i_pos = q * 6; //  6 inds per quad
			int q_pos = q * 4; // 4 verts per quad
			indices.set(i_pos, q_pos);
			indices.set(i_pos + 1, q_pos + 1);
			indices.set(i_pos + 2, q_pos + 2);
			indices.set(i_pos + 3, q_pos);
			indices.set(i_pos + 4, q_pos + 2);
			indices.set(i_pos + 5, q_pos + 3);

			// we can only use 16 bit indices in GLES2!
#ifdef DEBUG_ENABLED
			CRASH_COND((q_pos + 3) > 65535);
#endif
		}

		glBufferData(GL_ELEMENT_ARRAY_BUFFER, bdata.index_buffer_size_bytes, &indices[0], GL_STATIC_DRAW);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	} // only if there is a vertex buffer (batching is on)
}

RendererCanvasRender::PolygonID RasterizerCanvasGLES2::request_polygon(const Vector<int> &p_indices, const Vector<Point2> &p_points, const Vector<Color> &p_colors, const Vector<Point2> &p_uvs, const Vector<int> &p_bones, const Vector<float> &p_weights) {
	uint32_t id = _polydata.alloc();
	PolyData &pd = _polydata[id];
	pd.indices = p_indices;
	pd.points = p_points;
	pd.colors = p_colors;
	pd.uvs = p_uvs;
	return id;
}
void RasterizerCanvasGLES2::free_polygon(PolygonID p_polygon) {
	_polydata.free(p_polygon);
}

RasterizerCanvasGLES2::RasterizerCanvasGLES2() {
	material_storage = GLES::MaterialStorage::get_singleton();
	texture_storage = GLES::TextureStorage::get_singleton();
	config = GLES::Config::get_singleton();

	String global_defines;
	global_defines += "#define MAX_GLOBAL_SHADER_UNIFORMS 256\n"; // TODO: this is arbitrary for now
	//global_defines += "#define MAX_LIGHTS " + itos(data.max_lights_per_render) + "\n";

	state.canvas_shader = static_cast<CanvasShaderGLES2 *>(material_storage->shaders.canvas_shader);
	state.canvas_shader->initialize(global_defines, 1);
	data.canvas_shader_default_version = state.canvas_shader->version_create();

	default_canvas_texture = texture_storage->canvas_texture_allocate();
	texture_storage->canvas_texture_initialize(default_canvas_texture);

	batch_constructor();
}

RasterizerCanvasGLES2::~RasterizerCanvasGLES2() {
	state.canvas_shader->version_free(data.canvas_shader_default_version);
	texture_storage->canvas_texture_free(default_canvas_texture);

	glDeleteBuffers(1, &data.canvas_quad_vertices);
	glDeleteBuffers(1, &data.ninepatch_vertices);
	glDeleteBuffers(1, &data.ninepatch_vertices);
	glDeleteBuffers(1, &data.polygon_buffer);
	glDeleteBuffers(1, &data.polygon_index_buffer);
}

#endif // GLES2_ENABLED