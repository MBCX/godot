#ifndef RASTERIZER_GLES2_H
#define RASTERIZER_GLES2_H

#ifdef GLES2_ENABLED

#include "drivers/gles_common/rasterizer_gles.h"

class RasterizerGLES2 : public RasterizerGLES {
protected:
	virtual void _blit_render_target_to_screen(RID p_render_target, DisplayServer::WindowID p_screen, const Rect2 &p_screen_rect, uint32_t p_layer, bool p_first = true) override;

public:
	RasterizerGLES2();

	static RendererCompositor *_create_current() {
		return memnew(RasterizerGLES2);
	}

	static void make_current(bool p_gles_over_gl) {
		//TODO: look into gles over gl!
		gles_over_gl = p_gles_over_gl;
		_create_func = _create_current;
		low_end = true;
	}

	virtual bool is_gles3() override { return false; }
};

#endif // GLES2_ENABLED
#endif // RASTERIZER_GLES2_H
