/**************************************************************************/
/*  rasterizer_gles2.h                                                    */
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

#ifndef RASTERIZER_GLES2_H
#define RASTERIZER_GLES2_H

#ifdef GLES2_ENABLED

#include "effects/copy_effects.h"
#include "effects/cubemap_filter.h"
#include "effects/glow.h"
#include "effects/post_effects.h"
#include "environment/fog.h"
#include "environment/gi.h"
#include "rasterizer_canvas_gles2.h"
#include "rasterizer_scene_gles2.h"
#include "servers/rendering/renderer_compositor.h"
#include "storage/config.h"
#include "storage/light_storage.h"
#include "storage/material_storage.h"
#include "storage/mesh_storage.h"
#include "storage/particles_storage.h"
#include "storage/texture_storage.h"
#include "storage/utilities.h"

class RasterizerGLES2 : public RendererCompositor {
private:
	uint64_t frame = 1;
	float delta = 0;

	double time_total = 0.0;
	bool flip_xy_workaround = false;

	static bool gles_over_gl;

protected:
	GLES2::Config *config = nullptr;
	GLES2::Utilities *utilities = nullptr;
	GLES2::TextureStorage *texture_storage = nullptr;
	GLES2::MaterialStorage *material_storage = nullptr;
	GLES2::MeshStorage *mesh_storage = nullptr;
	GLES2::ParticlesStorage *particles_storage = nullptr;
	GLES2::LightStorage *light_storage = nullptr;
	GLES2::GI *gi = nullptr;
	GLES2::Fog *fog = nullptr;
	GLES2::CopyEffects *copy_effects = nullptr;
	GLES2::CubemapFilter *cubemap_filter = nullptr;
	GLES2::Glow *glow = nullptr;
	GLES2::PostEffects *post_effects = nullptr;
	// GLES2::RasterizerCanvasGLES2 *canvas = nullptr;
	// GLES2::RasterizerSceneGLES2 *scene = nullptr;
	GLES2::RendererCanvasRender *canvas = nullptr;
	GLES2::RendererSceneRender *scene = nullptr;
	static RasterizerGLES2 *singleton;

	void _blit_render_target_to_screen(RID p_render_target, DisplayServer::WindowID p_screen, const Rect2 &p_screen_rect, uint32_t p_layer, bool p_first = true);

public:
	RendererUtilities *get_utilities() { return utilities; }
	RendererLightStorage *get_light_storage() { return light_storage; }
	RendererMaterialStorage *get_material_storage() { return material_storage; }
	RendererMeshStorage *get_mesh_storage() { return mesh_storage; }
	RendererParticlesStorage *get_particles_storage() { return particles_storage; }
	RendererTextureStorage *get_texture_storage() { return texture_storage; }
	RendererGI *get_gi() { return gi; }
	RendererFog *get_fog() { return fog; }
	RendererCanvasRender *get_canvas() { return canvas; }
	RendererSceneRender *get_scene() { return scene; }

	void set_boot_image(const Ref<Image> &p_image, const Color &p_color, bool p_scale, bool p_use_filter = true);
	void set_shader_time_scale(float p_scale);

	void initialize();
	void begin_frame(double frame_step);
	void set_current_render_target(RID p_render_target);
	void restore_render_target(bool p_3d_was_drawn);
	void clear_render_target(const Color &p_color);
	void blit_render_target_to_screen(RID p_render_target, const Rect2 &p_screen_rect, int p_screen = 0);
	void output_lens_distorted_to_screen(RID p_render_target, const Rect2 &p_screen_rect, float p_k1, float p_k2, const Vector2 &p_eye_center, float p_oversample);
	void end_frame(bool p_swap_buffers);
	void gl_end_frame(bool p_swap_buffers);
	void finalize();

	static Error is_viable();
	static void register_config();

	static void make_current() {
		_create_func = _create_current;
		// No need to set low end, assume
		// it always it.
	}

	static RendererCompositor *_create_current() {
		return memnew(RasterizerGLES2);
	}

	static void clear_depth(float p_depth);
	static bool is_gles_over_gl() { return gles_over_gl; }
	static bool gl_check_errors();

	_ALWAYS_INLINE_ uint64_t get_frame_number() const { return frame; }
	_ALWAYS_INLINE_ double get_frame_delta_time() const { return delta; }
	_ALWAYS_INLINE_ double get_total_time() const { return time_total; }

	static RasterizerGLES2 *get_singleton() { return singleton; }

	RasterizerGLES2();
	~RasterizerGLES2();
};

#endif // GLES2_ENABLED

#endif // RASTERIZER_GLES2_H
