/**************************************************************************/
/*  texture_storage_gles2.h                                               */
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

#ifndef TEXTURE_STORAGE_GLES2_H
#define TEXTURE_STORAGE_GLES2_H

#if defined(GLES2_ENABLED)

#include "drivers/gles_common/storage/texture_storage.h"

namespace GLES {

class TextureStorageGLES2 : public GLES::TextureStorage {
protected:
	/* Texture API */
	virtual void _texture_set_data(RID p_texture, const Ref<Image> &p_image, int p_layer, bool p_initialize) override;

	/* Render Target API */
	virtual void _update_render_target(RenderTarget *rt) override;
	virtual void _render_target_allocate_sdf(RenderTarget *rt) override;

	virtual Ref<Image> _get_gl_image_and_format(const Ref<Image> &p_image, Image::Format p_format, Image::Format &r_real_format, GLenum &r_gl_format, GLenum &r_gl_internal_format, GLenum &r_gl_type, bool &r_compressed, bool p_force_decompress) const override;

public:
	/* Texture API */
	virtual RID texture_create_external(Texture::Type p_type, Image::Format p_format, unsigned int p_image, int p_width, int p_height, int p_depth, int p_layers, RS::TextureLayeredType p_layered_type = RS::TEXTURE_LAYERED_2D_ARRAY) override;

	virtual void texture_3d_initialize(RID p_texture, Image::Format, int p_width, int p_height, int p_depth, bool p_mipmaps, const Vector<Ref<Image>> &p_data) override;
	virtual void texture_3d_update(RID p_texture, const Vector<Ref<Image>> &p_data) override;
	virtual void texture_3d_placeholder_initialize(RID p_texture) override;
	virtual Vector<Ref<Image>> texture_3d_get(RID p_texture) const override;

	/* Render Target API */
	virtual void check_backbuffer(RenderTarget *rt, const bool uses_screen_texture, const bool uses_depth_texture) override;
	virtual void render_target_do_clear_request(RID p_render_target) override;

	virtual void render_target_sdf_process(RID p_render_target) override;
};
} //namespace GLES

#endif // GLES2_ENABLED
#endif // TEXTURE_STORAGE_GLES2_H