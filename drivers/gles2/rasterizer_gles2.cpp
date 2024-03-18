#ifdef GLES2_ENABLED

#include "rasterizer_gles2.h"
#include "drivers/gles2/effects/copy_effects_gles2.h"
#include "rasterizer_canvas_gles2.h"
#include "storage/config.h"
#include "storage/material_storage.h"
#include "storage/texture_storage_gles2.h"

RasterizerGLES2::RasterizerGLES2() {
	// OpenGL needs to be initialized before initializing the Rasterizers
	config = memnew(GLES::ConfigGLES2);
	utilities = memnew(GLES::Utilities);
	texture_storage = memnew(GLES::TextureStorageGLES2);
	texture_storage->initialize(); // we need virtual methods during init
	material_storage = memnew(GLES::MaterialStorageGLES2);
	mesh_storage = memnew(GLES::MeshStorage);
	particles_storage = memnew(GLES::ParticlesStorage);
	light_storage = memnew(GLES::LightStorage);
	copy_effects = memnew(GLES::CopyEffectsGLES2);
	gi = memnew(GLES::GI);
	fog = memnew(GLES::Fog);

	canvas = memnew(RasterizerCanvasGLES2);
	static_cast<RasterizerCanvasGLES2 *>(canvas)->initialize();
	scene = memnew(RasterizerSceneGLES());
}

void RasterizerGLES2::_blit_render_target_to_screen(RID p_render_target, DisplayServer::WindowID p_screen, const Rect2 &p_screen_rect, uint32_t p_layer, bool p_first) {
	GLES::RenderTarget *rt = texture_storage->get_render_target(p_render_target);

	ERR_FAIL_COND(!rt);

	//TODO: the GLES_OVER_GL define no longer exists, remove it
	glBindFramebuffer(GL_FRAMEBUFFER, texture_storage->system_fbo);

	glDisable(GL_BLEND);
	glDepthMask(GL_FALSE);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	glActiveTexture(GL_TEXTURE0);
	/*
		GLES::Texture *tex = texture_storage->get_texture(rt->texture);
		if (tex->is_external) {
			glBindTexture(GL_TEXTURE_2D, tex->tex_id);
		} else {
			glBindTexture(GL_TEXTURE_2D, tex->tex_id);
		}
	*/
	glBindTexture(GL_TEXTURE_2D, rt->color);

	//make sure we don't blur the image when the RT is smaller than the screen
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	copy_effects->copy_screen();

	glBindTexture(GL_TEXTURE_2D, 0);

	/* old GLES2 code:

		canvas->_set_texture_rect_mode(true);

		canvas->state.canvas_shader.set_custom_shader(0);
		canvas->state.canvas_shader.set_conditional(CanvasShaderGLES2::LINEAR_TO_SRGB, rt->flags[RasterizerStorage::RENDER_TARGET_KEEP_3D_LINEAR]);
		canvas->state.canvas_shader.bind();

		canvas->canvas_begin();
		glDisable(GL_BLEND);
		glBindFramebuffer(GL_FRAMEBUFFER, texture_storage->system_fbo);
		//WRAPPED_GL_ACTIVE_TEXTURE(GL_TEXTURE0 + storage->config.max_texture_image_units - 1);
		glActiveTexture(GL_TEXTURE0);

		GLES::Texture *tex = texture_storage->get_texture(rt->texture);
		if (tex->is_external) {
			//TODO: add this should we ever wish to support rendering to an external texture
			glBindTexture(GL_TEXTURE_2D, tex->tex_id);
		} else {
			glBindTexture(GL_TEXTURE_2D, tex->tex_id);
		}

		// TODO normals

		canvas->draw_generic_textured_rect(p_screen_rect, Rect2(0, 0, 1, -1));

		glBindTexture(GL_TEXTURE_2D, 0);
		canvas->canvas_end();

		canvas->state.canvas_shader.set_conditional(CanvasShaderGLES2::LINEAR_TO_SRGB, false);
		*/
}
#endif