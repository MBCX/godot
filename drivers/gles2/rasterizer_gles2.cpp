#ifdef GLES2_ENABLED

#include "rasterizer_gles2.h"
#include "drivers/gles2/effects/copy_effects_gles2.h"
#include "rasterizer_canvas_gles2.h"
#include "storage/config.h"
#include "storage/material_storage.h"
#include "storage/texture_storage_gles2.h"
#include "core/io/dir_access.h"

#define _EXT_DEBUG_OUTPUT_SYNCHRONOUS_ARB 0x8242
#define _EXT_DEBUG_NEXT_LOGGED_MESSAGE_LENGTH_ARB 0x8243
#define _EXT_DEBUG_CALLBACK_FUNCTION_ARB 0x8244
#define _EXT_DEBUG_CALLBACK_USER_PARAM_ARB 0x8245
#define _EXT_DEBUG_SOURCE_API_ARB 0x8246
#define _EXT_DEBUG_SOURCE_WINDOW_SYSTEM_ARB 0x8247
#define _EXT_DEBUG_SOURCE_SHADER_COMPILER_ARB 0x8248
#define _EXT_DEBUG_SOURCE_THIRD_PARTY_ARB 0x8249
#define _EXT_DEBUG_SOURCE_APPLICATION_ARB 0x824A
#define _EXT_DEBUG_SOURCE_OTHER_ARB 0x824B
#define _EXT_DEBUG_TYPE_ERROR_ARB 0x824C
#define _EXT_DEBUG_TYPE_DEPRECATED_BEHAVIOR_ARB 0x824D
#define _EXT_DEBUG_TYPE_UNDEFINED_BEHAVIOR_ARB 0x824E
#define _EXT_DEBUG_TYPE_PORTABILITY_ARB 0x824F
#define _EXT_DEBUG_TYPE_PERFORMANCE_ARB 0x8250
#define _EXT_DEBUG_TYPE_OTHER_ARB 0x8251
#define _EXT_MAX_DEBUG_MESSAGE_LENGTH_ARB 0x9143
#define _EXT_MAX_DEBUG_LOGGED_MESSAGES_ARB 0x9144
#define _EXT_DEBUG_LOGGED_MESSAGES_ARB 0x9145
#define _EXT_DEBUG_SEVERITY_HIGH_ARB 0x9146
#define _EXT_DEBUG_SEVERITY_MEDIUM_ARB 0x9147
#define _EXT_DEBUG_SEVERITY_LOW_ARB 0x9148
#define _EXT_DEBUG_OUTPUT 0x92E0

#ifdef EGL_ENABLED
void *_egl2_load_function_wrapper(const char *p_name) {
	return (void *)eglGetProcAddress(p_name);
}
#endif

#include "platform_gl.h"

#if defined(MINGW_ENABLED) || defined(_MSC_VER)
#define strcpy strcpy_s
#endif

bool RasterizerGLES2::gles_over_gl = true;
RasterizerGLES2 *RasterizerGLES2::singleton = nullptr;

RasterizerGLES2::RasterizerGLES2() {
	singleton = this;

#ifdef GLAD_ENABLED
	bool glad_loaded = false;

#ifdef EGL_ENABLED
	// There should be a more flexible system for getting the GL pointer, as
	// different DisplayServers can have different ways. We can just use the GLAD
	// version global to see if it loaded for now though, otherwise we fall back to
	// the generic loader below.
#if defined(EGL_STATIC)
	bool has_egl = true;
#else
	bool has_egl = (eglGetProcAddress != nullptr);
#endif

	if (gles_over_gl) {
		if (has_egl && !glad_loaded && gladLoadGL((GLADloadfunc)&_egl2_load_function_wrapper)) {
			glad_loaded = true;
		}
	} else {
		if (has_egl && !glad_loaded && gladLoadGLES2((GLADloadfunc)&_egl2_load_function_wrapper)) {
			glad_loaded = true;
		}
	}
#endif // EGL_ENABLED

	if (gles_over_gl) {
		if (!glad_loaded && gladLoaderLoadGL()) {
			glad_loaded = true;
		}
	} else {
		if (!glad_loaded && gladLoaderLoadGLES2()) {
			glad_loaded = true;
		}
	}

	// FIXME this is an early return from a constructor.  Any other code using this instance will crash or the finalizer will crash, because none of
	// the members of this instance are initialized, so this just makes debugging harder.  It should either crash here intentionally,
	// or we need to actually test for this situation before constructing this.
	ERR_FAIL_COND_MSG(!glad_loaded, "Error initializing GLAD.");

	if (gles_over_gl) {
		if (OS::get_singleton()->is_stdout_verbose()) {
			if (GLAD_GL_ARB_debug_output) {
				glEnable(_EXT_DEBUG_OUTPUT_SYNCHRONOUS_ARB);
				// glDebugMessageCallbackARB((GLDEBUGPROCARB)_gl_debug_print, nullptr);
				glEnable(_EXT_DEBUG_OUTPUT);
			} else {
				print_line("OpenGL debugging not supported!");
			}
		}
	}
#endif // GLAD_ENABLED

	// For debugging
#ifdef CAN_DEBUG
#ifdef GL_API_ENABLED
	if (gles_over_gl) {
		if (OS::get_singleton()->is_stdout_verbose() && GLAD_GL_ARB_debug_output) {
			glDebugMessageControlARB(_EXT_DEBUG_SOURCE_API_ARB, _EXT_DEBUG_TYPE_ERROR_ARB, _EXT_DEBUG_SEVERITY_HIGH_ARB, 0, nullptr, GL_TRUE);
			glDebugMessageControlARB(_EXT_DEBUG_SOURCE_API_ARB, _EXT_DEBUG_TYPE_DEPRECATED_BEHAVIOR_ARB, _EXT_DEBUG_SEVERITY_HIGH_ARB, 0, nullptr, GL_TRUE);
			glDebugMessageControlARB(_EXT_DEBUG_SOURCE_API_ARB, _EXT_DEBUG_TYPE_UNDEFINED_BEHAVIOR_ARB, _EXT_DEBUG_SEVERITY_HIGH_ARB, 0, nullptr, GL_TRUE);
			glDebugMessageControlARB(_EXT_DEBUG_SOURCE_API_ARB, _EXT_DEBUG_TYPE_PORTABILITY_ARB, _EXT_DEBUG_SEVERITY_HIGH_ARB, 0, nullptr, GL_TRUE);
			glDebugMessageControlARB(_EXT_DEBUG_SOURCE_API_ARB, _EXT_DEBUG_TYPE_PERFORMANCE_ARB, _EXT_DEBUG_SEVERITY_HIGH_ARB, 0, nullptr, GL_TRUE);
			glDebugMessageControlARB(_EXT_DEBUG_SOURCE_API_ARB, _EXT_DEBUG_TYPE_OTHER_ARB, _EXT_DEBUG_SEVERITY_HIGH_ARB, 0, nullptr, GL_TRUE);
		}
	}
#endif // GL_API_ENABLED
#ifdef GLES_API_ENABLED
	if (!gles_over_gl) {
		if (OS::get_singleton()->is_stdout_verbose()) {
			DebugMessageCallbackARB callback = (DebugMessageCallbackARB)eglGetProcAddress("glDebugMessageCallback");
			if (!callback) {
				callback = (DebugMessageCallbackARB)eglGetProcAddress("glDebugMessageCallbackKHR");
			}

			if (callback) {
				print_line("godot: ENABLING GL DEBUG");
				glEnable(_EXT_DEBUG_OUTPUT_SYNCHRONOUS_ARB);
				callback((DEBUGPROCARB)_gl_debug_print, nullptr);
				glEnable(_EXT_DEBUG_OUTPUT);
			}
		}
	}
#endif // GLES_API_ENABLED
#endif // CAN_DEBUG

	{
		String shader_cache_dir = Engine::get_singleton()->get_shader_cache_path();
		if (shader_cache_dir.is_empty()) {
			shader_cache_dir = "user://";
		}
		Ref<DirAccess> da = DirAccess::open(shader_cache_dir);
		if (da.is_null()) {
			ERR_PRINT("Can't create shader cache folder, no shader caching will happen: " + shader_cache_dir);
		} else {
			Error err = da->change_dir("shader_cache");
			if (err != OK) {
				err = da->make_dir("shader_cache");
			}
			if (err != OK) {
				ERR_PRINT("Can't create shader cache folder, no shader caching will happen: " + shader_cache_dir);
			} else {
				shader_cache_dir = shader_cache_dir.path_join("shader_cache");

				bool shader_cache_enabled = GLOBAL_GET("rendering/shader_compiler/shader_cache/enabled");
				if (!Engine::get_singleton()->is_editor_hint() && !shader_cache_enabled) {
					shader_cache_dir = String(); //disable only if not editor
				}

				if (!shader_cache_dir.is_empty()) {
					ShaderGLES2::set_shader_cache_dir(shader_cache_dir);
				}
			}
		}
	}

	// Disable OpenGL linear to sRGB conversion, because Godot will always do this conversion itself.
	glDisable(GL_FRAMEBUFFER_SRGB);

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

	canvas = memnew(RasterizerCanvasGLES2());
	static_cast<RasterizerCanvasGLES2 *>(canvas)->initialize();
	scene = memnew(RasterizerSceneGLES());
}

RasterizerGLES2::~RasterizerGLES2() {

}

void RasterizerGLES2::initialize() {
	Engine::get_singleton()->print_header(
		vformat("OpenGL 2.0 API %s - Legacy - Using Device: %s - %s",
			RS::get_singleton()->get_video_adapter_api_version(),
			RS::get_singleton()->get_video_adapter_vendor(),
			RS::get_singleton()->get_video_adapter_name())
	);

	// FLIP XY Bug: Are more devices affected?
	// Confirmed so far: all Adreno 3xx with old driver (until 2018)
	// ok on some tested Adreno devices: 4xx, 5xx and 6xx
	// flip_xy_bugfix = GLES::Config::get_singleton()->flip_xy_bugfix;
}

void RasterizerGLES2::finalize() {
	memdelete(scene);
	memdelete(canvas);
	memdelete(gi);
	memdelete(fog);
	memdelete(copy_effects);
	memdelete(light_storage);
	memdelete(particles_storage);
	memdelete(mesh_storage);
	memdelete(material_storage);
	memdelete(texture_storage);
	memdelete(utilities);
	memdelete(config);
}

void RasterizerGLES2::begin_frame(double frame_step) {
	frame++;
	delta = frame_step;

	time_total += frame_step;

	double time_roll_over = GLOBAL_GET("rendering/limits/time/time_rollover_secs");
	time_total = Math::fmod(time_total, time_roll_over);

	canvas->set_time(time_total);
	scene->set_time(time_total, frame_step);

	GLES::Utilities *utils = GLES::Utilities::get_singleton();
	utils->_capture_timestamps_begin();
}

void RasterizerGLES2::end_frame(bool p_swap_buffers) {
	GLES::Utilities *utils = GLES::Utilities::get_singleton();
	utils->capture_timestamps_end();
}

void RasterizerGLES2::gl_end_frame(bool p_swap_buffers) {
	if (p_swap_buffers) {
		DisplayServer::get_singleton()->swap_buffers();
	} else {
		glFinish();
	}
}

void RasterizerGLES2::clear_depth(float p_depth) {
#ifdef GL_API_ENABLED
	if (is_gles_over_gl()) {
		glClearDepth(p_depth);
	}
#endif // GL_API_ENABLED
#ifdef GLES_API_ENABLED
	if (!is_gles_over_gl()) {
		glClearDepthf(p_depth);
	}
#endif // GLES_API_ENABLED
}

void RasterizerGLES2::blit_render_targets_to_screen(DisplayServer::WindowID p_screen, const BlitToScreen *p_render_targets, int p_amount) {
	for (int i = 0; i < p_amount; i++) {
		const BlitToScreen &blit = p_render_targets[i];

		RID rid_rt = blit.render_target;

		Rect2 dst_rect = blit.dst_rect;
		_blit_render_target_to_screen(rid_rt, p_screen, dst_rect, blit.multi_view.use_layer ? blit.multi_view.layer : 0, i == 0);
	}
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
