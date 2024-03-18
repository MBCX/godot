/**************************************************************************/
/*  texture_storage_gles2.cpp                                             */
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

#if defined(GLES2_ENABLED)

#include "texture_storage_gles2.h"

#include "drivers/gles_common/rasterizer_gles.h"
#include "drivers/gles_common/storage/config.h"

using namespace GLES;

static const GLenum _cube_side_enum[6] = {
	GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
	GL_TEXTURE_CUBE_MAP_POSITIVE_X,
	GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
	GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
	GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
	GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
};

void TextureStorageGLES2::_texture_set_data(RID p_texture, const Ref<Image> &p_image, int p_layer, bool initialize) {
	Texture *texture = texture_owner.get_or_null(p_texture);

	ERR_FAIL_NULL(texture);
	/*
	if (texture->target == GL_TEXTURE_3D) {
		// Target is set to a 3D texture or array texture, exit early to avoid spamming errors
		return;
	}
	*/
	ERR_FAIL_COND(!texture->active);
	ERR_FAIL_COND(texture->is_render_target);
	ERR_FAIL_COND(p_image.is_null());
	ERR_FAIL_COND(texture->format != p_image->get_format());

	ERR_FAIL_COND(!p_image->get_width());
	ERR_FAIL_COND(!p_image->get_height());

	GLenum type;
	GLenum format;
	GLenum internal_format;
	bool compressed = false;

	Image::Format real_format;
	Ref<Image> img = _get_gl_image_and_format(p_image, p_image->get_format(), real_format, format, internal_format, type, compressed, texture->resize_to_po2);
	ERR_FAIL_COND(img.is_null());
	if (texture->resize_to_po2) {
		if (p_image->is_compressed()) {
			ERR_PRINT("Texture '" + texture->path + "' is required to be a power of 2 because it uses either mipmaps or repeat, so it was decompressed. This will hurt performance and memory usage.");
		}

		if (img == p_image) {
			img = img->duplicate();
		}
		img->resize_to_po2(false);
	}

	GLenum blit_target = (texture->target == GL_TEXTURE_CUBE_MAP) ? _cube_side_enum[p_layer] : texture->target;

	Vector<uint8_t> read = img->get_data();

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(texture->target, texture->tex_id);

	int mipmaps = img->has_mipmaps() ? img->get_mipmap_count() + 1 : 1;

	// Set filtering and repeat state to default.
	if (mipmaps > 1) {
		texture->gl_set_filter(RS::CANVAS_ITEM_TEXTURE_FILTER_NEAREST_WITH_MIPMAPS);
	} else {
		texture->gl_set_filter(RS::CANVAS_ITEM_TEXTURE_FILTER_NEAREST);
	}

	texture->gl_set_repeat(RS::CANVAS_ITEM_TEXTURE_REPEAT_ENABLED);

	int w = img->get_width();
	int h = img->get_height();

	int tsize = 0;

	for (int i = 0; i < mipmaps; i++) {
		int size, ofs;
		img->get_mipmap_offset_and_size(i, ofs, size);

		if (compressed) {
			glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

			int bw = w;
			int bh = h;

			glCompressedTexImage2D(blit_target, i, internal_format, bw, bh, 0, size, &read[ofs]);
		} else {
			glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
			glTexImage2D(blit_target, i, internal_format, w, h, 0, format, type, &read[ofs]);
		}

		tsize += size;

		w = MAX(1, w >> 1);
		h = MAX(1, h >> 1);
	}

	texture->total_data_size = tsize;

	texture->stored_cube_sides |= (1 << p_layer);

	texture->mipmaps = mipmaps;
}

void TextureStorageGLES2::_update_render_target(RenderTarget *rt) {
	// do not allocate a render target with no size
	if (rt->size.x <= 0 || rt->size.y <= 0) {
		return;
	}

	// do not allocate a render target that is attached to the screen
	if (rt->direct_to_screen) {
		rt->fbo = system_fbo;
		return;
	}

	Config *config = Config::get_singleton();

	if (rt->hdr) {
		/*
		rt->color_internal_format = GL_RGBA16F;
		rt->color_format = GL_RGBA;
		rt->color_type = GL_FLOAT;
		rt->color_format_size = 8;
		rt->image_format = Image::FORMAT_RGBAF
		*/
	} else if (rt->is_transparent) {
		rt->color_internal_format = GL_RGBA;
		rt->color_format = GL_RGBA;
		rt->color_type = GL_UNSIGNED_BYTE;
		rt->color_format_size = 4;
		rt->image_format = Image::FORMAT_RGBA8;
	} else {
		rt->color_internal_format = GL_RGBA;
		rt->color_format = GL_RGBA;
		rt->color_type = GL_UNSIGNED_BYTE;
		rt->color_format_size = 4;
		rt->image_format = Image::FORMAT_RGBA8;
	}

	glDisable(GL_SCISSOR_TEST);
	glColorMask(1, 1, 1, 1);
	glDepthMask(GL_FALSE);

	{
		Texture *texture;
		bool use_multiview = rt->view_count > 1 && config->multiview_supported;
		GLenum texture_target = GL_TEXTURE_2D;

		/* Front FBO */

		glGenFramebuffers(1, &rt->fbo);
		glBindFramebuffer(GL_FRAMEBUFFER, rt->fbo);

		// color
		if (rt->overridden.color.is_valid()) {
			texture = get_texture(rt->overridden.color);
			ERR_FAIL_NULL(texture);

			rt->color = texture->tex_id;
			rt->size = Size2i(texture->width, texture->height);
		} else {
			texture = get_texture(rt->texture);
			ERR_FAIL_NULL(texture);

			glGenTextures(1, &rt->color);
			glBindTexture(texture_target, rt->color);

			if (use_multiview) {
				//TODO: add if we ever want multiview glTexImage3D(texture_target, 0, rt->color_internal_format, rt->size.x, rt->size.y, rt->view_count, 0, rt->color_format, rt->color_type, nullptr);
			} else {
				glTexImage2D(texture_target, 0, rt->color_internal_format, rt->size.x, rt->size.y, 0, rt->color_format, rt->color_type, nullptr);
			}

			texture->gl_set_filter(RS::CANVAS_ITEM_TEXTURE_FILTER_NEAREST);
			texture->gl_set_repeat(RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED);

			GLES::Utilities::get_singleton()->texture_allocated_data(rt->color, rt->size.x * rt->size.y * rt->view_count * rt->color_format_size, "Render target color texture");
		}

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture_target, rt->color, 0);

		// depth
		if (rt->overridden.depth.is_valid()) {
			texture = get_texture(rt->overridden.depth);
			ERR_FAIL_NULL(texture);

			rt->depth = texture->tex_id;
		} else {
			glGenTextures(1, &rt->depth);
			glBindTexture(texture_target, rt->depth);

			if (use_multiview) {
				//TODO: add if we ever want multiview glTexImage3D(texture_target, 0, GL_DEPTH_COMPONENT, rt->size.x, rt->size.y, rt->view_count, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
			} else {
				glTexImage2D(texture_target, 0, GL_DEPTH_COMPONENT, rt->size.x, rt->size.y, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
			}

			glTexParameteri(texture_target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(texture_target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(texture_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(texture_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

			GLES::Utilities::get_singleton()->texture_allocated_data(rt->depth, rt->size.x * rt->size.y * rt->view_count * 3, "Render target depth texture");
		}

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, texture_target, rt->depth, 0);

		GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (status != GL_FRAMEBUFFER_COMPLETE) {
			glDeleteFramebuffers(1, &rt->fbo);
			if (rt->overridden.color.is_null()) {
				GLES::Utilities::get_singleton()->texture_free_data(rt->color);
			}
			if (rt->overridden.depth.is_null()) {
				GLES::Utilities::get_singleton()->texture_free_data(rt->depth);
			}
			rt->fbo = 0;
			rt->size.x = 0;
			rt->size.y = 0;
			rt->color = 0;
			rt->depth = 0;
			if (rt->overridden.color.is_null()) {
				texture->tex_id = 0;
				texture->active = false;
			}
			WARN_PRINT("Could not create render target, status: " + get_framebuffer_error(status));
			return;
		}

		texture->is_render_target = true;
		texture->render_target = rt;
		if (rt->overridden.color.is_null()) {
			texture->format = rt->image_format;
			texture->real_format = rt->image_format;
			texture->target = texture_target;
			if (rt->view_count > 1 && config->multiview_supported) {
				texture->type = Texture::TYPE_LAYERED;
				texture->layers = rt->view_count;
			} else {
				texture->type = Texture::TYPE_2D;
				texture->layers = 1;
			}
			texture->gl_format_cache = rt->color_format;
			texture->gl_type_cache = GL_UNSIGNED_BYTE;
			texture->gl_internal_format_cache = rt->color_internal_format;
			texture->tex_id = rt->color;
			texture->width = rt->size.x;
			texture->alloc_width = rt->size.x;
			texture->height = rt->size.y;
			texture->alloc_height = rt->size.y;
			texture->active = true;
		}
	}

	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT);
	glBindFramebuffer(GL_FRAMEBUFFER, system_fbo);
}

Ref<Image> TextureStorageGLES2::_get_gl_image_and_format(const Ref<Image> &p_image, Image::Format p_format, Image::Format &r_real_format, GLenum &r_gl_format, GLenum &r_gl_internal_format, GLenum &r_gl_type, bool &r_compressed, bool p_force_decompress) const {
	GLES::Config *config = GLES::Config::get_singleton();
	r_gl_format = 0;
	Ref<Image> image = p_image;
	r_compressed = false;
	r_real_format = p_format;

	bool need_decompress = false;
	bool decompress_ra_to_rg = false;

	switch (p_format) {
		case Image::FORMAT_L8: {
#ifdef GLES_OVER_GL
			if (RasterizerGLES::is_gles_over_gl()) {
				r_gl_internal_format = GL_RED;
				r_gl_format = GL_RED;
				r_gl_type = GL_UNSIGNED_BYTE;
			} else
#endif
			{
				r_gl_internal_format = GL_LUMINANCE;
				r_gl_format = GL_LUMINANCE;
				r_gl_type = GL_UNSIGNED_BYTE;
			}
		} break;
		case Image::FORMAT_LA8: {
#ifdef GLES_OVER_GL

			if (RasterizerGLES::is_gles_over_gl()) {
				r_gl_internal_format = GL_RED;
				r_gl_format = GL_RED;
				r_gl_type = GL_UNSIGNED_BYTE;
			} else
#endif
			{
				r_gl_internal_format = GL_LUMINANCE_ALPHA;
				r_gl_format = GL_LUMINANCE_ALPHA;
				r_gl_type = GL_UNSIGNED_BYTE;
			}
		} break;
		case Image::FORMAT_R8: {
			r_gl_internal_format = GL_ALPHA;
			r_gl_format = GL_ALPHA;
			r_gl_type = GL_UNSIGNED_BYTE;

		} break;
		case Image::FORMAT_RG8: {
			ERR_PRINT("RG8 Format is not supported by GLES2, converting to RGB.");
			if (image.is_valid())
				image->convert(Image::FORMAT_RGB8);
			r_real_format = Image::FORMAT_RGB8;
			r_gl_internal_format = GL_RGB;
			r_gl_format = GL_RGB;
			r_gl_type = GL_UNSIGNED_BYTE;

		} break;
		case Image::FORMAT_RGB8: {
			r_gl_internal_format = GL_RGB;
			r_gl_format = GL_RGB;
			r_gl_type = GL_UNSIGNED_BYTE;

		} break;
		case Image::FORMAT_RGBA8: {
			r_gl_format = GL_RGBA;
			r_gl_internal_format = GL_RGBA;
			r_gl_type = GL_UNSIGNED_BYTE;

		} break;
		case Image::FORMAT_RGBA4444: {
			r_gl_internal_format = GL_RGBA;
			r_gl_format = GL_RGBA;
			r_gl_type = GL_UNSIGNED_SHORT_4_4_4_4;

		} break;
		case Image::FORMAT_RF: {
			r_gl_internal_format = GL_ALPHA;
			r_gl_format = GL_ALPHA;
			r_gl_type = GL_UNSIGNED_BYTE;

		} break;
		case Image::FORMAT_RGF: {
			ERR_PRINT("RGF Format is not supported by GLES2, converting to RGB8.");
			if (image.is_valid())
				image->convert(Image::FORMAT_RGB8);
			r_real_format = Image::FORMAT_RGB8;
			r_gl_internal_format = GL_RGB;
			r_gl_format = GL_RGB;
			r_gl_type = GL_UNSIGNED_BYTE;

		} break;
		case Image::FORMAT_RGBF: {
			ERR_PRINT("RGB float texture not supported by GLES2, converting to RGB8.");
			if (image.is_valid())
				image->convert(Image::FORMAT_RGB8);
			r_real_format = Image::FORMAT_RGB8;
			r_gl_internal_format = GL_RGB;
			r_gl_format = GL_RGB;
			r_gl_type = GL_UNSIGNED_BYTE;

		} break;
		case Image::FORMAT_RGBAF: {
			ERR_PRINT("RGBA float texture not supported, converting to RGBA8.");
			if (image.is_valid())
				image->convert(Image::FORMAT_RGBA8);
			r_real_format = Image::FORMAT_RGBA8;
			r_gl_internal_format = GL_RGBA;
			r_gl_format = GL_RGBA;
			r_gl_type = GL_UNSIGNED_BYTE;

		} break;
		case Image::FORMAT_RH: {
			need_decompress = true;

		} break;
		case Image::FORMAT_RGH: {
			need_decompress = true;

		} break;
		case Image::FORMAT_RGBH: {
			need_decompress = true;

		} break;
		case Image::FORMAT_RGBAH: {
			need_decompress = true;

		} break;
		case Image::FORMAT_RGBE9995: {
			r_gl_internal_format = GL_RGB;
			r_gl_format = GL_RGB;
			r_gl_type = GL_UNSIGNED_BYTE;

			if (image.is_valid())
				image = image->rgbe_to_srgb();

			return image;

		} break;
		case Image::FORMAT_DXT1: {
			if (config->s3tc_supported) {
				r_gl_internal_format = _EXT_COMPRESSED_RGBA_S3TC_DXT1_EXT;
				r_gl_format = GL_RGBA;
				r_gl_type = GL_UNSIGNED_BYTE;
				r_compressed = true;
			} else {
				need_decompress = true;
			}
		} break;
		case Image::FORMAT_DXT3: {
			if (config->s3tc_supported) {
				r_gl_internal_format = _EXT_COMPRESSED_RGBA_S3TC_DXT3_EXT;
				r_gl_format = GL_RGBA;
				r_gl_type = GL_UNSIGNED_BYTE;
				r_compressed = true;
			} else {
				need_decompress = true;
			}
		} break;
		case Image::FORMAT_DXT5: {
			if (config->s3tc_supported) {
				r_gl_internal_format = _EXT_COMPRESSED_RGBA_S3TC_DXT5_EXT;
				r_gl_format = GL_RGBA;
				r_gl_type = GL_UNSIGNED_BYTE;
				r_compressed = true;
			} else {
				need_decompress = true;
			}
		} break;
		case Image::FORMAT_RGTC_R: {
			if (config->rgtc_supported) {
				r_gl_internal_format = _EXT_COMPRESSED_RED_RGTC1_EXT;
				r_gl_format = GL_RGBA;
				r_gl_type = GL_UNSIGNED_BYTE;
				r_compressed = true;
			} else {
				need_decompress = true;
			}
		} break;
		case Image::FORMAT_RGTC_RG: {
			if (config->rgtc_supported) {
				r_gl_internal_format = _EXT_COMPRESSED_RED_GREEN_RGTC2_EXT;
				r_gl_format = GL_RGBA;
				r_gl_type = GL_UNSIGNED_BYTE;
				r_compressed = true;
			} else {
				need_decompress = true;
			}
		} break;
		case Image::FORMAT_BPTC_RGBA: {
			if (config->bptc_supported) {
				r_gl_internal_format = _EXT_COMPRESSED_RGBA_BPTC_UNORM;
				r_gl_format = GL_RGBA;
				r_gl_type = GL_UNSIGNED_BYTE;
				r_compressed = true;
			} else {
				need_decompress = true;
			}
		} break;
		case Image::FORMAT_BPTC_RGBF: {
			if (config->bptc_supported) {
				r_gl_internal_format = _EXT_COMPRESSED_RGB_BPTC_SIGNED_FLOAT;
				r_gl_format = GL_RGB;
				r_gl_type = GL_FLOAT;
				r_compressed = true;
			} else {
				need_decompress = true;
			}
		} break;
		case Image::FORMAT_BPTC_RGBFU: {
			if (config->bptc_supported) {
				r_gl_internal_format = _EXT_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT;
				r_gl_format = GL_RGB;
				r_gl_type = GL_FLOAT;
				r_compressed = true;
			} else {
				need_decompress = true;
			}
		} break;
		case Image::FORMAT_ETC2_R11: {
#ifdef GLES3_ENABLED
			if (config->etc2_supported) {
				r_gl_internal_format = _EXT_COMPRESSED_R11_EAC;
				r_gl_format = GL_RED;
				r_gl_type = GL_UNSIGNED_BYTE;
				r_compressed = true;

			} else
#endif
			{
				need_decompress = true;
			}
		} break;
		case Image::FORMAT_ETC2_R11S: {
#ifdef GLES3_ENABLED

			if (config->etc2_supported) {
				r_gl_internal_format = _EXT_COMPRESSED_SIGNED_R11_EAC;
				r_gl_format = GL_RED;
				r_gl_type = GL_UNSIGNED_BYTE;
				r_compressed = true;

			} else
#endif
			{
				need_decompress = true;
			}
		} break;
		case Image::FORMAT_ETC2_RG11: {
			need_decompress = true;

		} break;
		case Image::FORMAT_ETC2_RG11S: {
			need_decompress = true;

		} break;
		case Image::FORMAT_ETC2_RGB8: {
			need_decompress = true;
		} break;
		case Image::FORMAT_ETC2_RGBA8: {
			need_decompress = true;
		} break;
		case Image::FORMAT_ETC2_RGB8A1: {
			need_decompress = true;
		} break;
		case Image::FORMAT_ETC2_RA_AS_RG: {
			need_decompress = true;

			decompress_ra_to_rg = true;
		} break;
		case Image::FORMAT_DXT5_RA_AS_RG: {
			need_decompress = true;

			decompress_ra_to_rg = true;
		} break;
		case Image::FORMAT_ASTC_4x4: {
			if (config->astc_supported) {
				r_gl_internal_format = _EXT_COMPRESSED_RGBA_ASTC_4x4_KHR;
				r_gl_format = GL_RGBA;
				r_gl_type = GL_UNSIGNED_BYTE;
				r_compressed = true;

			} else {
				need_decompress = true;
			}
		} break;
		case Image::FORMAT_ASTC_4x4_HDR: {
			if (config->astc_hdr_supported) {
				r_gl_internal_format = _EXT_COMPRESSED_RGBA_ASTC_4x4_KHR;
				r_gl_format = GL_RGBA;
				r_gl_type = GL_UNSIGNED_BYTE;
				r_compressed = true;

			} else {
				need_decompress = true;
			}
		} break;
		case Image::FORMAT_ASTC_8x8: {
			if (config->astc_supported) {
				r_gl_internal_format = _EXT_COMPRESSED_RGBA_ASTC_8x8_KHR;
				r_gl_format = GL_RGBA;
				r_gl_type = GL_UNSIGNED_BYTE;
				r_compressed = true;

			} else {
				need_decompress = true;
			}
		} break;
		case Image::FORMAT_ASTC_8x8_HDR: {
			if (config->astc_hdr_supported) {
				r_gl_internal_format = _EXT_COMPRESSED_RGBA_ASTC_8x8_KHR;
				r_gl_format = GL_RGBA;
				r_gl_type = GL_UNSIGNED_BYTE;
				r_compressed = true;

			} else {
				need_decompress = true;
			}
		} break;
		default: {
			ERR_FAIL_V_MSG(Ref<Image>(), "The image format " + itos(p_format) + " is not supported by the GL Compatibility rendering backend.");
		}
	}

	if (need_decompress || p_force_decompress) {
		if (!image.is_null()) {
			image = image->duplicate();
			image->decompress();
			ERR_FAIL_COND_V(image->is_compressed(), image);
			if (decompress_ra_to_rg) {
				image->convert_ra_rgba8_to_rg();
				image->convert(Image::FORMAT_RG8);
			}
			switch (image->get_format()) {
				case Image::FORMAT_RG8: {
					r_gl_format = GL_RGB;
					r_gl_internal_format = GL_RGB;
					r_gl_type = GL_UNSIGNED_BYTE;
					r_real_format = Image::FORMAT_RG8;
					r_compressed = false;
				} break;
				case Image::FORMAT_RGB8: {
					r_gl_format = GL_RGB;
					r_gl_internal_format = GL_RGB;
					r_gl_type = GL_UNSIGNED_BYTE;
					r_real_format = Image::FORMAT_RGB8;
					r_compressed = false;
				} break;
				case Image::FORMAT_RGBA8: {
					r_gl_format = GL_RGBA;
					r_gl_internal_format = GL_RGBA;
					r_gl_type = GL_UNSIGNED_BYTE;
					r_real_format = Image::FORMAT_RGBA8;
					r_compressed = false;
				} break;
				default: {
					image->convert(Image::FORMAT_RGBA8);
					r_gl_format = GL_RGBA;
					r_gl_internal_format = GL_RGBA;
					r_gl_type = GL_UNSIGNED_BYTE;
					r_real_format = Image::FORMAT_RGBA8;
					r_compressed = false;
				} break;
			}
		}

		return image;
	}

	return p_image;
}

RID TextureStorageGLES2::texture_create_external(GLES::Texture::Type p_type, Image::Format p_format, unsigned int p_image, int p_width, int p_height, int p_depth, int p_layers, RS::TextureLayeredType p_layered_type) {
	Texture texture;
	texture.active = true;
	texture.is_external = true;
	texture.type = p_type;

	switch (p_type) {
		case Texture::TYPE_2D: {
			texture.target = GL_TEXTURE_2D;
		} break;
		case Texture::TYPE_3D: {
			ERR_PRINT("The GLES2 driver doesn't support 3D textures.");
			//texture.target = GL_TEXTURE_3D;
		} break;
		case Texture::TYPE_LAYERED: {
			ERR_PRINT("2D Texture Arrays are not supported in the GLES2 backend.");
			texture.target = GL_TEXTURE_2D;
		} break;
	}

	texture.real_format = texture.format = p_format;
	texture.tex_id = p_image;
	texture.alloc_width = texture.width = p_width;
	texture.alloc_height = texture.height = p_height;
	texture.depth = p_depth;
	texture.layers = p_layers;
	texture.layered_type = p_layered_type;

	return texture_owner.make_rid(texture);
}

void TextureStorageGLES2::texture_3d_initialize(RID p_texture, Image::Format, int p_width, int p_height, int p_depth, bool p_mipmaps, const Vector<Ref<Image>> &p_data) {
	WARN_PRINT("Trying to initialize a 3D texture with the GLES2 driver, which doesn't support 3D Textures!");
	//texture_owner.initialize_rid(p_texture, Texture());
}

void TextureStorageGLES2::texture_3d_update(RID p_texture, const Vector<Ref<Image>> &p_data) {
	WARN_PRINT("Trying to update a 3D texture with the GLES2 driver, which doesn't support 3D Textures!");
};
void TextureStorageGLES2::texture_3d_placeholder_initialize(RID p_texture) {
	WARN_PRINT("Trying to initialize a placeholder 3D texture with the GLES2 driver, which doesn't support 3D Textures!");
};
Vector<Ref<Image>> TextureStorageGLES2::texture_3d_get(RID p_texture) const {
	ERR_FAIL_V_MSG(Vector<Ref<Image>>(), "Trying to get a 3D texture with the GLES2 driver, which doesn't support 3D Textures!");
};

void TextureStorageGLES2::check_backbuffer(RenderTarget *rt, const bool uses_screen_texture, const bool uses_depth_texture) {
	if (rt->backbuffer != 0 && rt->backbuffer_depth != 0) {
		return;
	}

	Config *config = Config::get_singleton();
	bool use_multiview = rt->view_count > 1 && config->multiview_supported;
	GLenum texture_target = GL_TEXTURE_2D;
	if (rt->backbuffer_fbo == 0) {
		glGenFramebuffers(1, &rt->backbuffer_fbo);
	}
	glBindFramebuffer(GL_FRAMEBUFFER, rt->backbuffer_fbo);
	if (rt->backbuffer == 0 && uses_screen_texture) {
		glGenTextures(1, &rt->backbuffer);
		glBindTexture(texture_target, rt->backbuffer);
		if (use_multiview) {
			//TODO: add if we ever want multiview glTexImage3D(texture_target, 0, rt->color_internal_format, rt->size.x, rt->size.y, rt->view_count, 0, rt->color_format, rt->color_type, nullptr);
		} else {
			glTexImage2D(texture_target, 0, rt->color_internal_format, rt->size.x, rt->size.y, 0, rt->color_format, rt->color_type, nullptr);
		}
		Utilities::get_singleton()->texture_allocated_data(rt->backbuffer, rt->size.x * rt->size.y * rt->view_count * rt->color_format_size, "Render target backbuffer color texture (3D)");
		glTexParameteri(texture_target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(texture_target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(texture_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(texture_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rt->backbuffer, 0);
	}
	if (rt->backbuffer_depth == 0 && uses_depth_texture) {
		glGenTextures(1, &rt->backbuffer_depth);
		glBindTexture(texture_target, rt->backbuffer_depth);
		if (use_multiview) {
			//TODO: add if we ever want multiview glTexImage3D(texture_target, 0, GL_DEPTH_COMPONENT, rt->size.x, rt->size.y, rt->view_count, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
		} else {
			glTexImage2D(texture_target, 0, GL_DEPTH_COMPONENT, rt->size.x, rt->size.y, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
		}
		GLES::Utilities::get_singleton()->texture_allocated_data(rt->backbuffer_depth, rt->size.x * rt->size.y * rt->view_count * 3, "Render target backbuffer depth texture");

		glTexParameteri(texture_target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(texture_target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(texture_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(texture_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, rt->backbuffer_depth, 0);
	}
}

void TextureStorageGLES2::render_target_do_clear_request(RID p_render_target) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL(rt);
	if (!rt->clear_requested) {
		return;
	}
	glBindFramebuffer(GL_FRAMEBUFFER, rt->fbo);

	//glClearBufferfv(GL_COLOR, 0, rt->clear_color.components);
	glClearColor(rt->clear_color.components[0], rt->clear_color.components[1], rt->clear_color.components[2], rt->clear_color.components[3]);
	glClear(GL_COLOR_BUFFER_BIT);

	rt->clear_requested = false;
	glBindFramebuffer(GL_FRAMEBUFFER, system_fbo);
}

void TextureStorageGLES2::_render_target_allocate_sdf(RenderTarget *rt) {
	/*
	ERR_FAIL_COND(rt->sdf_texture_write_fb != 0);

	Size2i size = _render_target_get_sdf_rect(rt).size;

	glGenTextures(1, &rt->sdf_texture_write);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, rt->sdf_texture_write);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, size.width, size.height, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
	GLES::Utilities::get_singleton()->texture_allocated_data(rt->sdf_texture_write, size.width * size.height, "SDF texture");
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 1);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glGenFramebuffers(1, &rt->sdf_texture_write_fb);
	glBindFramebuffer(GL_FRAMEBUFFER, rt->sdf_texture_write_fb);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rt->sdf_texture_write, 0);

	int scale;
	switch (rt->sdf_scale) {
		case RS::VIEWPORT_SDF_SCALE_100_PERCENT: {
			scale = 100;
		} break;
		case RS::VIEWPORT_SDF_SCALE_50_PERCENT: {
			scale = 50;
		} break;
		case RS::VIEWPORT_SDF_SCALE_25_PERCENT: {
			scale = 25;
		} break;
		default: {
			scale = 100;
		} break;
	}

	rt->process_size = size * scale / 100;
	rt->process_size.x = MAX(rt->process_size.x, 1);
	rt->process_size.y = MAX(rt->process_size.y, 1);

	glGenTextures(2, rt->sdf_texture_process);
	glBindTexture(GL_TEXTURE_2D, rt->sdf_texture_process[0]);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16I, rt->process_size.width, rt->process_size.height, 0, GL_RG_INTEGER, GL_SHORT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 1);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	GLES::Utilities::get_singleton()->texture_allocated_data(rt->sdf_texture_process[0], rt->process_size.width * rt->process_size.height * 4, "SDF process texture[0]");

	glBindTexture(GL_TEXTURE_2D, rt->sdf_texture_process[1]);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16I, rt->process_size.width, rt->process_size.height, 0, GL_RG_INTEGER, GL_SHORT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 1);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	GLES::Utilities::get_singleton()->texture_allocated_data(rt->sdf_texture_process[1], rt->process_size.width * rt->process_size.height * 4, "SDF process texture[1]");

	glGenTextures(1, &rt->sdf_texture_read);
	glBindTexture(GL_TEXTURE_2D, rt->sdf_texture_read);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, rt->process_size.width, rt->process_size.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 1);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	GLES::Utilities::get_singleton()->texture_allocated_data(rt->sdf_texture_read, rt->process_size.width * rt->process_size.height * 4, "SDF texture (read)");
	*/
}

void TextureStorageGLES2::render_target_sdf_process(RID p_render_target) {
	/*
	CopyEffects *copy_effects = CopyEffects::get_singleton();

	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL(rt);
	ERR_FAIL_COND(rt->sdf_texture_write_fb == 0);

	Rect2i r = _render_target_get_sdf_rect(rt);

	Size2i size = r.size;
	int32_t shift = 0;

	bool shrink = false;

	switch (rt->sdf_scale) {
		case RS::VIEWPORT_SDF_SCALE_50_PERCENT: {
			size[0] >>= 1;
			size[1] >>= 1;
			shift = 1;
			shrink = true;
		} break;
		case RS::VIEWPORT_SDF_SCALE_25_PERCENT: {
			size[0] >>= 2;
			size[1] >>= 2;
			shift = 2;
			shrink = true;
		} break;
		default: {
		};
	}

	GLuint temp_fb;
	glGenFramebuffers(1, &temp_fb);
	glBindFramebuffer(GL_FRAMEBUFFER, temp_fb);

	// Load
	CanvasSdfShaderGLES3::ShaderVariant variant = shrink ? CanvasSdfShaderGLES3::MODE_LOAD_SHRINK : CanvasSdfShaderGLES3::MODE_LOAD;
	bool success = sdf_shader.shader.version_bind_shader(sdf_shader.shader_version, variant);
	if (!success) {
		return;
	}

	sdf_shader.shader.version_set_uniform(CanvasSdfShaderGLES3::BASE_SIZE, r.size, sdf_shader.shader_version, variant);
	sdf_shader.shader.version_set_uniform(CanvasSdfShaderGLES3::SIZE, size, sdf_shader.shader_version, variant);
	sdf_shader.shader.version_set_uniform(CanvasSdfShaderGLES3::STRIDE, 0, sdf_shader.shader_version, variant);
	sdf_shader.shader.version_set_uniform(CanvasSdfShaderGLES3::SHIFT, shift, sdf_shader.shader_version, variant);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, rt->sdf_texture_write);

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rt->sdf_texture_process[0], 0);
	glViewport(0, 0, size.width, size.height);
	glEnable(GL_SCISSOR_TEST);
	glScissor(0, 0, size.width, size.height);

	copy_effects->draw_screen_triangle();

	// Process

	int stride = nearest_power_of_2_templated(MAX(size.width, size.height) / 2);

	variant = CanvasSdfShaderGLES3::MODE_PROCESS;
	success = sdf_shader.shader.version_bind_shader(sdf_shader.shader_version, variant);
	if (!success) {
		return;
	}

	sdf_shader.shader.version_set_uniform(CanvasSdfShaderGLES3::BASE_SIZE, r.size, sdf_shader.shader_version, variant);
	sdf_shader.shader.version_set_uniform(CanvasSdfShaderGLES3::SIZE, size, sdf_shader.shader_version, variant);
	sdf_shader.shader.version_set_uniform(CanvasSdfShaderGLES3::STRIDE, stride, sdf_shader.shader_version, variant);
	sdf_shader.shader.version_set_uniform(CanvasSdfShaderGLES3::SHIFT, shift, sdf_shader.shader_version, variant);

	bool swap = false;

	//jumpflood
	while (stride > 0) {
		glBindTexture(GL_TEXTURE_2D, 0);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rt->sdf_texture_process[swap ? 0 : 1], 0);
		glBindTexture(GL_TEXTURE_2D, rt->sdf_texture_process[swap ? 1 : 0]);

		sdf_shader.shader.version_set_uniform(CanvasSdfShaderGLES3::STRIDE, stride, sdf_shader.shader_version, variant);

		copy_effects->draw_screen_triangle();

		stride /= 2;
		swap = !swap;
	}

	// Store
	variant = shrink ? CanvasSdfShaderGLES3::MODE_STORE_SHRINK : CanvasSdfShaderGLES3::MODE_STORE;
	success = sdf_shader.shader.version_bind_shader(sdf_shader.shader_version, variant);
	if (!success) {
		return;
	}

	sdf_shader.shader.version_set_uniform(CanvasSdfShaderGLES3::BASE_SIZE, r.size, sdf_shader.shader_version, variant);
	sdf_shader.shader.version_set_uniform(CanvasSdfShaderGLES3::SIZE, size, sdf_shader.shader_version, variant);
	sdf_shader.shader.version_set_uniform(CanvasSdfShaderGLES3::STRIDE, stride, sdf_shader.shader_version, variant);
	sdf_shader.shader.version_set_uniform(CanvasSdfShaderGLES3::SHIFT, shift, sdf_shader.shader_version, variant);

	glBindTexture(GL_TEXTURE_2D, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rt->sdf_texture_read, 0);
	glBindTexture(GL_TEXTURE_2D, rt->sdf_texture_process[swap ? 1 : 0]);

	copy_effects->draw_screen_triangle();

	glBindTexture(GL_TEXTURE_2D, 0);
	glBindFramebuffer(GL_FRAMEBUFFER, system_fbo);
	glDeleteFramebuffers(1, &temp_fb);
	glDisable(GL_SCISSOR_TEST);
	*/
}

#endif // defined(GLES2_ENABLED)