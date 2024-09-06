/**************************************************************************/
/*  texture_storage.h                                                     */
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

#ifdef GLES2_ENABLED

#include "config.h"
#include "drivers/gles2/shader_gles2.h"
#include "servers/rendering/storage/texture_storage.h"
#include "platform_gl.h"

namespace GLES2 {

enum DefaultGLTexture {
	DEFAULT_GL_TEXTURE_WHITE,
	DEFAULT_GL_TEXTURE_BLACK,
	DEFAULT_GL_TEXTURE_TRANSPARENT,
	DEFAULT_GL_TEXTURE_NORMAL,
	DEFAULT_GL_TEXTURE_ANISO,
	DEFAULT_GL_TEXTURE_DEPTH,
	DEFAULT_GL_TEXTURE_CUBEMAP_BLACK,
	DEFAULT_GL_TEXTURE_CUBEMAP_WHITE,
	//DEFAULT_GL_TEXTURE_3D_WHITE, -- Not supported in GLES2
	//DEFAULT_GL_TEXTURE_3D_BLACK, -- Not supported in GLES2
	//DEFAULT_GL_TEXTURE_2D_ARRAY_WHITE, -- Not supported in GLES2
	//DEFAULT_GL_TEXTURE_2D_UINT, -- Not supported in GLES2
	DEFAULT_GL_TEXTURE_MAX
};

struct CanvasTexture {
	RID diffuse;
	RID normal_map;
	RID specular;
	Color specular_color = Color(1, 1, 1, 1);
	float shininess = 1.0;

	RS::CanvasItemTextureFilter texture_filter = RS::CANVAS_ITEM_TEXTURE_FILTER_DEFAULT;
	RS::CanvasItemTextureRepeat texture_repeat = RS::CANVAS_ITEM_TEXTURE_REPEAT_DEFAULT;
};

struct RenderTarget;

struct Texture {
    RID self;

    bool is_proxy = false;
    bool is_external = false;
    bool is_render_target = false;

    RID proxy_to;
    Vector<RID> proxies;

    String path;
    int width = 0;
    int height = 0;
    int depth = 0;
    int mipmaps = 1;
    int layers = 1;
    int alloc_width = 0;
    int alloc_height = 0;
    Image::Format format = Image::FORMAT_R8;
    Image::Format real_format = Image::FORMAT_R8;

    enum Type {
        TYPE_2D,
        TYPE_CUBEMAP,
        TYPE_3D
    };

    Type type = TYPE_2D;
    RS::TextureLayeredType layered_type = RS::TEXTURE_LAYERED_2D_ARRAY;

    GLenum target = GL_TEXTURE_2D;
    GLenum gl_format_cache = 0;
    GLenum gl_internal_format_cache = 0;
    GLenum gl_type_cache = 0;

    int total_data_size = 0;

    bool compressed = false;

    bool resize_to_po2 = false;

    bool active = false;
    GLuint tex_id = 0;

    uint16_t stored_cube_sides = 0;

    RenderTarget *render_target = nullptr;

    Ref<Image> image_cache_2d;
    Vector<Ref<Image>> image_cache_cubemap;

    bool redraw_if_visible = false;

    RS::TextureDetectCallback detect_3d_callback = nullptr;
    void *detect_3d_callback_ud = nullptr;

    RS::TextureDetectCallback detect_normal_callback = nullptr;
    void *detect_normal_callback_ud = nullptr;

    RS::TextureDetectRoughnessCallback detect_roughness_callback = nullptr;
    void *detect_roughness_callback_ud = nullptr;

    CanvasTexture *canvas_texture = nullptr;

    void copy_from(const Texture &o) {
        proxy_to = o.proxy_to;
        is_proxy = o.is_proxy;
        is_external = o.is_external;
        width = o.width;
        height = o.height;
        format = o.format;
        type = o.type;
        target = o.target;
        total_data_size = o.total_data_size;
        compressed = o.compressed;
        mipmaps = o.mipmaps;
        resize_to_po2 = o.resize_to_po2;
        active = o.active;
        tex_id = o.tex_id;
        stored_cube_sides = o.stored_cube_sides;
        render_target = o.render_target;
        is_render_target = o.is_render_target;
        redraw_if_visible = o.redraw_if_visible;
        detect_3d_callback = o.detect_3d_callback;
        detect_3d_callback_ud = o.detect_3d_callback_ud;
        detect_normal_callback = o.detect_normal_callback;
        detect_normal_callback_ud = o.detect_normal_callback_ud;
        detect_roughness_callback = o.detect_roughness_callback;
        detect_roughness_callback_ud = o.detect_roughness_callback_ud;
    }

    // texture state
    void gl_set_filter(RS::CanvasItemTextureFilter p_filter) {
        if (p_filter == state_filter) {
            return;
        }
        state_filter = p_filter;
        GLenum pmin = GL_NEAREST;
        GLenum pmag = GL_NEAREST;
        switch (state_filter) {
            case RS::CANVAS_ITEM_TEXTURE_FILTER_NEAREST: {
                pmin = GL_NEAREST;
                pmag = GL_NEAREST;
            } break;
            case RS::CANVAS_ITEM_TEXTURE_FILTER_LINEAR: {
                pmin = GL_LINEAR;
                pmag = GL_LINEAR;
            } break;
            case RS::CANVAS_ITEM_TEXTURE_FILTER_NEAREST_WITH_MIPMAPS:
            case RS::CANVAS_ITEM_TEXTURE_FILTER_NEAREST_WITH_MIPMAPS_ANISOTROPIC: {
                pmin = mipmaps > 1 ? GL_NEAREST_MIPMAP_NEAREST : GL_NEAREST;
                pmag = GL_NEAREST;
            } break;
            case RS::CANVAS_ITEM_TEXTURE_FILTER_LINEAR_WITH_MIPMAPS:
            case RS::CANVAS_ITEM_TEXTURE_FILTER_LINEAR_WITH_MIPMAPS_ANISOTROPIC: {
                pmin = mipmaps > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR;
                pmag = GL_LINEAR;
            } break;
            default: {
                return;
            } break;
        }
        glTexParameteri(target, GL_TEXTURE_MIN_FILTER, pmin);
        glTexParameteri(target, GL_TEXTURE_MAG_FILTER, pmag);
    }

    void gl_set_repeat(RS::CanvasItemTextureRepeat p_repeat) {
        if (p_repeat == state_repeat) {
            return;
        }
        state_repeat = p_repeat;
        GLenum prep = GL_CLAMP_TO_EDGE;
        switch (state_repeat) {
            case RS::CANVAS_ITEM_TEXTURE_REPEAT_DISABLED: {
                prep = GL_CLAMP_TO_EDGE;
            } break;
            case RS::CANVAS_ITEM_TEXTURE_REPEAT_ENABLED: {
                prep = GL_REPEAT;
            } break;
            case RS::CANVAS_ITEM_TEXTURE_REPEAT_MIRROR: {
                prep = GL_MIRRORED_REPEAT;
            } break;
            default: {
                return;
            } break;
        }
        glTexParameteri(target, GL_TEXTURE_WRAP_S, prep);
        glTexParameteri(target, GL_TEXTURE_WRAP_T, prep);
        if (target == GL_TEXTURE_CUBE_MAP || target == GL_TEXTURE_3D) {
            glTexParameteri(target, GL_TEXTURE_WRAP_R, prep);
        }
    }

private:
    RS::CanvasItemTextureFilter state_filter = RS::CANVAS_ITEM_TEXTURE_FILTER_MAX;
    RS::CanvasItemTextureRepeat state_repeat = RS::CANVAS_ITEM_TEXTURE_REPEAT_MAX;
};


struct RenderTarget {
    Point2i position = Point2i(0, 0);
    Size2i size = Size2i(0, 0);
    uint32_t view_count = 1;
    int mipmap_count = 1;
    RID self;
    GLuint fbo = 0;
    GLuint color = 0;
    GLuint depth = 0;

    bool hdr = false; // For Compatibility this affects both 2D and 3D rendering!
    GLuint color_internal_format = GL_RGBA;
    GLuint color_format = GL_RGBA;
    GLuint color_type = GL_UNSIGNED_BYTE;
    uint32_t color_format_size = 4;
    Image::Format image_format = Image::FORMAT_RGBA8;

    bool is_transparent = false;
    bool direct_to_screen = false;

    bool used_in_frame = false;
    bool reattach_textures = false;

    struct RTOverridden {
        bool is_overridden = false;
        RID color;
        RID depth;

        struct FBOCacheEntry {
            GLuint fbo;
            GLuint color;
            GLuint depth;
            Size2i size;
            Vector<GLuint> allocated_textures;
        };
        RBMap<uint32_t, FBOCacheEntry> fbo_cache;
    } overridden;

    RID texture;

    Color clear_color = Color(1, 1, 1, 1);
    bool clear_requested = false;

    RenderTarget() {
    }
};

class TextureStorage : public RendererTextureStorage {
private:
    static TextureStorage *singleton;

    RID default_gl_textures[DEFAULT_GL_TEXTURE_MAX];

    /* Canvas Texture API */

    RID_Owner<CanvasTexture, true> canvas_texture_owner;

    /* Texture API */

    mutable RID_Owner<Texture, true> texture_owner;

    Ref<Image> _get_gl_image_and_format(const Ref<Image> &p_image, Image::Format p_format, Image::Format &r_real_format, GLenum &r_gl_format, GLenum &r_gl_internal_format, GLenum &r_gl_type, bool &r_compressed, bool p_force_decompress) const;

    /* TEXTURE ATLAS API */

    struct TextureAtlas {
        struct Texture {
            int users;
            Rect2 uv_rect;
        };

        struct SortItem {
            RID texture;
            Size2i pixel_size;
            Size2i size;
            Point2i pos;

            bool operator<(const SortItem &p_item) const {
                //sort larger to smaller
                if (size.height == p_item.size.height) {
                    return size.width > p_item.size.width;
                } else {
                    return size.height > p_item.size.height;
                }
            }
        };

        HashMap<RID, Texture> textures;
        bool dirty = true;

        GLuint texture = 0;
        GLuint framebuffer = 0;
        Size2i size;
    } texture_atlas;

    /* Render Target API */

    mutable RID_Owner<RenderTarget> render_target_owner;

    void _clear_render_target(RenderTarget *rt);
    void _update_render_target(RenderTarget *rt);
    void _create_render_target_backbuffer(RenderTarget *rt);

public:
    static TextureStorage *get_singleton();

    TextureStorage();
    ~TextureStorage();

    /* TEXTURE API */

    _FORCE_INLINE_ RID texture_gl_get_default(DefaultGLTexture p_texture) {
        return default_gl_textures[p_texture];
    }

    /* Canvas Texture API */

    CanvasTexture *get_canvas_texture(RID p_rid) { return canvas_texture_owner.get_or_null(p_rid); };
    bool owns_canvas_texture(RID p_rid) { return canvas_texture_owner.owns(p_rid); };

    RID canvas_texture_allocate() override;
    void canvas_texture_initialize(RID p_rid) override;
    void canvas_texture_free(RID p_rid) override;

    void canvas_texture_set_channel(RID p_canvas_texture, RS::CanvasTextureChannel p_channel, RID p_texture) override;
    void canvas_texture_set_shading_parameters(RID p_canvas_texture, const Color &p_base_color, float p_shininess) override;

    void canvas_texture_set_texture_filter(RID p_item, RS::CanvasItemTextureFilter p_filter) override;
    void canvas_texture_set_texture_repeat(RID p_item, RS::CanvasItemTextureRepeat p_repeat) override;

    /* Texture API */

    Texture *get_texture(RID p_rid) {
        Texture *texture = texture_owner.get_or_null(p_rid);
        if (texture && texture->is_proxy) {
            return texture_owner.get_or_null(texture->proxy_to);
        }
        return texture;
    };
    bool owns_texture(RID p_rid) { return texture_owner.owns(p_rid); };

    bool can_create_resources_async() const override;

    RID texture_allocate() override;
    void texture_free(RID p_rid) override;

    void texture_2d_initialize(RID p_texture, const Ref<Image> &p_image) override;
    void texture_2d_layered_initialize(RID p_texture, const Vector<Ref<Image>> &p_layers, RS::TextureLayeredType p_layered_type) override;
    void texture_3d_initialize(RID p_texture, Image::Format, int p_width, int p_height, int p_depth, bool p_mipmaps, const Vector<Ref<Image>> &p_data) override;
    void texture_proxy_initialize(RID p_texture, RID p_base) override;

    void texture_2d_update(RID p_texture, const Ref<Image> &p_image, int p_layer = 0) override;
    void texture_3d_update(RID p_texture, const Vector<Ref<Image>> &p_data) override;
    void texture_proxy_update(RID p_proxy, RID p_base) override;

    void texture_2d_placeholder_initialize(RID p_texture) override;
    void texture_2d_layered_placeholder_initialize(RID p_texture, RenderingServer::TextureLayeredType p_layered_type) override;
    void texture_3d_placeholder_initialize(RID p_texture) override;

    Ref<Image> texture_2d_get(RID p_texture) const override;
    Ref<Image> texture_2d_layer_get(RID p_texture, int p_layer) const override;
    Vector<Ref<Image>> texture_3d_get(RID p_texture) const override;

    void texture_replace(RID p_texture, RID p_by_texture) override;
    void texture_set_size_override(RID p_texture, int p_width, int p_height) override;

    void texture_set_path(RID p_texture, const String &p_path) override;
    String texture_get_path(RID p_texture) const override;

    void texture_set_detect_3d_callback(RID p_texture, RS::TextureDetectCallback p_callback, void *p_userdata) override;
    void texture_set_detect_normal_callback(RID p_texture, RS::TextureDetectCallback p_callback, void *p_userdata) override;
    void texture_set_detect_roughness_callback(RID p_texture, RS::TextureDetectRoughnessCallback p_callback, void *p_userdata) override;

    void texture_debug_usage(List<RS::TextureInfo> *r_info) override;

    void texture_set_force_redraw_if_visible(RID p_texture, bool p_enable) override;

    Size2 texture_size_with_proxy(RID p_proxy) override;

    void texture_rd_initialize(RID p_texture, const RID &p_rd_texture, const RS::TextureLayeredType p_layer_type = RS::TEXTURE_LAYERED_2D_ARRAY) override;
    RID texture_get_rd_texture(RID p_texture, bool p_srgb = false) const override;
    uint64_t texture_get_native_handle(RID p_texture, bool p_srgb = false) const override;

    void texture_set_data(RID p_texture, const Ref<Image> &p_image, int p_layer = 0);
    Image::Format texture_get_format(RID p_texture) const override;
    uint32_t texture_get_texid(RID p_texture) const;
    uint32_t texture_get_width(RID p_texture) const;
    uint32_t texture_get_height(RID p_texture) const;
    uint32_t texture_get_depth(RID p_texture) const;
    void texture_bind(RID p_texture, uint32_t p_texture_no);

    /* TEXTURE ATLAS API */

    void update_texture_atlas();

    GLuint texture_atlas_get_texture() const;
    _FORCE_INLINE_ Rect2 texture_atlas_get_texture_rect(RID p_texture) {
        TextureAtlas::Texture *t = texture_atlas.textures.getptr(p_texture);
        if (!t) {
            return Rect2();
        }

        return t->uv_rect;
    }

    void texture_add_to_texture_atlas(RID p_texture);
    void texture_remove_from_texture_atlas(RID p_texture);
    void texture_atlas_mark_dirty_on_texture(RID p_texture);
    void texture_atlas_remove_texture(RID p_texture);

    /* DECAL API */

    RID decal_allocate() override;
    void decal_initialize(RID p_rid) override;
    void decal_free(RID p_rid) override;

    void decal_set_size(RID p_decal, const Vector3 &p_size) override;
    void decal_set_texture(RID p_decal, RS::DecalTexture p_type, RID p_texture) override;
    void decal_set_emission_energy(RID p_decal, float p_energy) override;
    void decal_set_albedo_mix(RID p_decal, float p_mix) override;
    void decal_set_modulate(RID p_decal, const Color &p_modulate) override;
    void decal_set_cull_mask(RID p_decal, uint32_t p_layers) override;
    void decal_set_distance_fade(RID p_decal, bool p_enabled, float p_begin, float p_length) override;
    void decal_set_fade(RID p_decal, float p_above, float p_below) override;
    void decal_set_normal_fade(RID p_decal, float p_fade) override;

    AABB decal_get_aabb(RID p_decal) const override;

    void texture_add_to_decal_atlas(RID p_texture, bool p_panorama_to_dp = false) override;
    void texture_remove_from_decal_atlas(RID p_texture, bool p_panorama_to_dp = false) override;

    /* DECAL INSTANCE */

    RID decal_instance_create(RID p_decal) override;
    void decal_instance_free(RID p_decal_instance) override;
    void decal_instance_set_transform(RID p_decal, const Transform3D &p_transform) override;

    /* RENDER TARGET API */

    /* RENDER TARGET API */

    static GLuint system_fbo;

    RenderTarget *get_render_target(RID p_rid) { return render_target_owner.get_or_null(p_rid); };
    bool owns_render_target(RID p_rid) { return render_target_owner.owns(p_rid); };

    RID render_target_create() override;
    void render_target_free(RID p_rid) override;

    void render_target_set_position(RID p_render_target, int p_x, int p_y) override;
    Point2i render_target_get_position(RID p_render_target) const override;
    void render_target_set_size(RID p_render_target, int p_width, int p_height, uint32_t p_view_count) override;
    Size2i render_target_get_size(RID p_render_target) const override;
    void render_target_set_transparent(RID p_render_target, bool p_is_transparent) override;
    bool render_target_get_transparent(RID p_render_target) const override;
    void render_target_set_direct_to_screen(RID p_render_target, bool p_direct_to_screen) override;
    bool render_target_get_direct_to_screen(RID p_render_target) const override;
    bool render_target_was_used(RID p_render_target) const override;
    void render_target_clear_used(RID p_render_target);
    void render_target_set_msaa(RID p_render_target, RS::ViewportMSAA p_msaa) override;
    RS::ViewportMSAA render_target_get_msaa(RID p_render_target) const override;
    void render_target_set_use_hdr(RID p_render_target, bool p_use_hdr) override;
    bool render_target_is_using_hdr(RID p_render_target) const override;

    void render_target_set_as_unused(RID p_render_target) override {
        render_target_clear_used(p_render_target);
    }

    GLuint render_target_get_color_fbo(RID p_render_target);
    GLuint render_target_get_depth_fbo(RID p_render_target);

    void render_target_request_clear(RID p_render_target, const Color &p_clear_color);
    bool render_target_is_clear_requested(RID p_render_target);
    Color render_target_get_clear_request_color(RID p_render_target);
    void render_target_disable_clear_request(RID p_render_target);
    void render_target_do_clear_request(RID p_render_target);

    RID render_target_get_texture(RID p_render_target) override;

    void render_target_set_external_texture(RID p_render_target, unsigned int p_texture_id);

    // void render_target_set_flag(RID p_render_target, RenderTargetFlags p_flag, bool p_value) override;

    void render_target_copy_to_back_buffer(RID p_render_target, const Rect2i &p_region, bool p_gen_mipmaps);
    void render_target_clear_back_buffer(RID p_render_target, const Rect2i &p_region, const Color &p_color);
    void render_target_gen_back_buffer_mipmaps(RID p_render_target, const Rect2i &p_region);

    void bind_framebuffer(GLuint framebuffer) {
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    }

    void bind_framebuffer_system() {
        glBindFramebuffer(GL_FRAMEBUFFER, system_fbo);
    }

    String get_framebuffer_error(GLenum p_status);

// private:
//     struct RenderTarget {
//         Point2i position;
//         Size2i size;
//         uint32_t view_count;
//         GLuint fbo;
//         GLuint color;
//         GLuint depth;
//         GLuint backbuffer_fbo;
//         GLuint backbuffer;

//         bool is_transparent;
//         bool direct_to_screen;
//         bool used_in_frame;
//         RS::ViewportMSAA msaa;

//         RID texture;

//         Color clear_color;
//         bool clear_requested;

//         RenderTarget() {
//             fbo = 0;
//             color = 0;
//             depth = 0;
//             backbuffer_fbo = 0;
//             backbuffer = 0;
//             is_transparent = false;
//             direct_to_screen = false;
//             used_in_frame = false;
//             msaa = RS::VIEWPORT_MSAA_DISABLED;
//             clear_requested = false;
//         }
//     };
};

}; // namespace GLES2

#endif // GLES2_ENABLED

#endif // TEXTURE_STORAGE_GLES2_H
