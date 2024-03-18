/**************************************************************************/
/*  rasterizer_canvas_gles2.h                                              */
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

#ifndef RASTERIZER_CANVAS_GLES2_H
#define RASTERIZER_CANVAS_GLES2_H

#if defined(GLES2_ENABLED)

#include "batcher/rasterizer_canvas_batcher.h"
#include "core/os/os.h"
#include "drivers/gles_common/rasterizer_canvas_gles.h"
#include "shaders/canvas.glsl.gen.h"

class RasterizerSceneGLES2;

class RasterizerCanvasGLES2 : public RasterizerCanvasGLES, public RasterizerCanvasBatcher {
	friend class RasterizerCanvasBatcher;

private:
	GLES::TextureStorage *texture_storage;
	GLES::MaterialStorage *material_storage;
	GLES::Config *config;

protected:
	struct Uniforms {
		Transform3D projection_matrix;

		Transform2D modelview_matrix;
		Transform2D extra_matrix;

		Color final_modulate;

		float time;
	};

	struct Data {
		GLuint canvas_quad_vertices;

		GLuint polygon_buffer;
		GLuint polygon_index_buffer;

		uint32_t polygon_buffer_size;
		uint32_t polygon_index_buffer_size;

		GLuint ninepatch_vertices;
		GLuint ninepatch_elements;

		RID canvas_shader_default_version;
	} data;

	struct PolyData {
		LocalVector<int> indices;
		LocalVector<Point2> points;
		LocalVector<Color> colors;
		LocalVector<Point2> uvs;
	};
	RasterizerPooledIndirectList<PolyData> _polydata;

	struct State {
		Uniforms uniforms;
		bool canvas_texscreen_used;
		CanvasShaderGLES2 *canvas_shader;
		//CanvasShadowShaderGLES2 canvas_shadow_shader;
		//LensDistortedShaderGLES2 lens_shader;

		RID shader_version;
		uint64_t specialization;
		CanvasShaderGLES2::ShaderVariant mode_variant = CanvasShaderGLES2::ShaderVariant::MODE_QUAD;

		bool using_ninepatch;
		bool using_skeleton;

		Transform2D skeleton_transform;
		Transform2D skeleton_transform_inverse;
		Size2i skeleton_texture_size;

		// primarily used and updated when binding textures with _bind_canvas_texture
		RID current_tex = RID();
		bool current_tex_is_rt = false;
		RS::CanvasItemTextureFilter current_filter_mode = RS::CANVAS_ITEM_TEXTURE_FILTER_MAX;
		RS::CanvasItemTextureRepeat current_repeat_mode = RS::CANVAS_ITEM_TEXTURE_REPEAT_MAX;
		Size2 texpixel_size = Size2(1.0, 1.0);
		Size2i texture_size = Size2i(1, 1); //needed for ninepatch
		bool normal_used = false;

		Transform3D vp;
		Light *using_light = nullptr;
		bool using_shadow;

		RID render_target;

		// new for Godot 4.0
		// min mag filter is per item, and repeat
		RS::CanvasItemTextureFilter default_filter = RS::CANVAS_ITEM_TEXTURE_FILTER_NEAREST;
		RS::CanvasItemTextureRepeat default_repeat = RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED;
		RS::CanvasItemTextureFilter current_filter = RS::CANVAS_ITEM_TEXTURE_FILTER_MAX;
		RS::CanvasItemTextureRepeat current_repeat = RS::CANVAS_ITEM_TEXTURE_REPEAT_MAX;
	} state;

	RID default_canvas_texture;

	void _set_shadow_filer(RS::CanvasLightShadowFilter p_filter);
	void _set_texture_rect_mode(bool p_texture_rect, bool p_light_angle = false, bool p_modulate = false, bool p_large_vertex = false);
	void _bind_canvas_texture(RID p_texture, RS::CanvasItemTextureFilter p_base_filter, RS::CanvasItemTextureRepeat p_base_repeat);

	void _set_canvas_uniforms();

	void _bind_quad_buffer();
	inline void _buffer_orphan_and_upload(unsigned int p_buffer_size_bytes, unsigned int p_offset_bytes, unsigned int p_data_size_bytes, const void *p_data, GLenum p_target, GLenum p_usage, bool p_optional_orphan) const;

	void _legacy_draw_primitive(Item::CommandPrimitive *p_pr, GLES::Material *p_material);
	void _legacy_draw_line(Item::CommandPrimitive *p_pr, GLES::Material *p_material);
	void _legacy_draw_poly_triangles(Item::CommandPolygon *p_poly, GLES::Material *p_material);

	void _draw_gui_primitive(int p_points, const Vector2 *p_vertices, const Color *p_colors, const Vector2 *p_uvs, const float *p_light_angles = nullptr);
	void _draw_polygon(const int *p_indices, int p_index_count, int p_vertex_count, const Vector2 *p_vertices, const Vector2 *p_uvs, const Color *p_colors, bool p_singlecolor, const float *p_weights = nullptr, const int *p_bones = nullptr);

	void _copy_texscreen(const Rect2 &p_rect);
	void _copy_screen(const Rect2 &p_rect);

public:
	virtual void set_time(double p_time) override {
		state.uniforms.time = p_time;
	}

	void reset_canvas();

	virtual void canvas_render_items_begin(const Color &p_modulate, Light *p_light, const Transform2D &p_base_transform);
	virtual void canvas_render_items_end();
	void canvas_render_items_internal(Item *p_item_list, int p_z, const Color &p_modulate, Light *p_light, const Transform2D &p_base_transform);
	virtual void canvas_begin(RID p_to_render_target, bool p_to_backbuffer);
	virtual void canvas_end();

	void canvas_render_items(RID p_to_render_target, Item *p_item_list, const Color &p_modulate, Light *p_light_list, Light *p_directional_list, const Transform2D &p_canvas_transform, RS::CanvasItemTextureFilter p_default_filter, RS::CanvasItemTextureRepeat p_default_repeat, bool p_snap_2d_vertices_to_pixel, bool &r_sdf_used, RenderingMethod::RenderInfo *r_render_info = nullptr) override {
		state.default_filter = p_default_filter;
		state.default_repeat = p_default_repeat;

		// first set the current render target
		GLES::RenderTarget *rt = texture_storage->get_render_target(p_to_render_target);

		if (rt) {
			state.render_target = p_to_render_target;
			ERR_FAIL_COND(!rt);
			//frame.clear_request = false;

			glViewport(0, 0, rt->size.width, rt->size.height);

			//		print_line("_set_current_render_target w " + itos(rt->width) + " h " + itos(rt->height));
			/*
						_dims.rt_width = state.render_target->size.width;
						_dims.rt_height = state.render_target->size.height;
						_dims.win_width = state.render_target->size.width;
						_dims.win_height = state.render_target->size.height;
			*/
		} else {
			state.render_target = RID();
			//frame.clear_request = false;
			// FTODO

			glViewport(0, 0, DisplayServer::get_singleton()->window_get_size().width, DisplayServer::get_singleton()->window_get_size().height);
			texture_storage->bind_framebuffer_system();
		}

		// binds the render target (framebuffer)
		bool backbuffer = false;
		canvas_begin(p_to_render_target, backbuffer);

		canvas_render_items_begin(p_modulate, p_light_list, p_canvas_transform);
		canvas_render_items_internal(p_item_list, 0, p_modulate, p_light_list, p_canvas_transform);
		canvas_render_items_end();

		canvas_end();

		// not sure why these are needed to get frame to render?
		// storage->_set_current_render_target(RID());
		//		storage->frame.current_rt = nullptr;
		//		canvas_begin();
		//		canvas_end();
	}

	RendererCanvasRender::PolygonID request_polygon(const Vector<int> &p_indices, const Vector<Point2> &p_points, const Vector<Color> &p_colors, const Vector<Point2> &p_uvs = Vector<Point2>(), const Vector<int> &p_bones = Vector<int>(), const Vector<float> &p_weights = Vector<float>()) override;
	void free_polygon(PolygonID p_polygon) override;

private:
	//TODO: remove once no longer needed!
	void _legacy_canvas_render_item(Item *p_ci, RenderItemState &r_ris);

	// high level batch funcs
	void canvas_render_items_implementation(Item *p_item_list, int p_z, const Color &p_modulate, Light *p_light, const Transform2D &p_base_transform);

#ifdef GODOT_3
	//void render_joined_item(const BItemJoined &p_bij, RenderItemState &r_ris);
	//bool try_join_item(Item *p_ci, RenderItemState &r_ris, bool &r_batch_break);
#endif
	void render_batches(Item::Command *const *p_commands, Item *p_current_clip, bool &r_reclip, GLES::Material *p_material);

	// low level batch funcs
	void _batch_upload_buffers();
	//	void _batch_render_generic(const Batch &p_batch, RasterizerStorageGLES2::Material *p_material);
	//	void _batch_render_lines(const Batch &p_batch, RasterizerStorageGLES2::Material *p_material, bool p_anti_alias);

	// funcs used from rasterizer_canvas_batcher template
	void gl_enable_scissor(int p_x, int p_y, int p_width, int p_height) const;
	void gl_disable_scissor() const;

public:
	void initialize();
	RasterizerCanvasGLES2();
	~RasterizerCanvasGLES2();

	/* Implement light stuff in the future */
	RID light_create() override { return RID(); };
	void light_set_texture(RID p_rid, RID p_texture) override{};
	void light_set_use_shadow(RID p_rid, bool p_enable) override{};
	void light_update_shadow(RID p_rid, int p_shadow_index, const Transform2D &p_light_xform, int p_light_mask, float p_near, float p_far, LightOccluderInstance *p_occluders) override{};
	void light_update_directional_shadow(RID p_rid, int p_shadow_index, const Transform2D &p_light_xform, int p_light_mask, float p_cull_distance, const Rect2 &p_clip_rect, LightOccluderInstance *p_occluders) override{};

	void render_sdf(RID p_render_target, LightOccluderInstance *p_occluders) override{};
	RID occluder_polygon_create() override { return RID(); };
	void occluder_polygon_set_shape(RID p_occluder, const Vector<Vector2> &p_points, bool p_closed) override{};
	void occluder_polygon_set_cull_mode(RID p_occluder, RS::CanvasOccluderPolygonCullMode p_mode) override{};
	void set_shadow_texture_size(int p_size) override{};

	bool free(RID p_rid) override {
		//destroy any lights and occluders we create above here
		return false;
	};
	void update() override{};
	virtual void set_debug_redraw(bool p_enabled, double p_time, const Color &p_color) override {
		if (p_enabled) {
			WARN_PRINT_ONCE("Debug CanvasItem Redraw is not available yet when using the GL Compatibility backend.");
		}
	}
};

#endif // GLES2_ENABLED
#endif // RASTERIZER_CANVAS_GLES2_H