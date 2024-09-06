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

#include "texture_storage.h"

#ifdef GLES2_ENABLED

#include "config.h"
#include "drivers/gles2/rasterizer_gles2.h"
#include "drivers/gles2/rasterizer_canvas_gles2.h"
#include "core/config/project_settings.h"
#include "core/math/math_funcs.h"

using namespace GLES2;

TextureStorage *TextureStorage::singleton = nullptr;

TextureStorage *TextureStorage::get_singleton() {
    return singleton;
}

TextureStorage::TextureStorage() {
    singleton = this;

    { // Create default textures

        // White texture
        {
            Ref<Image> image = Image::create_empty(4, 4, true, Image::FORMAT_RGBA8);
            image->fill(Color(1, 1, 1, 1));

            default_gl_textures[DEFAULT_GL_TEXTURE_WHITE] = texture_allocate();
            texture_2d_initialize(default_gl_textures[DEFAULT_GL_TEXTURE_WHITE], image);
        }

        // Black texture
        {
            Ref<Image> image = Image::create_empty(4, 4, true, Image::FORMAT_RGBA8);
            image->fill(Color(0, 0, 0, 1));

            default_gl_textures[DEFAULT_GL_TEXTURE_BLACK] = texture_allocate();
            texture_2d_initialize(default_gl_textures[DEFAULT_GL_TEXTURE_BLACK], image);
        }

        // Transparent texture
        {
            Ref<Image> image = Image::create_empty(4, 4, true, Image::FORMAT_RGBA8);
            image->fill(Color(0, 0, 0, 0));

            default_gl_textures[DEFAULT_GL_TEXTURE_TRANSPARENT] = texture_allocate();
            texture_2d_initialize(default_gl_textures[DEFAULT_GL_TEXTURE_TRANSPARENT], image);
        }

        // Normal texture
        {
            Ref<Image> image = Image::create_empty(4, 4, true, Image::FORMAT_RGBA8);
            image->fill(Color(0.5, 0.5, 1, 1));

            default_gl_textures[DEFAULT_GL_TEXTURE_NORMAL] = texture_allocate();
            texture_2d_initialize(default_gl_textures[DEFAULT_GL_TEXTURE_NORMAL], image);
        }

        // MSDF texture
        {
            Ref<Image> image = Image::create_empty(4, 4, true, Image::FORMAT_RGBA8);
            image->fill(Color(0.5, 0.5, 0.5, 1));

            default_gl_textures[DEFAULT_GL_TEXTURE_MSDF] = texture_allocate();
            texture_2d_initialize(default_gl_textures[DEFAULT_GL_TEXTURE_MSDF], image);
        }
    }

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glFrontFace(GL_CW);
}

Ref<Image> TextureStorage::_get_gl_image_and_format(const Ref<Image> &p_image, Image::Format p_format, Image::Format &r_real_format, GLenum &r_gl_format, GLenum &r_gl_internal_format, GLenum &r_gl_type, bool &r_compressed, bool p_force_decompress) const {
    r_real_format = p_format;
    r_compressed = false;
    r_gl_format = 0;
    Ref<Image> image = p_image;
    switch (p_format) {
        case Image::FORMAT_L8: {
            r_gl_internal_format = GL_LUMINANCE;
            r_gl_format = GL_LUMINANCE;
            r_gl_type = GL_UNSIGNED_BYTE;
        } break;
        case Image::FORMAT_LA8: {
            r_gl_internal_format = GL_LUMINANCE_ALPHA;
            r_gl_format = GL_LUMINANCE_ALPHA;
            r_gl_type = GL_UNSIGNED_BYTE;
        } break;
        case Image::FORMAT_R8: {
            r_gl_internal_format = GL_ALPHA;
            r_gl_format = GL_ALPHA;
            r_gl_type = GL_UNSIGNED_BYTE;
        } break;
        case Image::FORMAT_RG8: {
            ERR_PRINT("RG texture not supported in GLES2");
            return Ref<Image>();
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
        case Image::FORMAT_RGB565: {
            r_gl_internal_format = GL_RGB;
            r_gl_format = GL_RGB;
            r_gl_type = GL_UNSIGNED_SHORT_5_6_5;
        } break;
        case Image::FORMAT_DXT1: {
            if (Config::get_singleton()->s3tc_supported) {
                r_gl_internal_format = _EXT_COMPRESSED_RGB_S3TC_DXT1_EXT;
                r_gl_format = GL_RGB;
                r_gl_type = GL_UNSIGNED_BYTE;
                r_compressed = true;
            } else {
                image = image->decompress();
                r_real_format = image->get_format();
                _get_gl_image_and_format(image, r_real_format, r_real_format, r_gl_format, r_gl_internal_format, r_gl_type, r_compressed, p_force_decompress);
            }
        } break;
        case Image::FORMAT_DXT3: {
            if (Config::get_singleton()->s3tc_supported) {
                r_gl_internal_format = _EXT_COMPRESSED_RGBA_S3TC_DXT3_EXT;
                r_gl_format = GL_RGBA;
                r_gl_type = GL_UNSIGNED_BYTE;
                r_compressed = true;
            } else {
                image = image->decompress();
                r_real_format = image->get_format();
                _get_gl_image_and_format(image, r_real_format, r_real_format, r_gl_format, r_gl_internal_format, r_gl_type, r_compressed, p_force_decompress);
            }
        } break;
        case Image::FORMAT_DXT5: {
            if (Config::get_singleton()->s3tc_supported) {
                r_gl_internal_format = _EXT_COMPRESSED_RGBA_S3TC_DXT5_EXT;
                r_gl_format = GL_RGBA;
                r_gl_type = GL_UNSIGNED_BYTE;
                r_compressed = true;
            } else {
                image = image->decompress();
                r_real_format = image->get_format();
                _get_gl_image_and_format(image, r_real_format, r_real_format, r_gl_format, r_gl_internal_format, r_gl_type, r_compressed, p_force_decompress);
            }
        } break;
        case Image::FORMAT_PVRTC2: {
            if (Config::get_singleton()->pvrtc_supported) {
                r_gl_internal_format = _EXT_COMPRESSED_RGB_PVRTC_2BPPV1_IMG;
                r_gl_format = GL_RGB;
                r_gl_type = GL_UNSIGNED_BYTE;
                r_compressed = true;
            } else {
                image = image->decompress();
                r_real_format = image->get_format();
                _get_gl_image_and_format(image, r_real_format, r_real_format, r_gl_format, r_gl_internal_format, r_gl_type, r_compressed, p_force_decompress);
            }
        } break;
        case Image::FORMAT_PVRTC2A: {
            if (Config::get_singleton()->pvrtc_supported) {
                r_gl_internal_format = _EXT_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG;
                r_gl_format = GL_RGBA;
                r_gl_type = GL_UNSIGNED_BYTE;
                r_compressed = true;
            } else {
                image = image->decompress();
                r_real_format = image->get_format();
                _get_gl_image_and_format(image, r_real_format, r_real_format, r_gl_format, r_gl_internal_format, r_gl_type, r_compressed, p_force_decompress);
            }
        } break;
        case Image::FORMAT_PVRTC4: {
            if (Config::get_singleton()->pvrtc_supported) {
                r_gl_internal_format = _EXT_COMPRESSED_RGB_PVRTC_4BPPV1_IMG;
                r_gl_format = GL_RGB;
                r_gl_type = GL_UNSIGNED_BYTE;
                r_compressed = true;
            } else {
                image = image->decompress();
                r_real_format = image->get_format();
                _get_gl_image_and_format(image, r_real_format, r_real_format, r_gl_format, r_gl_internal_format, r_gl_type, r_compressed, p_force_decompress);
            }
        } break;
        case Image::FORMAT_PVRTC4A: {
            if (Config::get_singleton()->pvrtc_supported) {
                r_gl_internal_format = _EXT_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG;
                r_gl_format = GL_RGBA;
                r_gl_type = GL_UNSIGNED_BYTE;
                r_compressed = true;
            } else {
                image = image->decompress();
                r_real_format = image->get_format();
                _get_gl_image_and_format(image, r_real_format, r_real_format, r_gl_format, r_gl_internal_format, r_gl_type, r_compressed, p_force_decompress);
            }
        } break;
        case Image::FORMAT_ETC: {
            if (Config::get_singleton()->etc_supported) {
                r_gl_internal_format = _EXT_ETC1_RGB8_OES;
                r_gl_format = GL_RGB;
                r_gl_type = GL_UNSIGNED_BYTE;
                r_compressed = true;
            } else {
                image = image->decompress();
                r_real_format = image->get_format();
                _get_gl_image_and_format(image, r_real_format, r_real_format, r_gl_format, r_gl_internal_format, r_gl_type, r_compressed, p_force_decompress);
            }
        } break;
        default: {
            ERR_FAIL_V_MSG(Ref<Image>(), "Texture format " + itos(p_format) + " not supported for GLES2 backend.");
        } break;
    }

    return image;
}

RID TextureStorage::texture_allocate() {
    return texture_owner.allocate_rid();
}

void TextureStorage::texture_free(RID p_texture) {
    Texture *t = texture_owner.get_or_null(p_texture);
    ERR_FAIL_NULL(t);

    if (t->tex_id != 0) {
        glDeleteTextures(1, &t->tex_id);
        t->tex_id = 0;
    }

    texture_owner.free(p_texture);
}

void TextureStorage::texture_2d_initialize(RID p_texture, const Ref<Image> &p_image) {
    ERR_FAIL_COND(p_image.is_null());

    Texture *texture = texture_owner.get_or_null(p_texture);
    ERR_FAIL_NULL(texture);

    GLenum format;
    GLenum internal_format;
    GLenum type;

    Image::Format real_format;
    Ref<Image> img = _get_gl_image_and_format(p_image, p_image->get_format(), real_format, format, internal_format, type, texture->compressed, false);
    ERR_FAIL_COND(img.is_null());

    texture->width = img->get_width();
    texture->height = img->get_height();
    texture->format = real_format;
    texture->gl_format_cache = format;
    texture->gl_internal_format_cache = internal_format;
    texture->gl_type_cache = type;
    texture->data_size = img->get_data().size();
    texture->mipmaps = img->has_mipmaps() ? img->get_mipmap_count() + 1 : 1;

    glGenTextures(1, &texture->tex_id);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture->tex_id);

    int mipmaps = texture->mipmaps;

    for (int i = 0; i < mipmaps; i++) {
        int level_width = texture->width >> i;
        int level_height = texture->height >> i;

        if (texture->compressed) {
            glCompressedTexImage2D(GL_TEXTURE_2D, i, texture->gl_internal_format_cache, level_width, level_height, 0, img->get_mipmap_byte_size(i), img->get_mipmap_data(i).ptr());
        } else {
            glTexImage2D(GL_TEXTURE_2D, i, texture->gl_internal_format_cache, level_width, level_height, 0, texture->gl_format_cache, texture->gl_type_cache, img->get_mipmap_data(i).ptr());
        }
    }

    texture->active = true;
}

void TextureStorage::texture_2d_update(RID p_texture, const Ref<Image> &p_image, int p_layer) {
    Texture *texture = texture_owner.get_or_null(p_texture);
    ERR_FAIL_NULL(texture);
    ERR_FAIL_COND(!texture->active);
    ERR_FAIL_COND(texture->render_target);

    GLenum format;
    GLenum internal_format;
    GLenum type;

    Image::Format real_format;
    Ref<Image> img = _get_gl_image_and_format(p_image, p_image->get_format(), real_format, format, internal_format, type, texture->compressed, false);
    ERR_FAIL_COND(img.is_null());

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture->tex_id);

    if (texture->compressed) {
        glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, texture->width, texture->height, internal_format, img->get_data().size(), img->get_data().ptr());
    } else {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, texture->width, texture->height, format, type, img->get_data().ptr());
    }

    if (img->has_mipmaps()) {
        for (int i = 1; i < img->get_mipmap_count() + 1; i++) {
            if (texture->compressed) {
                glCompressedTexSubImage2D(GL_TEXTURE_2D, i, 0, 0, texture->width >> i, texture->height >> i, internal_format, img->get_mipmap_data(i).size(), img->get_mipmap_data(i).ptr());
            } else {
                glTexSubImage2D(GL_TEXTURE_2D, i, 0, 0, texture->width >> i, texture->height >> i, format, type, img->get_mipmap_data(i).ptr());
            }
        }
    }

    texture->format = real_format;
    texture->gl_format_cache = format;
    texture->gl_internal_format_cache = internal_format;
    texture->gl_type_cache = type;
    texture->data_size = img->get_data().size();
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

void TextureStorage::texture_bind(RID p_texture, uint32_t p_texture_no) {
    Texture *texture = texture_owner.get_or_null(p_texture);
    ERR_FAIL_NULL(texture);

    glActiveTexture(GL_TEXTURE0 + p_texture_no);
    glBindTexture(GL_TEXTURE_2D, texture->tex_id);
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

/* RENDER TARGET API */

RID TextureStorage::render_target_create() {
    RenderTarget *rt = memnew(RenderTarget);
    rt->texture = texture_allocate();
    rt->fbo = 0;
    rt->color = 0;
    rt->depth = 0;
    rt->width = 0;
    rt->height = 0;
    rt->is_transparent = false;
    rt->use_fbo = true;

    Texture *t = texture_owner.get_or_null(rt->texture);
    t->format = Image::FORMAT_RGBA8;
    t->flags = 0;
    t->width = 0;
    t->height = 0;
    t->render_target = rt;

    return render_target_owner.make_rid(rt);
}

void TextureStorage::render_target_free(RID p_rid) {
    RenderTarget *rt = render_target_owner.get_or_null(p_rid);
    ERR_FAIL_NULL(rt);

    Texture *t = texture_owner.get_or_null(rt->texture);
    t->render_target = nullptr;

    if (rt->fbo) {
        glDeleteFramebuffers(1, &rt->fbo);
    }

    if (rt->color) {
        glDeleteTextures(1, &rt->color);
    }

    if (rt->depth) {
        glDeleteRenderbuffers(1, &rt->depth);
    }

    texture_free(rt->texture);
    memdelete(rt);
    render_target_owner.free(p_rid);
}

void TextureStorage::render_target_set_size(RID p_render_target, int p_width, int p_height, uint32_t p_view_count) {
    RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
    ERR_FAIL_NULL(rt);

    if (rt->width == p_width && rt->height == p_height)
        return;

    _render_target_clear(rt);
    rt->width = p_width;
    rt->height = p_height;
    rt->allocation_depth = p_view_count;

    _render_target_allocate(rt);
}

void TextureStorage::_render_target_clear(RenderTarget *rt) {
    if (rt->fbo) {
        glDeleteFramebuffers(1, &rt->fbo);
        rt->fbo = 0;
    }

    if (rt->color) {
        glDeleteTextures(1, &rt->color);
        rt->color = 0;
    }

    if (rt->depth) {
        glDeleteRenderbuffers(1, &rt->depth);
        rt->depth = 0;
    }

    Texture *tex = texture_owner.get_or_null(rt->texture);
    tex->width = 0;
    tex->height = 0;
    tex->active = false;
}

void TextureStorage::_render_target_allocate(RenderTarget *rt) {
    ERR_FAIL_COND(rt->width <= 0 || rt->height <= 0);

    Texture *tex = texture_owner.get_or_null(rt->texture);

    tex->width = rt->width;
    tex->height = rt->height;
    tex->active = true;

    // Create color texture
    glGenTextures(1, &rt->color);
    glBindTexture(GL_TEXTURE_2D, rt->color);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, rt->width, rt->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Create depth buffer
    glGenRenderbuffers(1, &rt->depth);
    glBindRenderbuffer(GL_RENDERBUFFER, rt->depth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, rt->width, rt->height);

    // Create FBO
    glGenFramebuffers(1, &rt->fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, rt->fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rt->color, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rt->depth);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        ERR_PRINT("Framebuffer status is incomplete. Status: " + itos(status));
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void TextureStorage::render_target_set_flag(RID p_render_target, RenderTargetFlags p_flag, bool p_value) {
    RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
    ERR_FAIL_NULL(rt);

    switch (p_flag) {
        case RENDER_TARGET_TRANSPARENT: {
            rt->is_transparent = p_value;
            // Re-create the render target if it already exists
            if (rt->fbo != 0) {
                _render_target_clear(rt);
                _render_target_allocate(rt);
            }
        } break;
        case RENDER_TARGET_DIRECT_TO_SCREEN: {
            if (p_value) {
                // Direct to screen not supported in GLES2
                WARN_PRINT("Direct to screen render targets are not supported in GLES2.");
            }
        } break;
        default: {
            ERR_FAIL_MSG("Unknown render target flag.");
        }
    }
}

bool TextureStorage::render_target_was_used(RID p_render_target) {
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
    // MSAA for render targets is not supported in GLES2
    WARN_PRINT("MSAA for render targets is not supported in GLES2.");
}

void TextureStorage::render_target_set_use_fxaa(RID p_render_target, bool p_fxaa) {
    // FXAA is not supported in GLES2
    WARN_PRINT("FXAA is not supported in GLES2.");
}

void TextureStorage::render_target_set_use_debanding(RID p_render_target, bool p_debanding) {
    // Debanding is not supported in GLES2
    WARN_PRINT("Debanding is not supported in GLES2.");
}

void TextureStorage::render_target_set_use_hdr(RID p_render_target, bool p_use_hdr) {
    // HDR is not supported in GLES2
    WARN_PRINT("HDR render targets are not supported in GLES2.");
}

/* TEXTURE ATLAS API */

RID TextureStorage::texture_atlas_create() {
    TextureAtlas *atlas = memnew(TextureAtlas);
    ERR_FAIL_NULL_V(atlas, RID());

    return texture_atlas_owner.make_rid(atlas);
}

void TextureStorage::texture_atlas_free(RID p_atlas) {
    TextureAtlas *atlas = texture_atlas_owner.get_or_null(p_atlas);
    ERR_FAIL_NULL(atlas);

    for (const KeyValue<RID, TextureAtlas::Texture> &E : atlas->textures) {
        if (E.value.users > 0) {
            texture_remove_from_texture_atlas(E.key);
        }
    }

    if (atlas->texture != 0) {
        glDeleteTextures(1, &atlas->texture);
        atlas->texture = 0;
    }

    texture_atlas_owner.free(p_atlas);
    memdelete(atlas);
}

void TextureStorage::texture_atlas_set_size(RID p_atlas, int p_width, int p_height, int p_depth) {
    TextureAtlas *atlas = texture_atlas_owner.get_or_null(p_atlas);
    ERR_FAIL_NULL(atlas);

    if (p_width == atlas->size.width && p_height == atlas->size.height)
        return;

    atlas->size.width = p_width;
    atlas->size.height = p_height;
    atlas->dirty = true;
}

void TextureStorage::texture_atlas_add_texture(RID p_atlas, RID p_texture) {
    TextureAtlas *atlas = texture_atlas_owner.get_or_null(p_atlas);
    ERR_FAIL_NULL(atlas);

    TextureAtlas::Texture t;
    t.users = 1;
    atlas->textures[p_texture] = t;
    atlas->dirty = true;
}

void TextureStorage::texture_atlas_remove_texture(RID p_atlas, RID p_texture) {
    TextureAtlas *atlas = texture_atlas_owner.get_or_null(p_atlas);
    ERR_FAIL_NULL(atlas);

    if (!atlas->textures.has(p_texture))
        return;

    atlas->textures[p_texture].users--;
    if (atlas->textures[p_texture].users == 0) {
        atlas->textures.erase(p_texture);
    }
    atlas->dirty = true;
}

void TextureStorage::texture_atlas_update(RID p_atlas) {
    TextureAtlas *atlas = texture_atlas_owner.get_or_null(p_atlas);
    ERR_FAIL_NULL(atlas);

    if (!atlas->dirty)
        return;

    // Sort textures by size
    Vector<TextureAtlas::SortItem> items;
    items.resize(atlas->textures.size());
    int idx = 0;
    for (const KeyValue<RID, TextureAtlas::Texture> &E : atlas->textures) {
        TextureAtlas::SortItem &si = items.write[idx];
        si.texture = E.key;
        Texture *t = texture_owner.get_or_null(si.texture);
        if (t) {
            si.pixel_size = Size2i(t->width, t->height);
            si.size.width = si.pixel_size.width + 2; // Add padding
            si.size.height = si.pixel_size.height + 2;
        }
        idx++;
    }

    items.sort();

    // Allocate spaces for textures
    for (int i = 0; i < items.size(); i++) {
        TextureAtlas::SortItem &si = items.write[i];

        // Simple allocation strategy: find the first available space
        bool allocated = false;
        for (int y = 0; y < atlas->size.height - si.size.height + 1 && !allocated; y++) {
            for (int x = 0; x < atlas->size.width - si.size.width + 1; x++) {
                bool fits = true;
                for (int j = 0; j < si.size.height && fits; j++) {
                    for (int k = 0; k < si.size.width && fits; k++) {
                        if (atlas->used[y + j][x + k]) {
                            fits = false;
                        }
                    }
                }
                if (fits) {
                    si.pos.x = x + 1; // Add padding
                    si.pos.y = y + 1;
                    for (int j = 0; j < si.size.height; j++) {
                        for (int k = 0; k < si.size.width; k++) {
                            atlas->used[y + j][x + k] = true;
                        }
                    }
                    allocated = true;
                    break;
                }
            }
        }
        ERR_CONTINUE(!allocated);
    }

    // Create or update the atlas texture
    if (atlas->texture == 0) {
        glGenTextures(1, &atlas->texture);
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, atlas->texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, atlas->size.width, atlas->size.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    // Copy textures to the atlas
    for (int i = 0; i < items.size(); i++) {
        TextureAtlas::SortItem &si = items[i];
        Texture *t = texture_owner.get_or_null(si.texture);
        if (t && t->tex_id) {
            glBindTexture(GL_TEXTURE_2D, t->tex_id);
            glCopyTexSubImage2D(GL_TEXTURE_2D, 0, si.pos.x, si.pos.y, 0, 0, si.pixel_size.width, si.pixel_size.height);
        }
    }

    // Update UV rects
    for (int i = 0; i < items.size(); i++) {
        TextureAtlas::SortItem &si = items[i];
        TextureAtlas::Texture &t = atlas->textures[si.texture];
        t.uv_rect.position = Vector2(si.pos) / Vector2(atlas->size);
        t.uv_rect.size = Vector2(si.pixel_size) / Vector2(atlas->size);
    }

    atlas->dirty = false;
}

void TextureStorage::texture_set_proxy(RID p_texture, RID p_proxy) {
    // Texture proxies are not supported in GLES2
    WARN_PRINT("Texture proxies are not supported in GLES2.");
}

void TextureStorage::texture_set_force_redraw_if_visible(RID p_texture, bool p_enable) {
    Texture *texture = texture_owner.get_or_null(p_texture);
    ERR_FAIL_NULL(texture);

    texture->redraw_if_visible = p_enable;
}

void TextureStorage::texture_set_detect_3d_callback(RID p_texture, RS::TextureDetectCallback p_callback, void *p_userdata) {
    Texture *texture = texture_owner.get_or_null(p_texture);
    ERR_FAIL_NULL(texture);

    texture->detect_3d_callback = p_callback;
    texture->detect_3d_callback_userdata = p_userdata;
}

void TextureStorage::texture_set_detect_normal_callback(RID p_texture, RS::TextureDetectCallback p_callback, void *p_userdata) {
    Texture *texture = texture_owner.get_or_null(p_texture);
    ERR_FAIL_NULL(texture);

    texture->detect_normal_callback = p_callback;
    texture->detect_normal_callback_userdata = p_userdata;
}

void TextureStorage::texture_set_detect_roughness_callback(RID p_texture, RS::TextureDetectRoughnessCallback p_callback, void *p_userdata) {
    Texture *texture = texture_owner.get_or_null(p_texture);
    ERR_FAIL_NULL(texture);

    texture->detect_roughness_callback = p_callback;
    texture->detect_roughness_callback_userdata = p_userdata;
}

void TextureStorage::texture_debug_usage(List<RS::TextureInfo> *r_info) {
    List<RID> textures;
    texture_owner.get_owned_list(&textures);

    for (List<RID>::Element *E = textures.front(); E; E = E->next()) {
        Texture *t = texture_owner.get_or_null(E->get());
        if (!t)
            continue;
        RS::TextureInfo tinfo;
        tinfo.path = t->path;
        tinfo.format = t->format;
        tinfo.width = t->width;
        tinfo.height = t->height;
        tinfo.depth = 0;
        tinfo.bytes = t->data_size;
        r_info->push_back(tinfo);
    }
}

void TextureStorage::texture_set_path(RID p_texture, const String &p_path) {
    Texture *texture = texture_owner.get_or_null(p_texture);
    ERR_FAIL_NULL(texture);

    texture->path = p_path;
}

String TextureStorage::texture_get_path(RID p_texture) const {
    Texture *texture = texture_owner.get_or_null(p_texture);
    ERR_FAIL_NULL_V(texture, String());

    return texture->path;
}

void TextureStorage::texture_set_shrink_all_x2_on_set_data(bool p_enable) {
    // This optimization is not applicable to GLES2
    WARN_PRINT("Texture shrink optimization is not supported in GLES2.");
}

void TextureStorage::texture_bind(RID p_texture, uint32_t p_texture_no) {
    Texture *texture = texture_owner.get_or_null(p_texture);

    if (!texture) {
        glActiveTexture(GL_TEXTURE0 + p_texture_no);
        glBindTexture(GL_TEXTURE_2D, 0);
        return;
    }

    glActiveTexture(GL_TEXTURE0 + p_texture_no);
    glBindTexture(GL_TEXTURE_2D, texture->tex_id);
}

void TextureStorage::texture_set_filter(RID p_texture, RS::CanvasItemTextureFilter p_filter) {
    Texture *texture = texture_owner.get_or_null(p_texture);
    ERR_FAIL_NULL(texture);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture->tex_id);

    switch (p_filter) {
        case RS::CANVAS_ITEM_TEXTURE_FILTER_NEAREST: {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        } break;
        case RS::CANVAS_ITEM_TEXTURE_FILTER_LINEAR: {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        } break;
        case RS::CANVAS_ITEM_TEXTURE_FILTER_NEAREST_WITH_MIPMAPS: {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        } break;
        case RS::CANVAS_ITEM_TEXTURE_FILTER_LINEAR_WITH_MIPMAPS: {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        } break;
        case RS::CANVAS_ITEM_TEXTURE_FILTER_NEAREST_WITH_MIPMAPS_ANISOTROPIC: {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            if (config->use_anisotropic_filter) {
                glTexParameterf(GL_TEXTURE_2D, _GL_TEXTURE_MAX_ANISOTROPY_EXT, config->anisotropic_level);
            }
        } break;
        case RS::CANVAS_ITEM_TEXTURE_FILTER_LINEAR_WITH_MIPMAPS_ANISOTROPIC: {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            if (config->use_anisotropic_filter) {
                glTexParameterf(GL_TEXTURE_2D, _GL_TEXTURE_MAX_ANISOTROPY_EXT, config->anisotropic_level);
            }
        } break;
        case RS::CANVAS_ITEM_TEXTURE_FILTER_MAX: break; // Internal
    }
}

void TextureStorage::texture_set_repeat(RID p_texture, RS::CanvasItemTextureRepeat p_repeat) {
    Texture *texture = texture_owner.get_or_null(p_texture);
    ERR_FAIL_NULL(texture);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture->tex_id);

    switch (p_repeat) {
        case RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED: {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        } break;
        case RS::CANVAS_ITEM_TEXTURE_REPEAT_ENABLED: {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        } break;
        case RS::CANVAS_ITEM_TEXTURE_REPEAT_MIRROR: {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
        } break;
        case RS::CANVAS_ITEM_TEXTURE_REPEAT_MAX: break; // Internal
    }
}

RID TextureStorage::texture_create_radiance_cubemap(RID p_source, int p_resolution) const {
    // Radiance cubemaps are not supported in GLES2
    WARN_PRINT("Radiance cubemaps are not supported in GLES2.");
    return RID();
}

RenderTarget *TextureStorage::get_render_target(RID p_rid) {
    return render_target_owner.get_or_null(p_rid);
}

void TextureStorage::render_target_set_size(RID p_render_target, int p_width, int p_height) {
    RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
    ERR_FAIL_NULL(rt);

    if (rt->width == p_width && rt->height == p_height) {
        return;
    }

    _render_target_clear(rt);
    rt->width = p_width;
    rt->height = p_height;
    _render_target_allocate(rt);
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
}

Size2i TextureStorage::render_target_get_size(RID p_render_target) const {
    RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
    ERR_FAIL_NULL_V(rt, Size2i());

    return Size2i(rt->width, rt->height);
}

RID TextureStorage::render_target_get_texture(RID p_render_target) {
    RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
    ERR_FAIL_NULL_V(rt, RID());

    return rt->texture;
}

void TextureStorage::render_target_set_external_texture(RID p_render_target, unsigned int p_texture_id) {
    // External textures are not supported in GLES2
    WARN_PRINT("External textures are not supported in GLES2.");
}

void TextureStorage::render_target_set_flag(RID p_render_target, RenderTargetFlags p_flag, bool p_value) {
    RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
    ERR_FAIL_NULL(rt);

    switch (p_flag) {
        case RENDER_TARGET_TRANSPARENT:
            rt->is_transparent = p_value;
            break;
        case RENDER_TARGET_DIRECT_TO_SCREEN:
            // Not supported in GLES2
            WARN_PRINT("Direct to screen render targets are not supported in GLES2.");
            break;
        case RENDER_TARGET_FLAG_MAX:
            break; // Can't happen, but silences warning
    }
}

bool TextureStorage::render_target_was_used(RID p_render_target) {
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
    // MSAA is not supported for render targets in GLES2
    WARN_PRINT("MSAA for render targets is not supported in GLES2.");
}

void TextureStorage::texture_add_to_decal_atlas(RID p_texture, bool p_panorama_to_dp) {
    // Decal atlases are not supported in GLES2
    WARN_PRINT("Decal atlases are not supported in GLES2.");
}

void TextureStorage::texture_remove_from_decal_atlas(RID p_texture, bool p_panorama_to_dp) {
    // Decal atlases are not supported in GLES2
    WARN_PRINT("Decal atlases are not supported in GLES2.");
}

TextureStorage::~TextureStorage() {
    singleton = nullptr;
}

#endif // GLES2_ENABLED
