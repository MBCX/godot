/**************************************************************************/
/*  texture_storage.cpp                                                   */
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

#if defined(GLES3_ENABLED) || defined(GLES2_ENABLED)

#include "texture_storage.h"

#include "../effects/copy_effects.h"
#include "../rasterizer_gles.h"
#include "config.h"
#include "utilities.h"

#ifdef ANDROID_ENABLED
#define glFramebufferTextureMultiviewOVR GLES::Config::get_singleton()->eglFramebufferTextureMultiviewOVR
#endif

using namespace GLES;

TextureStorage *TextureStorage::singleton = nullptr;

TextureStorage *TextureStorage::get_singleton() {
	return singleton;
}

TextureStorage::TextureStorage() {
	singleton = this;
}

void TextureStorage::initialize() {
	{ //create default textures
		{ // White Textures

			Ref<Image> image = Image::create_empty(4, 4, true, Image::FORMAT_RGBA8);
			image->fill(Color(1, 1, 1, 1));
			image->generate_mipmaps();

			default_gl_textures[DEFAULT_GL_TEXTURE_WHITE] = texture_allocate();
			texture_2d_initialize(default_gl_textures[DEFAULT_GL_TEXTURE_WHITE], image);

			Vector<Ref<Image>> images;
			images.push_back(image);

			default_gl_textures[DEFAULT_GL_TEXTURE_2D_ARRAY_WHITE] = texture_allocate();
			texture_2d_layered_initialize(default_gl_textures[DEFAULT_GL_TEXTURE_2D_ARRAY_WHITE], images, RS::TEXTURE_LAYERED_2D_ARRAY);

			for (int i = 0; i < 5; i++) {
				images.push_back(image);
			}

			default_gl_textures[DEFAULT_GL_TEXTURE_CUBEMAP_WHITE] = texture_allocate();
			texture_2d_layered_initialize(default_gl_textures[DEFAULT_GL_TEXTURE_CUBEMAP_WHITE], images, RS::TEXTURE_LAYERED_CUBEMAP);
		}

		{
			Ref<Image> image = Image::create_empty(4, 4, false, Image::FORMAT_RGBA8);
			image->fill(Color(1, 1, 1, 1));

			Vector<Ref<Image>> images;
			for (int i = 0; i < 4; i++) {
				images.push_back(image);
			}
			default_gl_textures[DEFAULT_GL_TEXTURE_3D_WHITE] = texture_allocate();
			texture_3d_initialize(default_gl_textures[DEFAULT_GL_TEXTURE_3D_WHITE], image->get_format(), 4, 4, 4, false, images);
		}

		{ // black
			Ref<Image> image = Image::create_empty(4, 4, true, Image::FORMAT_RGBA8);
			image->fill(Color(0, 0, 0, 1));
			image->generate_mipmaps();

			default_gl_textures[DEFAULT_GL_TEXTURE_BLACK] = texture_allocate();
			texture_2d_initialize(default_gl_textures[DEFAULT_GL_TEXTURE_BLACK], image);

			Vector<Ref<Image>> images;
			for (int i = 0; i < 6; i++) {
				images.push_back(image);
			}
			default_gl_textures[DEFAULT_GL_TEXTURE_CUBEMAP_BLACK] = texture_allocate();
			texture_2d_layered_initialize(default_gl_textures[DEFAULT_GL_TEXTURE_CUBEMAP_BLACK], images, RS::TEXTURE_LAYERED_CUBEMAP);
		}

		{
			Ref<Image> image = Image::create_empty(4, 4, false, Image::FORMAT_RGBA8);
			image->fill(Color());

			Vector<Ref<Image>> images;
			for (int i = 0; i < 4; i++) {
				images.push_back(image);
			}
			default_gl_textures[DEFAULT_GL_TEXTURE_3D_BLACK] = texture_allocate();
			texture_3d_initialize(default_gl_textures[DEFAULT_GL_TEXTURE_3D_BLACK], image->get_format(), 4, 4, 4, false, images);
		}

		{ // transparent black
			Ref<Image> image = Image::create_empty(4, 4, true, Image::FORMAT_RGBA8);
			image->fill(Color(0, 0, 0, 0));
			image->generate_mipmaps();

			default_gl_textures[DEFAULT_GL_TEXTURE_TRANSPARENT] = texture_allocate();
			texture_2d_initialize(default_gl_textures[DEFAULT_GL_TEXTURE_TRANSPARENT], image);
		}

		{
			Ref<Image> image = Image::create_empty(4, 4, true, Image::FORMAT_RGBA8);
			image->fill(Color(0.5, 0.5, 1, 1));
			image->generate_mipmaps();

			default_gl_textures[DEFAULT_GL_TEXTURE_NORMAL] = texture_allocate();
			texture_2d_initialize(default_gl_textures[DEFAULT_GL_TEXTURE_NORMAL], image);
		}

		{
			Ref<Image> image = Image::create_empty(4, 4, true, Image::FORMAT_RGBA8);
			image->fill(Color(1.0, 0.5, 1, 1));
			image->generate_mipmaps();

			default_gl_textures[DEFAULT_GL_TEXTURE_ANISO] = texture_allocate();
			texture_2d_initialize(default_gl_textures[DEFAULT_GL_TEXTURE_ANISO], image);
		}

		{
			// RGBA8UI is only supported in GLES3
			//TODO: we need a dynmic if for cases where both gles3 and gles2 are enabled
#ifdef GLES3_ENABLED

			unsigned char pixel_data[4 * 4 * 4];
			for (int i = 0; i < 16; i++) {
				pixel_data[i * 4 + 0] = 0;
				pixel_data[i * 4 + 1] = 0;
				pixel_data[i * 4 + 2] = 0;
				pixel_data[i * 4 + 3] = 0;
			}

			default_gl_textures[DEFAULT_GL_TEXTURE_2D_UINT] = texture_allocate();
			Texture texture;
			texture.width = 4;
			texture.height = 4;
			texture.format = Image::FORMAT_RGBA8;
			texture.type = Texture::TYPE_2D;
			texture.target = GL_TEXTURE_2D;
			texture.active = true;
			glGenTextures(1, &texture.tex_id);
			texture_owner.initialize_rid(default_gl_textures[DEFAULT_GL_TEXTURE_2D_UINT], texture);

			glBindTexture(GL_TEXTURE_2D, texture.tex_id);

			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8UI, 4, 4, 0, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, pixel_data);

			Utilities::get_singleton()->texture_allocated_data(texture.tex_id, 4 * 4 * 4, "Default uint texture");
			texture.gl_set_filter(RS::CANVAS_ITEM_TEXTURE_FILTER_NEAREST);
#else
			Ref<Image> image = Image::create_empty(4, 4, true, Image::FORMAT_RGBA8);
			image->fill(Color(1.0, 0.5, 1, 1));
			image->generate_mipmaps();

			default_gl_textures[DEFAULT_GL_TEXTURE_2D_UINT] = texture_allocate();
			texture_2d_initialize(default_gl_textures[DEFAULT_GL_TEXTURE_2D_UINT], image);

#endif
		}
		{
			uint16_t pixel_data[4 * 4];
			for (int i = 0; i < 16; i++) {
				pixel_data[i] = Math::make_half_float(1.0f);
			}

			default_gl_textures[DEFAULT_GL_TEXTURE_DEPTH] = texture_allocate();
			Texture texture;
			texture.width = 4;
			texture.height = 4;
			texture.format = Image::FORMAT_RGBA8;
			texture.type = Texture::TYPE_2D;
			texture.target = GL_TEXTURE_2D;
			texture.active = true;
			glGenTextures(1, &texture.tex_id);
			texture_owner.initialize_rid(default_gl_textures[DEFAULT_GL_TEXTURE_DEPTH], texture);

			glBindTexture(GL_TEXTURE_2D, texture.tex_id);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16, 4, 4, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, pixel_data);
			Utilities::get_singleton()->texture_allocated_data(texture.tex_id, 4 * 4 * 2, "Default depth texture");
			texture.gl_set_filter(RS::CANVAS_ITEM_TEXTURE_FILTER_NEAREST);
		}
	}

	glBindTexture(GL_TEXTURE_2D, 0);

	{ // Atlas Texture initialize.
		uint8_t pixel_data[4 * 4 * 4];
		for (int i = 0; i < 16; i++) {
			pixel_data[i * 4 + 0] = 0;
			pixel_data[i * 4 + 1] = 0;
			pixel_data[i * 4 + 2] = 0;
			pixel_data[i * 4 + 3] = 255;
		}

		glGenTextures(1, &texture_atlas.texture);
		glBindTexture(GL_TEXTURE_2D, texture_atlas.texture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel_data);
		GLES::Utilities::get_singleton()->texture_allocated_data(texture_atlas.texture, 4 * 4 * 4, "Texture atlas (Default)");
	}

	glBindTexture(GL_TEXTURE_2D, 0);

	/*
		{
			sdf_shader.shader.initialize();
			sdf_shader.shader_version = sdf_shader.shader.version_create();
		}
	*/

#ifdef GL_API_ENABLED
	if (RasterizerGLES::is_gles_over_gl()) {
		glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
	}
#endif // GL_API_ENABLED
}

TextureStorage::~TextureStorage() {
	singleton = nullptr;
	for (int i = 0; i < DEFAULT_GL_TEXTURE_MAX; i++) {
		texture_free(default_gl_textures[i]);
	}
	if (texture_atlas.texture != 0) {
		GLES::Utilities::get_singleton()->texture_free_data(texture_atlas.texture);
	}
	texture_atlas.texture = 0;
	glDeleteFramebuffers(1, &texture_atlas.framebuffer);
	texture_atlas.framebuffer = 0;
	//sdf_shader.shader.version_free(sdf_shader.shader_version);
}

//TODO, move back to storage
bool TextureStorage::can_create_resources_async() const {
	return false;
}

/* Canvas Texture API */

RID TextureStorage::canvas_texture_allocate() {
	return canvas_texture_owner.allocate_rid();
}

void TextureStorage::canvas_texture_initialize(RID p_rid) {
	canvas_texture_owner.initialize_rid(p_rid);
}

void TextureStorage::canvas_texture_free(RID p_rid) {
	canvas_texture_owner.free(p_rid);
}

void TextureStorage::canvas_texture_set_channel(RID p_canvas_texture, RS::CanvasTextureChannel p_channel, RID p_texture) {
	CanvasTexture *ct = canvas_texture_owner.get_or_null(p_canvas_texture);
	ERR_FAIL_NULL(ct);

	switch (p_channel) {
		case RS::CANVAS_TEXTURE_CHANNEL_DIFFUSE: {
			ct->diffuse = p_texture;
		} break;
		case RS::CANVAS_TEXTURE_CHANNEL_NORMAL: {
			ct->normal_map = p_texture;
		} break;
		case RS::CANVAS_TEXTURE_CHANNEL_SPECULAR: {
			ct->specular = p_texture;
		} break;
	}
}

void TextureStorage::canvas_texture_set_shading_parameters(RID p_canvas_texture, const Color &p_specular_color, float p_shininess) {
	CanvasTexture *ct = canvas_texture_owner.get_or_null(p_canvas_texture);
	ERR_FAIL_NULL(ct);

	ct->specular_color.r = p_specular_color.r;
	ct->specular_color.g = p_specular_color.g;
	ct->specular_color.b = p_specular_color.b;
	ct->specular_color.a = p_shininess;
}

void TextureStorage::canvas_texture_set_texture_filter(RID p_canvas_texture, RS::CanvasItemTextureFilter p_filter) {
	CanvasTexture *ct = canvas_texture_owner.get_or_null(p_canvas_texture);
	ERR_FAIL_NULL(ct);

	ct->texture_filter = p_filter;
}

void TextureStorage::canvas_texture_set_texture_repeat(RID p_canvas_texture, RS::CanvasItemTextureRepeat p_repeat) {
	CanvasTexture *ct = canvas_texture_owner.get_or_null(p_canvas_texture);
	ERR_FAIL_NULL(ct);

	ct->texture_repeat = p_repeat;
}

/* Texture API */

RID TextureStorage::texture_allocate() {
	return texture_owner.allocate_rid();
}

void TextureStorage::texture_free(RID p_texture) {
	Texture *t = texture_owner.get_or_null(p_texture);
	ERR_FAIL_NULL(t);
	ERR_FAIL_COND(t->is_render_target);

	if (t->canvas_texture) {
		memdelete(t->canvas_texture);
	}

	bool must_free_data = false;
	if (t->is_proxy) {
		if (t->proxy_to.is_valid()) {
			Texture *proxy_to = texture_owner.get_or_null(t->proxy_to);
			if (proxy_to) {
				proxy_to->proxies.erase(p_texture);
			}
		}
	} else {
		must_free_data = t->tex_id != 0 && !t->is_external;
	}
	if (must_free_data) {
		Utilities::get_singleton()->texture_free_data(t->tex_id);
		t->tex_id = 0;
	}

	texture_atlas_remove_texture(p_texture);

	for (int i = 0; i < t->proxies.size(); i++) {
		Texture *p = texture_owner.get_or_null(t->proxies[i]);
		ERR_CONTINUE(!p);
		p->proxy_to = RID();
		p->tex_id = 0;
	}

	texture_owner.free(p_texture);
}

void TextureStorage::texture_2d_initialize(RID p_texture, const Ref<Image> &p_image) {
	ERR_FAIL_COND(p_image.is_null());

	Texture texture;
	texture.width = p_image->get_width();
	texture.height = p_image->get_height();
	texture.alloc_width = texture.width;
	texture.alloc_height = texture.height;
	texture.mipmaps = p_image->get_mipmap_count() + 1;
	texture.format = p_image->get_format();
	texture.type = Texture::TYPE_2D;
	texture.target = GL_TEXTURE_2D;
	Ref<Image> dummy;
	_get_gl_image_and_format(dummy, texture.format, texture.real_format, texture.gl_format_cache, texture.gl_internal_format_cache, texture.gl_type_cache, texture.compressed, false);
	texture.total_data_size = p_image->get_image_data_size(texture.width, texture.height, texture.format, texture.mipmaps);
	texture.active = true;
	glGenTextures(1, &texture.tex_id);
	GLES::Utilities::get_singleton()->texture_allocated_data(texture.tex_id, texture.total_data_size, "Texture 2D");
	texture_owner.initialize_rid(p_texture, texture);
	texture_set_data(p_texture, p_image);
}

void TextureStorage::texture_2d_layered_initialize(RID p_texture, const Vector<Ref<Image>> &p_layers, RS::TextureLayeredType p_layered_type) {
	ERR_FAIL_COND(p_layers.is_empty());

	ERR_FAIL_COND(p_layered_type == RS::TEXTURE_LAYERED_CUBEMAP && p_layers.size() != 6);
	ERR_FAIL_COND_MSG(p_layered_type == RS::TEXTURE_LAYERED_CUBEMAP_ARRAY, "Cubemap Arrays are not supported in the GLES2 backend.");
	ERR_FAIL_COND_MSG(p_layered_type == RS::TEXTURE_LAYERED_2D_ARRAY, "2D Texture Arrays are not supported in the GLES2 backend.");

	const Ref<Image> &image = p_layers[0];
	{
		int valid_width = 0;
		int valid_height = 0;
		bool valid_mipmaps = false;
		Image::Format valid_format = Image::FORMAT_MAX;

		for (int i = 0; i < p_layers.size(); i++) {
			ERR_FAIL_COND(p_layers[i]->is_empty());

			if (i == 0) {
				valid_width = p_layers[i]->get_width();
				valid_height = p_layers[i]->get_height();
				valid_format = p_layers[i]->get_format();
				valid_mipmaps = p_layers[i]->has_mipmaps();
			} else {
				ERR_FAIL_COND(p_layers[i]->get_width() != valid_width);
				ERR_FAIL_COND(p_layers[i]->get_height() != valid_height);
				ERR_FAIL_COND(p_layers[i]->get_format() != valid_format);
				ERR_FAIL_COND(p_layers[i]->has_mipmaps() != valid_mipmaps);
			}
		}
	}

	GLES::Texture texture;
	texture.width = image->get_width();
	texture.height = image->get_height();
	texture.alloc_width = texture.width;
	texture.alloc_height = texture.height;
	texture.mipmaps = image->get_mipmap_count() + 1;
	texture.format = image->get_format();
	texture.type = GLES::Texture::TYPE_LAYERED;
	texture.layered_type = p_layered_type;
	texture.target = GL_TEXTURE_CUBE_MAP;
	texture.layers = p_layers.size();
	_get_gl_image_and_format(Ref<Image>(), texture.format, texture.real_format, texture.gl_format_cache, texture.gl_internal_format_cache, texture.gl_type_cache, texture.compressed, false);
	texture.total_data_size = p_layers[0]->get_image_data_size(texture.width, texture.height, texture.format, texture.mipmaps) * texture.layers;
	texture.active = true;
	glGenTextures(1, &texture.tex_id);
	GLES::Utilities::get_singleton()->texture_allocated_data(texture.tex_id, texture.total_data_size, "Texture Layered");
	texture_owner.initialize_rid(p_texture, texture);
	for (int i = 0; i < p_layers.size(); i++) {
		_texture_set_data(p_texture, p_layers[i], i, i == 0);
	}
}

// Called internally when texture_proxy_create(p_base) is called.
// Note: p_base is the root and p_texture is the proxy.
void TextureStorage::texture_proxy_initialize(RID p_texture, RID p_base) {
	Texture *texture = texture_owner.get_or_null(p_base);
	ERR_FAIL_NULL(texture);
	Texture proxy_tex;
	proxy_tex.copy_from(*texture);
	proxy_tex.proxy_to = p_base;
	proxy_tex.is_render_target = false;
	proxy_tex.is_proxy = true;
	proxy_tex.proxies.clear();
	texture->proxies.push_back(p_texture);
	texture_owner.initialize_rid(p_texture, proxy_tex);
}

void TextureStorage::texture_2d_update(RID p_texture, const Ref<Image> &p_image, int p_layer) {
	texture_set_data(p_texture, p_image, p_layer);

	Texture *tex = texture_owner.get_or_null(p_texture);
	ERR_FAIL_NULL(tex);
	GLES::Utilities::get_singleton()->texture_resize_data(tex->tex_id, tex->total_data_size);

#ifdef TOOLS_ENABLED
	tex->image_cache_2d.unref();
#endif
}

void TextureStorage::texture_proxy_update(RID p_texture, RID p_proxy_to) {
	Texture *tex = texture_owner.get_or_null(p_texture);
	ERR_FAIL_NULL(tex);
	ERR_FAIL_COND(!tex->is_proxy);
	Texture *proxy_to = texture_owner.get_or_null(p_proxy_to);
	ERR_FAIL_NULL(proxy_to);
	ERR_FAIL_COND(proxy_to->is_proxy);

	if (tex->proxy_to.is_valid()) {
		Texture *prev_tex = texture_owner.get_or_null(tex->proxy_to);
		ERR_FAIL_NULL(prev_tex);
		prev_tex->proxies.erase(p_texture);
	}

	*tex = *proxy_to;

	tex->proxy_to = p_proxy_to;
	tex->is_render_target = false;
	tex->is_proxy = true;
	tex->proxies.clear();
	tex->canvas_texture = nullptr;
	tex->tex_id = 0;
	proxy_to->proxies.push_back(p_texture);
}

void TextureStorage::texture_2d_placeholder_initialize(RID p_texture) {
	//this could be better optimized to reuse an existing image , done this way
	//for now to get it working
	Ref<Image> image = Image::create_empty(4, 4, false, Image::FORMAT_RGBA8);
	image->fill(Color(1, 0, 1, 1));

	texture_2d_initialize(p_texture, image);
}

void TextureStorage::texture_2d_layered_placeholder_initialize(RID p_texture, RenderingServer::TextureLayeredType p_layered_type) {
	//this could be better optimized to reuse an existing image , done this way
	//for now to get it working
	Ref<Image> image = Image::create_empty(4, 4, false, Image::FORMAT_RGBA8);
	image->fill(Color(1, 0, 1, 1));

	Vector<Ref<Image>> images;
	if (p_layered_type == RS::TEXTURE_LAYERED_2D_ARRAY) {
		images.push_back(image);
	} else {
		//cube
		for (int i = 0; i < 6; i++) {
			images.push_back(image);
		}
	}

	texture_2d_layered_initialize(p_texture, images, p_layered_type);
}

void TextureStorage::texture_3d_placeholder_initialize(RID p_texture) {
	//this could be better optimized to reuse an existing image , done this way
	//for now to get it working
	Ref<Image> image = Image::create_empty(4, 4, false, Image::FORMAT_RGBA8);
	image->fill(Color(1, 0, 1, 1));

	Vector<Ref<Image>> images;
	//cube
	for (int i = 0; i < 4; i++) {
		images.push_back(image);
	}

	texture_3d_initialize(p_texture, Image::FORMAT_RGBA8, 4, 4, 4, false, images);
}

Ref<Image> TextureStorage::texture_2d_get(RID p_texture) const {
	Texture *texture = texture_owner.get_or_null(p_texture);
	ERR_FAIL_NULL_V(texture, Ref<Image>());

#ifdef TOOLS_ENABLED
	if (texture->image_cache_2d.is_valid() && !texture->is_render_target) {
		return texture->image_cache_2d;
	}
#endif

	Ref<Image> image;
#if defined(GL_API_ENABLED) && defined(GLES3_ENABLED)
	if (RasterizerGLES::is_gles_over_gl() && RasterizerGLES::get_singleton()->is_gles3()) {
		// OpenGL 3.3 supports glGetTexImage which is faster and simpler than glReadPixels.
		// It also allows for reading compressed textures, mipmaps, and more formats.
		Vector<uint8_t> data;

		int data_size = Image::get_image_data_size(texture->alloc_width, texture->alloc_height, texture->real_format, texture->mipmaps > 1);

		data.resize(data_size * 2); // Add some memory at the end, just in case for buggy drivers.
		uint8_t *w = data.ptrw();

		glActiveTexture(GL_TEXTURE0);

		glBindTexture(texture->target, texture->tex_id);

		glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

		for (int i = 0; i < texture->mipmaps; i++) {
			int ofs = Image::get_image_mipmap_offset(texture->alloc_width, texture->alloc_height, texture->real_format, i);

			if (texture->compressed) {
				glPixelStorei(GL_PACK_ALIGNMENT, 4);
				glGetCompressedTexImage(texture->target, i, &w[ofs]);

			} else {
				glPixelStorei(GL_PACK_ALIGNMENT, 1);

				glGetTexImage(texture->target, i, texture->gl_format_cache, texture->gl_type_cache, &w[ofs]);
			}
		}

		data.resize(data_size);

		ERR_FAIL_COND_V(data.is_empty(), Ref<Image>());
		image = Image::create_from_data(texture->width, texture->height, texture->mipmaps > 1, texture->real_format, data);
		ERR_FAIL_COND_V(image->is_empty(), Ref<Image>());
		if (texture->format != texture->real_format) {
			image->convert(texture->format);
		}
	}
#endif // GL_API_ENABLED
#if defined(GLES_API_ENABLED)
	if (!RasterizerGLES::is_gles_over_gl() || !RasterizerGLES::get_singleton()->is_gles3()) {
		Vector<uint8_t> data;

		// On web and mobile we always read an RGBA8 image with no mipmaps.
		int data_size = Image::get_image_data_size(texture->alloc_width, texture->alloc_height, Image::FORMAT_RGBA8, false);

		data.resize(data_size * 2); // Add some memory at the end, just in case for buggy drivers.
		uint8_t *w = data.ptrw();

		GLuint temp_framebuffer;
		glGenFramebuffers(1, &temp_framebuffer);

		GLuint temp_color_texture;
		glGenTextures(1, &temp_color_texture);

		glBindFramebuffer(GL_FRAMEBUFFER, temp_framebuffer);

		glBindTexture(GL_TEXTURE_2D, temp_color_texture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texture->alloc_width, texture->alloc_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, temp_color_texture, 0);

		glDepthMask(GL_FALSE);
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE);
		glDisable(GL_BLEND);
		glDepthFunc(GL_LEQUAL);
		glColorMask(1, 1, 1, 1);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, texture->tex_id);

		glViewport(0, 0, texture->alloc_width, texture->alloc_height);
		glClearColor(0.0, 0.0, 0.0, 0.0);
		glClear(GL_COLOR_BUFFER_BIT);

		CopyEffects::get_singleton()->copy_to_rect(Rect2i(0, 0, 1.0, 1.0));

		glReadPixels(0, 0, texture->alloc_width, texture->alloc_height, GL_RGBA, GL_UNSIGNED_BYTE, &w[0]);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glDeleteTextures(1, &temp_color_texture);
		glDeleteFramebuffers(1, &temp_framebuffer);

		data.resize(data_size);

		ERR_FAIL_COND_V(data.is_empty(), Ref<Image>());
		image = Image::create_from_data(texture->width, texture->height, false, Image::FORMAT_RGBA8, data);
		ERR_FAIL_COND_V(image->is_empty(), Ref<Image>());

		if (texture->format != Image::FORMAT_RGBA8) {
			image->convert(texture->format);
		}

		if (texture->mipmaps > 1) {
			image->generate_mipmaps();
		}
	}
#endif // GLES_API_ENABLED

#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint() && !texture->is_render_target) {
		texture->image_cache_2d = image;
	}
#endif

	return image;
}

Ref<Image> TextureStorage::texture_2d_layer_get(RID p_texture, int p_layer) const {
	//TODO: implement me only for gles3
	ERR_FAIL_V_MSG(Ref<Image>(), "2D layered textures are currently not supported by gles2");
}

Vector<Ref<Image>> TextureStorage::_texture_3d_read_framebuffer(Texture *p_texture) const {
	ERR_FAIL_NULL_V(p_texture, Vector<Ref<Image>>());

	Vector<Ref<Image>> ret;
	Vector<uint8_t> data;

	int width = p_texture->width;
	int height = p_texture->height;
	int depth = p_texture->depth;

	for (int mipmap_level = 0; mipmap_level < p_texture->mipmaps; mipmap_level++) {
		int data_size = Image::get_image_data_size(width, height, Image::FORMAT_RGBA8, false);
		glViewport(0, 0, width, height);
		glClearColor(0.0, 0.0, 0.0, 0.0);
		glClear(GL_COLOR_BUFFER_BIT);

		for (int layer = 0; layer < depth; layer++) {
			data.resize(data_size * 2); //add some memory at the end, just in case for buggy drivers
			uint8_t *w = data.ptrw();

			float layer_f = layer / float(depth);
			CopyEffects::get_singleton()->copy_to_rect_3d(Rect2i(0, 0, 1, 1), layer_f, Texture::TYPE_3D, mipmap_level);
			glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, &w[0]);

			data.resize(data_size);
			ERR_FAIL_COND_V(data.is_empty(), Vector<Ref<Image>>());

			Ref<Image> img = Image::create_from_data(width, height, false, Image::FORMAT_RGBA8, data);
			ERR_FAIL_COND_V(img->is_empty(), Vector<Ref<Image>>());

			if (p_texture->format != Image::FORMAT_RGBA8) {
				img->convert(p_texture->format);
			}

			ret.push_back(img);
		}

		width = MAX(1, width >> 1);
		height = MAX(1, height >> 1);
		depth = MAX(1, depth >> 1);
	}

	return ret;
}
/*
Vector<Ref<Image>> TextureStorage::texture_3d_get(RID p_texture) const {
	Texture *texture = texture_owner.get_or_null(p_texture);
	ERR_FAIL_NULL_V(texture, Vector<Ref<Image>>());
	ERR_FAIL_COND_V(texture->type != Texture::TYPE_3D, Vector<Ref<Image>>());

#ifdef TOOLS_ENABLED
	if (!texture->image_cache_3d.is_empty() && !texture->is_render_target) {
		return texture->image_cache_3d;
	}
#endif

	GLuint temp_framebuffer;
	glGenFramebuffers(1, &temp_framebuffer);

	GLuint temp_color_texture;
	glGenTextures(1, &temp_color_texture);

	glBindFramebuffer(GL_FRAMEBUFFER, temp_framebuffer);

	glBindTexture(GL_TEXTURE_2D, temp_color_texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texture->alloc_width, texture->alloc_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, temp_color_texture, 0);

	glDepthMask(GL_FALSE);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_BLEND);
	glDepthFunc(GL_LEQUAL);
	glColorMask(1, 1, 1, 1);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_3D, texture->tex_id);

	Vector<Ref<Image>> ret = _texture_3d_read_framebuffer(texture);

	glBindFramebuffer(GL_FRAMEBUFFER, TextureStorage::system_fbo);
	glDeleteTextures(1, &temp_color_texture);
	glDeleteFramebuffers(1, &temp_framebuffer);

#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint() && !texture->is_render_target) {
		texture->image_cache_3d = ret;
	}
#endif

	return ret;

}
*/

void TextureStorage::texture_replace(RID p_texture, RID p_by_texture) {
	Texture *tex_to = texture_owner.get_or_null(p_texture);
	ERR_FAIL_NULL(tex_to);
	ERR_FAIL_COND(tex_to->is_proxy); //can't replace proxy
	Texture *tex_from = texture_owner.get_or_null(p_by_texture);
	ERR_FAIL_NULL(tex_from);
	ERR_FAIL_COND(tex_from->is_proxy); //can't replace proxy

	if (tex_to == tex_from) {
		return;
	}

	if (tex_to->canvas_texture) {
		memdelete(tex_to->canvas_texture);
		tex_to->canvas_texture = nullptr;
	}

	if (tex_to->tex_id) {
		GLES::Utilities::get_singleton()->texture_free_data(tex_to->tex_id);
		tex_to->tex_id = 0;
	}

	Vector<RID> proxies_to_update = tex_to->proxies;
	Vector<RID> proxies_to_redirect = tex_from->proxies;

	*tex_to = *tex_from;

	tex_to->proxies = proxies_to_update; //restore proxies, so they can be updated

	if (tex_to->canvas_texture) {
		tex_to->canvas_texture->diffuse = p_texture; //update
	}

	for (int i = 0; i < proxies_to_update.size(); i++) {
		texture_proxy_update(proxies_to_update[i], p_texture);
	}
	for (int i = 0; i < proxies_to_redirect.size(); i++) {
		texture_proxy_update(proxies_to_redirect[i], p_texture);
	}
	//delete last, so proxies can be updated
	texture_owner.free(p_by_texture);

	texture_atlas_mark_dirty_on_texture(p_texture);
}

void TextureStorage::texture_set_size_override(RID p_texture, int p_width, int p_height) {
	Texture *texture = texture_owner.get_or_null(p_texture);

	ERR_FAIL_NULL(texture);
	ERR_FAIL_COND(texture->is_render_target);

	ERR_FAIL_COND(p_width <= 0 || p_width > 16384);
	ERR_FAIL_COND(p_height <= 0 || p_height > 16384);
	//real texture size is in alloc width and height
	texture->width = p_width;
	texture->height = p_height;
}

void TextureStorage::texture_set_path(RID p_texture, const String &p_path) {
	Texture *texture = texture_owner.get_or_null(p_texture);
	ERR_FAIL_NULL(texture);

	texture->path = p_path;
}

String TextureStorage::texture_get_path(RID p_texture) const {
	Texture *texture = texture_owner.get_or_null(p_texture);
	ERR_FAIL_NULL_V(texture, "");

	return texture->path;
}

void TextureStorage::texture_set_detect_3d_callback(RID p_texture, RS::TextureDetectCallback p_callback, void *p_userdata) {
	Texture *texture = texture_owner.get_or_null(p_texture);
	ERR_FAIL_NULL(texture);

	texture->detect_3d_callback = p_callback;
	texture->detect_3d_callback_ud = p_userdata;
}

void TextureStorage::texture_set_detect_srgb_callback(RID p_texture, RS::TextureDetectCallback p_callback, void *p_userdata) {
}

void TextureStorage::texture_set_detect_normal_callback(RID p_texture, RS::TextureDetectCallback p_callback, void *p_userdata) {
	Texture *texture = texture_owner.get_or_null(p_texture);
	ERR_FAIL_NULL(texture);

	texture->detect_normal_callback = p_callback;
	texture->detect_normal_callback_ud = p_userdata;
}

void TextureStorage::texture_set_detect_roughness_callback(RID p_texture, RS::TextureDetectRoughnessCallback p_callback, void *p_userdata) {
	Texture *texture = texture_owner.get_or_null(p_texture);
	ERR_FAIL_NULL(texture);

	texture->detect_roughness_callback = p_callback;
	texture->detect_roughness_callback_ud = p_userdata;
}

void TextureStorage::texture_debug_usage(List<RS::TextureInfo> *r_info) {
	List<RID> textures;
	texture_owner.get_owned_list(&textures);

	for (List<RID>::Element *E = textures.front(); E; E = E->next()) {
		Texture *t = texture_owner.get_or_null(E->get());
		if (!t) {
			continue;
		}
		RS::TextureInfo tinfo;
		tinfo.path = t->path;
		tinfo.format = t->format;
		tinfo.width = t->alloc_width;
		tinfo.height = t->alloc_height;
		tinfo.depth = 0;
		tinfo.bytes = t->total_data_size;
		r_info->push_back(tinfo);
	}
}

void TextureStorage::texture_set_force_redraw_if_visible(RID p_texture, bool p_enable) {
	Texture *texture = texture_owner.get_or_null(p_texture);
	ERR_FAIL_NULL(texture);

	texture->redraw_if_visible = p_enable;
}

Size2 TextureStorage::texture_size_with_proxy(RID p_texture) {
	const Texture *texture = texture_owner.get_or_null(p_texture);
	ERR_FAIL_NULL_V(texture, Size2());
	if (texture->is_proxy) {
		const Texture *proxy = texture_owner.get_or_null(texture->proxy_to);
		return Size2(proxy->width, proxy->height);
	} else {
		return Size2(texture->width, texture->height);
	}
}

void TextureStorage::texture_rd_initialize(RID p_texture, const RID &p_rd_texture, const RS::TextureLayeredType p_layer_type) {
}

RID TextureStorage::texture_get_rd_texture(RID p_texture, bool p_srgb) const {
	return RID();
}

uint64_t TextureStorage::texture_get_native_handle(RID p_texture, bool p_srgb) const {
	const Texture *texture = texture_owner.get_or_null(p_texture);
	ERR_FAIL_NULL_V(texture, 0);

	return texture->tex_id;
}

void TextureStorage::texture_set_data(RID p_texture, const Ref<Image> &p_image, int p_layer) {
	_texture_set_data(p_texture, p_image, p_layer, false);
}

Image::Format TextureStorage::texture_get_format(RID p_texture) const {
	Texture *texture = texture_owner.get_or_null(p_texture);

	ERR_FAIL_NULL_V(texture, Image::FORMAT_L8);

	return texture->format;
}

uint32_t TextureStorage::texture_get_texid(RID p_texture) const {
	Texture *texture = texture_owner.get_or_null(p_texture);

	ERR_FAIL_NULL_V(texture, 0);

	return texture->tex_id;
}

uint32_t TextureStorage::texture_get_width(RID p_texture) const {
	Texture *texture = texture_owner.get_or_null(p_texture);

	ERR_FAIL_NULL_V(texture, 0);

	return texture->width;
}

uint32_t TextureStorage::texture_get_height(RID p_texture) const {
	Texture *texture = texture_owner.get_or_null(p_texture);

	ERR_FAIL_NULL_V(texture, 0);

	return texture->height;
}

uint32_t TextureStorage::texture_get_depth(RID p_texture) const {
	Texture *texture = texture_owner.get_or_null(p_texture);

	ERR_FAIL_NULL_V(texture, 0);

	return texture->depth;
}

void TextureStorage::texture_bind(RID p_texture, uint32_t p_texture_no) {
	Texture *texture = texture_owner.get_or_null(p_texture);

	ERR_FAIL_NULL(texture);

	glActiveTexture(GL_TEXTURE0 + p_texture_no);
	glBindTexture(texture->target, texture->tex_id);
}

/* TEXTURE ATLAS API */

void TextureStorage::texture_add_to_texture_atlas(RID p_texture) {
	if (!texture_atlas.textures.has(p_texture)) {
		TextureAtlas::Texture t;
		t.users = 1;
		texture_atlas.textures[p_texture] = t;
		texture_atlas.dirty = true;
	} else {
		TextureAtlas::Texture *t = texture_atlas.textures.getptr(p_texture);
		t->users++;
	}
}

void TextureStorage::texture_remove_from_texture_atlas(RID p_texture) {
	TextureAtlas::Texture *t = texture_atlas.textures.getptr(p_texture);
	ERR_FAIL_NULL(t);
	t->users--;
	if (t->users == 0) {
		texture_atlas.textures.erase(p_texture);
		// Do not mark it dirty, there is no need to since it remains working.
	}
}

void TextureStorage::texture_atlas_mark_dirty_on_texture(RID p_texture) {
	if (texture_atlas.textures.has(p_texture)) {
		texture_atlas.dirty = true; // Mark it dirty since it was most likely modified.
	}
}

void TextureStorage::texture_atlas_remove_texture(RID p_texture) {
	if (texture_atlas.textures.has(p_texture)) {
		texture_atlas.textures.erase(p_texture);
		// There is not much a point of making it dirty, texture can be removed next time the atlas is updated.
	}
}

GLuint TextureStorage::texture_atlas_get_texture() const {
	return texture_atlas.texture;
}

void TextureStorage::update_texture_atlas() {
	CopyEffects *copy_effects = CopyEffects::get_singleton();
	ERR_FAIL_NULL(copy_effects);

	if (!texture_atlas.dirty) {
		return; //nothing to do
	}

	texture_atlas.dirty = false;

	if (texture_atlas.texture != 0) {
		GLES::Utilities::get_singleton()->texture_free_data(texture_atlas.texture);
		texture_atlas.texture = 0;
		glDeleteFramebuffers(1, &texture_atlas.framebuffer);
		texture_atlas.framebuffer = 0;
	}

	const int border = 2;

	if (texture_atlas.textures.size()) {
		//generate atlas
		Vector<TextureAtlas::SortItem> itemsv;
		itemsv.resize(texture_atlas.textures.size());
		uint32_t base_size = 8;

		int idx = 0;

		for (const KeyValue<RID, TextureAtlas::Texture> &E : texture_atlas.textures) {
			TextureAtlas::SortItem &si = itemsv.write[idx];

			Texture *src_tex = get_texture(E.key);

			si.size.width = (src_tex->width / border) + 1;
			si.size.height = (src_tex->height / border) + 1;
			si.pixel_size = Size2i(src_tex->width, src_tex->height);

			if (base_size < (uint32_t)si.size.width) {
				base_size = nearest_power_of_2_templated(si.size.width);
			}

			si.texture = E.key;
			idx++;
		}

		//sort items by size
		itemsv.sort();

		//attempt to create atlas
		int item_count = itemsv.size();
		TextureAtlas::SortItem *items = itemsv.ptrw();

		int atlas_height = 0;

		while (true) {
			Vector<int> v_offsetsv;
			v_offsetsv.resize(base_size);

			int *v_offsets = v_offsetsv.ptrw();
			memset(v_offsets, 0, sizeof(int) * base_size);

			int max_height = 0;

			for (int i = 0; i < item_count; i++) {
				//best fit
				TextureAtlas::SortItem &si = items[i];
				int best_idx = -1;
				int best_height = 0x7FFFFFFF;
				for (uint32_t j = 0; j <= base_size - si.size.width; j++) {
					int height = 0;
					for (int k = 0; k < si.size.width; k++) {
						int h = v_offsets[k + j];
						if (h > height) {
							height = h;
							if (height > best_height) {
								break; //already bad
							}
						}
					}

					if (height < best_height) {
						best_height = height;
						best_idx = j;
					}
				}

				//update
				for (int k = 0; k < si.size.width; k++) {
					v_offsets[k + best_idx] = best_height + si.size.height;
				}

				si.pos.x = best_idx;
				si.pos.y = best_height;

				if (si.pos.y + si.size.height > max_height) {
					max_height = si.pos.y + si.size.height;
				}
			}

			if ((uint32_t)max_height <= base_size * 2) {
				atlas_height = max_height;
				break; //good ratio, break;
			}

			base_size *= 2;
		}

		texture_atlas.size.width = base_size * border;
		texture_atlas.size.height = nearest_power_of_2_templated(atlas_height * border);

		for (int i = 0; i < item_count; i++) {
			TextureAtlas::Texture *t = texture_atlas.textures.getptr(items[i].texture);
			t->uv_rect.position = items[i].pos * border + Vector2i(border / 2, border / 2);
			t->uv_rect.size = items[i].pixel_size;

			t->uv_rect.position /= Size2(texture_atlas.size);
			t->uv_rect.size /= Size2(texture_atlas.size);
		}
	} else {
		texture_atlas.size.width = 4;
		texture_atlas.size.height = 4;
	}

	{ // Atlas Texture initialize.
		// TODO validate texture atlas size with maximum texture size
		glGenTextures(1, &texture_atlas.texture);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, texture_atlas.texture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texture_atlas.size.width, texture_atlas.size.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
		GLES::Utilities::get_singleton()->texture_allocated_data(texture_atlas.texture, texture_atlas.size.width * texture_atlas.size.height * 4, "Texture atlas");

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		//TODO: renable me for gles3
		//		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
		//		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 1);

		glGenFramebuffers(1, &texture_atlas.framebuffer);
		glBindFramebuffer(GL_FRAMEBUFFER, texture_atlas.framebuffer);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_atlas.texture, 0);

		GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

		if (status != GL_FRAMEBUFFER_COMPLETE) {
			glDeleteFramebuffers(1, &texture_atlas.framebuffer);
			texture_atlas.framebuffer = 0;
			GLES::Utilities::get_singleton()->texture_free_data(texture_atlas.texture);
			texture_atlas.texture = 0;
			WARN_PRINT("Could not create texture atlas, status: " + get_framebuffer_error(status));
			return;
		}
		glViewport(0, 0, texture_atlas.size.width, texture_atlas.size.height);
		glClearColor(0.0, 0.0, 0.0, 0.0);
		glClear(GL_COLOR_BUFFER_BIT);
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	glDisable(GL_BLEND);

	if (texture_atlas.textures.size()) {
		for (const KeyValue<RID, TextureAtlas::Texture> &E : texture_atlas.textures) {
			TextureAtlas::Texture *t = texture_atlas.textures.getptr(E.key);
			Texture *src_tex = get_texture(E.key);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, src_tex->tex_id);
			copy_effects->copy_to_rect(t->uv_rect);
		}
	}
	glBindFramebuffer(GL_FRAMEBUFFER, TextureStorage::system_fbo);
}

/* DECAL API */

RID TextureStorage::decal_allocate() {
	return RID();
}

void TextureStorage::decal_initialize(RID p_rid) {
}

void TextureStorage::decal_set_size(RID p_decal, const Vector3 &p_size) {
}

void TextureStorage::decal_set_texture(RID p_decal, RS::DecalTexture p_type, RID p_texture) {
}

void TextureStorage::decal_set_emission_energy(RID p_decal, float p_energy) {
}

void TextureStorage::decal_set_albedo_mix(RID p_decal, float p_mix) {
}

void TextureStorage::decal_set_modulate(RID p_decal, const Color &p_modulate) {
}

void TextureStorage::decal_set_cull_mask(RID p_decal, uint32_t p_layers) {
}

void TextureStorage::decal_set_distance_fade(RID p_decal, bool p_enabled, float p_begin, float p_length) {
}

void TextureStorage::decal_set_fade(RID p_decal, float p_above, float p_below) {
}

void TextureStorage::decal_set_normal_fade(RID p_decal, float p_fade) {
}

AABB TextureStorage::decal_get_aabb(RID p_decal) const {
	return AABB();
}

/* RENDER TARGET API */

GLuint TextureStorage::system_fbo = 0;

void TextureStorage::_create_render_target_backbuffer(RenderTarget *rt) {
	ERR_FAIL_COND_MSG(rt->backbuffer_fbo != 0, "Cannot allocate RenderTarget backbuffer: already initialized.");
	ERR_FAIL_COND(rt->direct_to_screen);
	// Allocate mipmap chains for full screen blur
	// Limit mipmaps so smallest is 32x32 to avoid unnecessary framebuffer switches
	int count = MAX(1, Image::get_image_required_mipmaps(rt->size.x, rt->size.y, Image::FORMAT_RGBA8) - 4);
	if (rt->size.x > 40 && rt->size.y > 40) {
		GLsizei width = rt->size.x;
		GLsizei height = rt->size.y;

		rt->mipmap_count = count;

		glGenTextures(1, &rt->backbuffer);
		glBindTexture(GL_TEXTURE_2D, rt->backbuffer);
		uint32_t texture_size_bytes = 0;

		for (int l = 0; l < count; l++) {
			texture_size_bytes += width * height * 4;
			glTexImage2D(GL_TEXTURE_2D, l, rt->color_internal_format, width, height, 0, rt->color_format, rt->color_type, nullptr);
			width = MAX(1, (width / 2));
			height = MAX(1, (height / 2));
		}

		//TODO: renable me for gles3
		//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
		//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, count - 1);

		glGenFramebuffers(1, &rt->backbuffer_fbo);
		glBindFramebuffer(GL_FRAMEBUFFER, rt->backbuffer_fbo);

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rt->backbuffer, 0);

		GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (status != GL_FRAMEBUFFER_COMPLETE) {
			WARN_PRINT_ONCE("Cannot allocate mipmaps for canvas screen blur. Status: " + get_framebuffer_error(status));
			glBindFramebuffer(GL_FRAMEBUFFER, system_fbo);
			return;
		}
		GLES::Utilities::get_singleton()->texture_allocated_data(rt->backbuffer, texture_size_bytes, "Render target backbuffer color texture");

		// Initialize all levels to clear black.
		for (int j = 0; j < count; j++) {
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rt->backbuffer, j);
			glClearColor(0.0, 0.0, 0.0, 0.0);
			glClear(GL_COLOR_BUFFER_BIT);
		}

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rt->backbuffer, 0);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}
}

void TextureStorage::_clear_render_target(RenderTarget *rt) {
	// there is nothing else to clear when DIRECT_TO_SCREEN is used
	if (rt->direct_to_screen) {
		return;
	}

	// Dispose of the cached fbo's and the allocated textures
	for (KeyValue<uint32_t, RenderTarget::RTOverridden::FBOCacheEntry> &E : rt->overridden.fbo_cache) {
		glDeleteTextures(E.value.allocated_textures.size(), E.value.allocated_textures.ptr());
		// Don't delete the current FBO, we'll do that a couple lines down.
		if (E.value.fbo != rt->fbo) {
			glDeleteFramebuffers(1, &E.value.fbo);
		}
	}
	rt->overridden.fbo_cache.clear();

	if (rt->fbo) {
		glDeleteFramebuffers(1, &rt->fbo);
		rt->fbo = 0;
	}

	if (rt->overridden.color.is_null()) {
		if (rt->texture.is_valid()) {
			Texture *tex = get_texture(rt->texture);
			tex->alloc_height = 0;
			tex->alloc_width = 0;
			tex->width = 0;
			tex->height = 0;
			tex->active = false;
			tex->render_target = nullptr;
			tex->is_render_target = false;
			tex->gl_set_filter(RS::CANVAS_ITEM_TEXTURE_FILTER_MAX);
			tex->gl_set_repeat(RS::CANVAS_ITEM_TEXTURE_REPEAT_MAX);
		}
	} else {
		Texture *tex = get_texture(rt->overridden.color);
		tex->render_target = nullptr;
		tex->is_render_target = false;
	}

	if (rt->overridden.color.is_valid()) {
		rt->overridden.color = RID();
	} else if (rt->color) {
		GLES::Utilities::get_singleton()->texture_free_data(rt->color);
		if (rt->texture.is_valid()) {
			Texture *tex = get_texture(rt->texture);
			tex->tex_id = 0;
		}
	}
	rt->color = 0;

	if (rt->overridden.depth.is_valid()) {
		rt->overridden.depth = RID();
	} else if (rt->depth) {
		GLES::Utilities::get_singleton()->texture_free_data(rt->depth);
	}
	rt->depth = 0;

	rt->overridden.velocity = RID();
	rt->overridden.is_overridden = false;

	if (rt->backbuffer_fbo != 0) {
		glDeleteFramebuffers(1, &rt->backbuffer_fbo);
		GLES::Utilities::get_singleton()->texture_free_data(rt->backbuffer);
		rt->backbuffer = 0;
		rt->backbuffer_fbo = 0;
	}
	if (rt->backbuffer_depth != 0) {
		GLES::Utilities::get_singleton()->texture_free_data(rt->backbuffer_depth);
		rt->backbuffer_depth = 0;
	}
	_render_target_clear_sdf(rt);
}

RID TextureStorage::render_target_create() {
	RenderTarget render_target;
	render_target.used_in_frame = false;
	render_target.clear_requested = false;

	Texture t;
	t.active = true;
	t.render_target = &render_target;
	t.is_render_target = true;

	render_target.texture = texture_owner.make_rid(t);
	_update_render_target(&render_target);
	return render_target_owner.make_rid(render_target);
}

void TextureStorage::render_target_free(RID p_rid) {
	RenderTarget *rt = render_target_owner.get_or_null(p_rid);
	_clear_render_target(rt);

	Texture *t = get_texture(rt->texture);
	if (t) {
		t->is_render_target = false;
		if (rt->overridden.color.is_null()) {
			texture_free(rt->texture);
		}
		//memdelete(t);
	}
	render_target_owner.free(p_rid);
}

void TextureStorage::render_target_set_position(RID p_render_target, int p_x, int p_y) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL(rt);

	rt->position = Point2i(p_x, p_y);
}

Point2i TextureStorage::render_target_get_position(RID p_render_target) const {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, Point2i());

	return rt->position;
};

void TextureStorage::render_target_set_size(RID p_render_target, int p_width, int p_height, uint32_t p_view_count) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL(rt);

	if (p_width == rt->size.x && p_height == rt->size.y && p_view_count == rt->view_count) {
		return;
	}
	if (rt->overridden.color.is_valid()) {
		return;
	}

	_clear_render_target(rt);

	rt->size = Size2i(p_width, p_height);
	rt->view_count = p_view_count;

	_update_render_target(rt);
}

// TODO: convert to Size2i internally
Size2i TextureStorage::render_target_get_size(RID p_render_target) const {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, Size2i());

	return rt->size;
}

void TextureStorage::render_target_set_override(RID p_render_target, RID p_color_texture, RID p_depth_texture, RID p_velocity_texture) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL(rt);
	ERR_FAIL_COND(rt->direct_to_screen);

	rt->overridden.velocity = p_velocity_texture;

	if (rt->overridden.color == p_color_texture && rt->overridden.depth == p_depth_texture) {
		return;
	}

	if (p_color_texture.is_null() && p_depth_texture.is_null()) {
		_clear_render_target(rt);
		_update_render_target(rt);
		return;
	}

	if (!rt->overridden.is_overridden) {
		_clear_render_target(rt);
	}

	rt->overridden.color = p_color_texture;
	rt->overridden.depth = p_depth_texture;
	rt->overridden.is_overridden = true;

	uint32_t hash_key = hash_murmur3_one_64(p_color_texture.get_id());
	hash_key = hash_murmur3_one_64(p_depth_texture.get_id(), hash_key);
	hash_key = hash_fmix32(hash_key);

	RBMap<uint32_t, RenderTarget::RTOverridden::FBOCacheEntry>::Element *cache;
	if ((cache = rt->overridden.fbo_cache.find(hash_key)) != nullptr) {
		rt->fbo = cache->get().fbo;
		rt->color = cache->get().color;
		rt->depth = cache->get().depth;
		rt->size = cache->get().size;
		rt->texture = p_color_texture;
		return;
	}

	_update_render_target(rt);

	RenderTarget::RTOverridden::FBOCacheEntry new_entry;
	new_entry.fbo = rt->fbo;
	new_entry.color = rt->color;
	new_entry.depth = rt->depth;
	new_entry.size = rt->size;
	// Keep track of any textures we had to allocate because they weren't overridden.
	if (p_color_texture.is_null()) {
		new_entry.allocated_textures.push_back(rt->color);
	}
	if (p_depth_texture.is_null()) {
		new_entry.allocated_textures.push_back(rt->depth);
	}
	rt->overridden.fbo_cache.insert(hash_key, new_entry);
}

RID TextureStorage::render_target_get_override_color(RID p_render_target) const {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, RID());

	return rt->overridden.color;
}

RID TextureStorage::render_target_get_override_depth(RID p_render_target) const {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, RID());

	return rt->overridden.depth;
}

RID TextureStorage::render_target_get_override_velocity(RID p_render_target) const {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, RID());

	return rt->overridden.velocity;
}

RID TextureStorage::render_target_get_texture(RID p_render_target) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, RID());

	if (rt->overridden.color.is_valid()) {
		return rt->overridden.color;
	}

	return rt->texture;
}

void TextureStorage::render_target_set_transparent(RID p_render_target, bool p_transparent) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL(rt);

	rt->is_transparent = p_transparent;

	if (rt->overridden.color.is_null()) {
		_clear_render_target(rt);
		_update_render_target(rt);
	}
}

bool TextureStorage::render_target_get_transparent(RID p_render_target) const {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, false);

	return rt->is_transparent;
}

void TextureStorage::render_target_set_direct_to_screen(RID p_render_target, bool p_direct_to_screen) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL(rt);

	if (p_direct_to_screen == rt->direct_to_screen) {
		return;
	}
	// When setting DIRECT_TO_SCREEN, you need to clear before the value is set, but allocate after as
	// those functions change how they operate depending on the value of DIRECT_TO_SCREEN
	_clear_render_target(rt);
	rt->direct_to_screen = p_direct_to_screen;
	if (rt->direct_to_screen) {
		rt->overridden.color = RID();
		rt->overridden.depth = RID();
		rt->overridden.velocity = RID();
	}
	_update_render_target(rt);
}

bool TextureStorage::render_target_get_direct_to_screen(RID p_render_target) const {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, false);

	return rt->direct_to_screen;
}

bool TextureStorage::render_target_was_used(RID p_render_target) const {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, false);

	return rt->used_in_frame;
}

void TextureStorage::render_target_clear_used(RID p_render_target) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL(rt);

	rt->used_in_frame = false;
}

void TextureStorage::render_target_set_msaa(RID p_render_target, RS::ViewportMSAA p_msaa) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL(rt);
	ERR_FAIL_COND(rt->direct_to_screen);
	if (p_msaa == rt->msaa) {
		return;
	}

	WARN_PRINT("2D MSAA is not yet supported for GLES3.");

	_clear_render_target(rt);
	rt->msaa = p_msaa;
	_update_render_target(rt);
}

RS::ViewportMSAA TextureStorage::render_target_get_msaa(RID p_render_target) const {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, RS::VIEWPORT_MSAA_DISABLED);

	return rt->msaa;
}

void TextureStorage::render_target_set_use_hdr(RID p_render_target, bool p_use_hdr_2d) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL(rt);
	ERR_FAIL_COND(rt->direct_to_screen);
	if (p_use_hdr_2d == rt->hdr) {
		return;
	}

	_clear_render_target(rt);
	rt->hdr = p_use_hdr_2d;
	_update_render_target(rt);
}

bool TextureStorage::render_target_is_using_hdr(RID p_render_target) const {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, false);

	return rt->hdr;
}

GLuint TextureStorage::render_target_get_color_internal_format(RID p_render_target) const {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, GL_RGBA);

	return rt->color_internal_format;
}

GLuint TextureStorage::render_target_get_color_format(RID p_render_target) const {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, GL_RGBA);

	return rt->color_format;
}

GLuint TextureStorage::render_target_get_color_type(RID p_render_target) const {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, GL_UNSIGNED_BYTE);

	return rt->color_type;
}

uint32_t TextureStorage::render_target_get_color_format_size(RID p_render_target) const {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, 4);

	return rt->color_format_size;
}

void TextureStorage::render_target_request_clear(RID p_render_target, const Color &p_clear_color) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL(rt);
	rt->clear_requested = true;
	rt->clear_color = p_clear_color;
}

bool TextureStorage::render_target_is_clear_requested(RID p_render_target) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, false);
	return rt->clear_requested;
}
Color TextureStorage::render_target_get_clear_request_color(RID p_render_target) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, Color());
	return rt->clear_color;
}

void TextureStorage::render_target_disable_clear_request(RID p_render_target) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL(rt);
	rt->clear_requested = false;
}

GLuint TextureStorage::render_target_get_fbo(RID p_render_target) const {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, 0);

	return rt->fbo;
}

GLuint TextureStorage::render_target_get_color(RID p_render_target) const {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, 0);

	if (rt->overridden.color.is_valid()) {
		Texture *texture = get_texture(rt->overridden.color);
		ERR_FAIL_NULL_V(texture, 0);

		return texture->tex_id;
	} else {
		return rt->color;
	}
}

GLuint TextureStorage::render_target_get_depth(RID p_render_target) const {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, 0);

	if (rt->overridden.depth.is_valid()) {
		Texture *texture = get_texture(rt->overridden.depth);
		ERR_FAIL_NULL_V(texture, 0);

		return texture->tex_id;
	} else {
		return rt->depth;
	}
}

void TextureStorage::render_target_set_reattach_textures(RID p_render_target, bool p_reattach_textures) const {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL(rt);

	rt->reattach_textures = p_reattach_textures;
}

bool TextureStorage::render_target_is_reattach_textures(RID p_render_target) const {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, false);

	return rt->reattach_textures;
}

void TextureStorage::render_target_set_sdf_size_and_scale(RID p_render_target, RS::ViewportSDFOversize p_size, RS::ViewportSDFScale p_scale) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL(rt);
	if (rt->sdf_oversize == p_size && rt->sdf_scale == p_scale) {
		return;
	}

	rt->sdf_oversize = p_size;
	rt->sdf_scale = p_scale;

	_render_target_clear_sdf(rt);
}

Rect2i TextureStorage::_render_target_get_sdf_rect(const RenderTarget *rt) const {
	Size2i margin;
	int scale;
	switch (rt->sdf_oversize) {
		case RS::VIEWPORT_SDF_OVERSIZE_100_PERCENT: {
			scale = 100;
		} break;
		case RS::VIEWPORT_SDF_OVERSIZE_120_PERCENT: {
			scale = 120;
		} break;
		case RS::VIEWPORT_SDF_OVERSIZE_150_PERCENT: {
			scale = 150;
		} break;
		case RS::VIEWPORT_SDF_OVERSIZE_200_PERCENT: {
			scale = 200;
		} break;
		default: {
			ERR_PRINT("Invalid viewport SDF oversize, defaulting to 100%.");
			scale = 100;
		} break;
	}

	margin = (rt->size * scale / 100) - rt->size;

	Rect2i r(Vector2i(), rt->size);
	r.position -= margin;
	r.size += margin * 2;

	return r;
}

Rect2i TextureStorage::render_target_get_sdf_rect(RID p_render_target) const {
	const RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, Rect2i());

	return _render_target_get_sdf_rect(rt);
}

void TextureStorage::render_target_mark_sdf_enabled(RID p_render_target, bool p_enabled) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL(rt);

	rt->sdf_enabled = p_enabled;
}

bool TextureStorage::render_target_is_sdf_enabled(RID p_render_target) const {
	const RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, false);

	return rt->sdf_enabled;
}

GLuint TextureStorage::render_target_get_sdf_texture(RID p_render_target) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, 0);
	if (rt->sdf_texture_read == 0) {
		Texture *texture = texture_owner.get_or_null(default_gl_textures[DEFAULT_GL_TEXTURE_BLACK]);
		return texture->tex_id;
	}

	return rt->sdf_texture_read;
}

void TextureStorage::_render_target_clear_sdf(RenderTarget *rt) {
	if (rt->sdf_texture_write_fb != 0) {
		GLES::Utilities::get_singleton()->texture_free_data(rt->sdf_texture_read);
		GLES::Utilities::get_singleton()->texture_free_data(rt->sdf_texture_write);
		GLES::Utilities::get_singleton()->texture_free_data(rt->sdf_texture_process[0]);
		GLES::Utilities::get_singleton()->texture_free_data(rt->sdf_texture_process[1]);

		glDeleteFramebuffers(1, &rt->sdf_texture_write_fb);
		rt->sdf_texture_read = 0;
		rt->sdf_texture_write = 0;
		rt->sdf_texture_process[0] = 0;
		rt->sdf_texture_process[1] = 0;
		rt->sdf_texture_write_fb = 0;
	}
}

GLuint TextureStorage::render_target_get_sdf_framebuffer(RID p_render_target) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, 0);

	if (rt->sdf_texture_write_fb == 0) {
		_render_target_allocate_sdf(rt);
	}

	return rt->sdf_texture_write_fb;
}

void TextureStorage::render_target_copy_to_back_buffer(RID p_render_target, const Rect2i &p_region, bool p_gen_mipmaps) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL(rt);
	ERR_FAIL_COND(rt->direct_to_screen);

	if (rt->backbuffer_fbo == 0) {
		_create_render_target_backbuffer(rt);
	}

	Rect2i region;
	if (p_region == Rect2i()) {
		region.size = rt->size;
	} else {
		region = Rect2i(Size2i(), rt->size).intersection(p_region);
		if (region.size == Size2i()) {
			return; //nothing to do
		}
	}

	glDisable(GL_BLEND);
	// Single texture copy for backbuffer.
	glBindFramebuffer(GL_FRAMEBUFFER, rt->backbuffer_fbo);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, rt->color);
	Rect2 normalized_region = region;
	normalized_region.position = normalized_region.position / Size2(rt->size);
	normalized_region.size = normalized_region.size / Size2(rt->size);
	GLES::CopyEffects::get_singleton()->copy_to_and_from_rect(normalized_region);

	if (p_gen_mipmaps) {
		GLES::CopyEffects::get_singleton()->gaussian_blur(rt->backbuffer, rt->mipmap_count, region, rt->size);
		glBindFramebuffer(GL_FRAMEBUFFER, rt->backbuffer_fbo);
	}

	glEnable(GL_BLEND); // 2D starts with blend enabled.
}

void TextureStorage::render_target_clear_back_buffer(RID p_render_target, const Rect2i &p_region, const Color &p_color) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL(rt);
	ERR_FAIL_COND(rt->direct_to_screen);

	if (rt->backbuffer_fbo == 0) {
		_create_render_target_backbuffer(rt);
	}

	Rect2i region;
	if (p_region == Rect2i()) {
		// Just do a full screen clear;
		glBindFramebuffer(GL_FRAMEBUFFER, rt->backbuffer_fbo);
		glClearColor(p_color.r, p_color.g, p_color.b, p_color.a);
		glClear(GL_COLOR_BUFFER_BIT);
	} else {
		region = Rect2i(Size2i(), rt->size).intersection(p_region);
		if (region.size == Size2i()) {
			return; //nothing to do
		}
		glBindFramebuffer(GL_FRAMEBUFFER, rt->backbuffer_fbo);
		GLES::CopyEffects::get_singleton()->set_color(p_color, region);
	}
}

void TextureStorage::render_target_gen_back_buffer_mipmaps(RID p_render_target, const Rect2i &p_region) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL(rt);

	if (rt->backbuffer_fbo == 0) {
		_create_render_target_backbuffer(rt);
	}

	Rect2i region;
	if (p_region == Rect2i()) {
		region.size = rt->size;
	} else {
		region = Rect2i(Size2i(), rt->size).intersection(p_region);
		if (region.size == Size2i()) {
			return; //nothing to do
		}
	}
	glDisable(GL_BLEND);
	GLES::CopyEffects::get_singleton()->gaussian_blur(rt->backbuffer, rt->mipmap_count, region, rt->size);
	glEnable(GL_BLEND); // 2D starts with blend enabled.

	glBindFramebuffer(GL_FRAMEBUFFER, rt->backbuffer_fbo);
}

#endif // defined(GLES3_ENABLED) || defined(GLES2_ENABLED)
