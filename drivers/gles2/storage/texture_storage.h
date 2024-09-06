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
    virtual ~TextureStorage();

public:
    /* TEXTURE API */

    _FORCE_INLINE_ RID texture_gl_get_default(DefaultGLTexture p_texture) {
        return default_gl_textures[p_texture];
    }

    /* Canvas Texture API */

    CanvasTexture *get_canvas_texture(RID p_rid) { return canvas_texture_owner.get_or_null(p_rid); };
    bool owns_canvas_texture(RID p_rid) { return canvas_texture_owner.owns(p_rid); };

    virtual RID canvas_texture_allocate() override;
    virtual void canvas_texture_initialize(RID p_rid) override;
    virtual void canvas_texture_free(RID p_rid) override;

    virtual void canvas_texture_set_channel(RID p_canvas_texture, RS::CanvasTextureChannel p_channel, RID p_texture) override;
    virtual void canvas_texture_set_shading_parameters(RID p_canvas_texture, const Color &p_base_color, float p_shininess) override;

    virtual void canvas_texture_set_texture_filter(RID p_item, RS::CanvasItemTextureFilter p_filter) override;
    virtual void canvas_texture_set_texture_repeat(RID p_item, RS::CanvasItemTextureRepeat p_repeat) override;

    /* Texture API */

    Texture *get_texture(RID p_rid) {
        Texture *texture = texture_owner.get_or_null(p_rid);
        if (texture && texture->is_proxy) {
            return texture_owner.get_or_null(texture->proxy_to);
        }
        return texture;
    };
    bool owns_texture(RID p_rid) { return texture_owner.owns(p_rid); };

    virtual bool can_create_resources_async() const override;

    virtual RID texture_allocate() override;
    virtual void texture_free(RID p_rid) override;

    virtual void texture_2d_initialize(RID p_texture, const Ref<Image> &p_image) override;
    virtual void texture_2d_layered_initialize(RID p_texture, const Vector<Ref<Image>> &p_layers, RS::TextureLayeredType p_layered_type) override;
    virtual void texture_3d_initialize(RID p_texture, Image::Format, int p_width, int p_height, int p_depth, bool p_mipmaps, const Vector<Ref<Image>> &p_data) override;
    virtual void texture_proxy_initialize(RID p_texture, RID p_base) override;

    virtual void texture_2d_update(RID p_texture, const Ref<Image> &p_image, int p_layer = 0) override;
    virtual void texture_3d_update(RID p_texture, const Vector<Ref<Image>> &p_data) override;
    virtual void texture_proxy_update(RID p_proxy, RID p_base) override;

    virtual void texture_2d_placeholder_initialize(RID p_texture) override;
    virtual void texture_2d_layered_placeholder_initialize(RID p_texture, RenderingServer::TextureLayeredType p_layered_type) override;
    virtual void texture_3d_placeholder_initialize(RID p_texture) override;

    virtual Ref<Image> texture_2d_get(RID p_texture) const override;
    virtual Ref<Image> texture_2d_layer_get(RID p_texture, int p_layer) const override;
    virtual Vector<Ref<Image>> texture_3d_get(RID p_texture) const override;

    virtual void texture_replace(RID p_texture, RID p_by_texture) override;
    virtual void texture_set_size_override(RID p_texture, int p_width, int p_height) override;

    virtual void texture_set_path(RID p_texture, const String &p_path) override;
    virtual String texture_get_path(RID p_texture) const override;

    virtual void texture_set_detect_3d_callback(RID p_texture, RS::TextureDetectCallback p_callback, void *p_userdata) override;
    virtual void texture_set_detect_normal_callback(RID p_texture, RS::TextureDetectCallback p_callback, void *p_userdata) override;
    virtual void texture_set_detect_roughness_callback(RID p_texture, RS::TextureDetectRoughnessCallback p_callback, void *p_userdata) override;

    virtual void texture_debug_usage(List<RS::TextureInfo> *r_info) override;

    virtual void texture_set_force_redraw_if_visible(RID p_texture, bool p_enable) override;

    virtual Size2 texture_size_with_proxy(RID p_proxy) override;

    virtual void texture_rd_initialize(RID p_texture, const RID &p_rd_texture, const RS::TextureLayeredType p_layer_type = RS::TEXTURE_LAYERED_2D_ARRAY) override;
    virtual RID texture_get_rd_texture(RID p_texture, bool p_srgb = false) const override;
    virtual uint64_t texture_get_native_handle(RID p_texture, bool p_srgb = false) const override;

    void texture_set_data(RID p_texture, const Ref<Image> &p_image, int p_layer = 0);
    virtual Image::Format texture_get_format(RID p_texture) const override;
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

    virtual RID decal_allocate() override;
    virtual void decal_initialize(RID p_rid) override;
    virtual void decal_free(RID p_rid) override;

    virtual void decal_set_size(RID p_decal, const Vector3 &p_size) override;
    virtual void decal_set_texture(RID p_decal, RS::DecalTexture p_type, RID p_texture) override;
    virtual void decal_set_emission_energy(RID p_decal, float p_energy) override;
    virtual void decal_set_albedo_mix(RID p_decal, float p_mix) override;
    virtual void decal_set_modulate(RID p_decal, const Color &p_modulate) override;
    virtual void decal_set_cull_mask(RID p_decal, uint32_t p_layers) override;
    virtual void decal_set_distance_fade(RID p_decal, bool p_enabled, float p_begin, float p_length) override;
    virtual void decal_set_fade(RID p_decal, float p_above, float p_below) override;
    virtual void decal_set_normal_fade(RID p_decal, float p_fade) override;

    virtual AABB decal_get_aabb(RID p_decal) const override;

    virtual void texture_add_to_decal_atlas(RID p_texture, bool p_panorama_to_dp = false) override;
    virtual void texture_remove_from_decal_atlas(RID p_texture, bool p_panorama_to_dp = false) override;

    /* DECAL INSTANCE */

    virtual RID decal_instance_create(RID p_decal) override;
    virtual void decal_instance_free(RID p_decal_instance) override;
    virtual void decal_instance_set_transform(RID p_decal, const Transform3D &p_transform) override;

    /* RENDER TARGET API */

/* RENDER TARGET API */

    static GLuint system_fbo;

    RenderTarget *get_render_target(RID p_rid) { return render_target_owner.get_or_null(p_rid); };
    bool owns_render_target(RID p_rid) { return render_target_owner.owns(p_rid); };

    virtual RID render_target_create() override;
    virtual void render_target_free(RID p_rid) override;

    virtual void render_target_set_position(RID p_render_target, int p_x, int p_y) override;
    virtual Point2i render_target_get_position(RID p_render_target) const override;
    virtual void render_target_set_size(RID p_render_target, int p_width, int p_height, uint32_t p_view_count) override;
    virtual Size2i render_target_get_size(RID p_render_target) const override;
    virtual void render_target_set_transparent(RID p_render_target, bool p_is_transparent) override;
    virtual bool render_target_get_transparent(RID p_render_target) const override;
    virtual void render_target_set_direct_to_screen(RID p_render_target, bool p_direct_to_screen) override;
    virtual bool render_target_get_direct_to_screen(RID p_render_target) const override;
    virtual bool render_target_was_used(RID p_render_target) const override;
    void render_target_clear_used(RID p_render_target);
    virtual void render_target_set_msaa(RID p_render_target, RS::ViewportMSAA p_msaa) override;
    virtual RS::ViewportMSAA render_target_get_msaa(RID p_render_target) const override;
    virtual void render_target_set_use_hdr(RID p_render_target, bool p_use_hdr) override;
    virtual bool render_target_is_using_hdr(RID p_render_target) const override;

    virtual void render_target_set_as_unused(RID p_render_target) override {
        render_target_clear_used(p_render_target);
    }

    GLuint render_target_get_color_fbo(RID p_render_target);
    GLuint render_target_get_depth_fbo(RID p_render_target);

    void render_target_request_clear(RID p_render_target, const Color &p_clear_color);
    bool render_target_is_clear_requested(RID p_render_target);
    Color render_target_get_clear_request_color(RID p_render_target);
    void render_target_disable_clear_request(RID p_render_target);
    void render_target_do_clear_request(RID p_render_target);

    virtual RID render_target_get_texture(RID p_render_target) override;

    void render_target_set_external_texture(RID p_render_target, unsigned int p_texture_id);

    virtual void render_target_set_flag(RID p_render_target, RenderTargetFlags p_flag, bool p_value) override;
    virtual bool render_target_was_used(RID p_render_target) override;
    virtual void render_target_set_as_unused(RID p_render_target) override;

    virtual void render_target_copy_to_back_buffer(RID p_render_target, const Rect2i &p_region, bool p_gen_mipmaps) override;
    virtual void render_target_clear_back_buffer(RID p_render_target, const Rect2i &p_region, const Color &p_color) override;
    virtual void render_target_gen_back_buffer_mipmaps(RID p_render_target, const Rect2i &p_region) override;

    void bind_framebuffer(GLuint framebuffer) {
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    }

    void bind_framebuffer_system() {
        glBindFramebuffer(GL_FRAMEBUFFER, system_fbo);
    }

    String get_framebuffer_error(GLenum p_status);

private:
    struct RenderTarget {
        Point2i position;
        Size2i size;
        uint32_t view_count;
        GLuint fbo;
        GLuint color;
        GLuint depth;
        GLuint backbuffer_fbo;
        GLuint backbuffer;

        bool is_transparent;
        bool direct_to_screen;
        bool used_in_frame;
        RS::ViewportMSAA msaa;

        RID texture;

        Color clear_color;
        bool clear_requested;

        RenderTarget() {
            fbo = 0;
            color = 0;
            depth = 0;
            backbuffer_fbo = 0;
            backbuffer = 0;
            is_transparent = false;
            direct_to_screen = false;
            used_in_frame = false;
            msaa = RS::VIEWPORT_MSAA_DISABLED;
            clear_requested = false;
        }
    };
};

}; // namespace GLES2

#endif // GLES2_ENABLED

#endif // TEXTURE_STORAGE_GLES2_H
