/**************************************************************************/
/*  rasterizer_scene_gles2.h                                              */
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

#ifndef RASTERIZER_SCENE_GLES2_H
#define RASTERIZER_SCENE_GLES2_H

#ifdef GLES2_ENABLED

#include "core/math/projection.h"
#include "core/templates/paged_allocator.h"
#include "core/templates/rid_owner.h"
#include "core/templates/self_list.h"
#include "rasterizer_canvas_gles2.h"
#include "drivers/gles2/shaders/sky.glsl.gen.h"
#include "scene/resources/mesh.h"
#include "servers/rendering/renderer_compositor.h"
#include "servers/rendering/renderer_scene_render.h"
#include "servers/rendering_server.h"
#include "shader_gles2.h"
#include "storage/light_storage.h"
#include "storage/material_storage.h"
#include "storage/render_scene_buffers_gles2.h"
#include "storage/utilities.h"

class RasterizerSceneGLES2;

class RasterizerSceneGLES2 : public RendererSceneRender {
private:
    static RasterizerSceneGLES2 *singleton;
    RS::ViewportDebugDraw debug_draw = RS::VIEWPORT_DEBUG_DRAW_DISABLED;
    uint64_t scene_pass = 0;

	template <typename T>
    struct InstanceSort {
        float depth;
        T *instance = nullptr;
        bool operator<(const InstanceSort &p_sort) const {
            return depth < p_sort.depth;
        }
    };

    struct SceneGlobals {
        RID shader_default_version;
        RID default_material;
        RID default_shader;
        RID overdraw_material;
        RID overdraw_shader;
    } scene_globals;

    GLES2::MaterialData *default_material_data_ptr = nullptr;
    GLES2::MaterialData *overdraw_material_data_ptr = nullptr;

    class GeometryInstanceGLES2;

    struct LightData {
        float position[3];
        float inv_radius;

        float direction[3];
        float attenuation;

        float color[3];
        float energy;

        float specular;
        float shadow_alpha;
        float spot_attenuation;
        float spot_angle;

        uint32_t bake_mode;
        uint32_t pad[3];
    };

    struct DirectionalLightData {
        float direction[3];
        float energy;

        float color[3];
        float specular;

        uint32_t bake_mode;
        uint32_t pad[3];
    };

	struct GeometryInstanceSurface {
        enum {
            FLAG_PASS_DEPTH = 1,
            FLAG_PASS_OPAQUE = 2,
            FLAG_PASS_ALPHA = 4,
            FLAG_PASS_SHADOW = 8,
            FLAG_USES_SHARED_SHADOW_MATERIAL = 128,
            FLAG_USES_SCREEN_TEXTURE = 2048,
            FLAG_USES_DEPTH_TEXTURE = 4096,
            FLAG_USES_NORMAL_TEXTURE = 8192,
        };

        uint64_t sort_key;

        RS::PrimitiveType primitive = RS::PRIMITIVE_MAX;
        uint32_t flags = 0;
        uint32_t surface_index = 0;
        uint32_t index_count = 0;

        void *surface = nullptr;
        GLES2::ShaderData *shader = nullptr;
        GLES2::MaterialData *material = nullptr;

        void *surface_shadow = nullptr;
        GLES2::ShaderData *shader_shadow = nullptr;
        GLES2::MaterialData *material_shadow = nullptr;

        GeometryInstanceSurface *next = nullptr;
        GeometryInstanceGLES2 *owner = nullptr;
    };

    struct GeometryInstanceLightmapSH {
        Color sh[9];
    };

    class GeometryInstanceGLES2 : public RenderGeometryInstanceBase {
    public:
        bool store_transform_cache = true;

        int32_t instance_count = 0;

        bool can_sdfgi = false;
        bool using_projectors = false;
        bool using_softshadows = false;

        RID lightmap_instance;
        Rect2 lightmap_uv_scale;
        uint32_t lightmap_slice_index;
        GeometryInstanceLightmapSH *lightmap_sh = nullptr;

        GeometryInstanceSurface *surface_caches = nullptr;
        SelfList<GeometryInstanceGLES2> dirty_list_element;

        GeometryInstanceGLES2() :
                dirty_list_element(this) {}

        virtual void _mark_dirty() override;
        virtual void set_use_lightmap(RID p_lightmap_instance, const Rect2 &p_lightmap_uv_scale, int p_lightmap_slice_index) override;
        virtual void set_lightmap_capture(const Color *p_sh9) override;

        virtual void pair_light_instances(const RID *p_light_instances, uint32_t p_light_instance_count) override;
        virtual void pair_reflection_probe_instances(const RID *p_reflection_probe_instances, uint32_t p_reflection_probe_instance_count) override;
        virtual void pair_decal_instances(const RID *p_decal_instances, uint32_t p_decal_instance_count) override {}
        virtual void pair_voxel_gi_instances(const RID *p_voxel_gi_instances, uint32_t p_voxel_gi_instance_count) override {}

        virtual void set_softshadow_projector_pairing(bool p_softshadow, bool p_projector) override {}
    };

	enum {
        INSTANCE_DATA_FLAGS_DYNAMIC = 8,
        INSTANCE_DATA_FLAG_USE_LIGHTMAP = 256,
        INSTANCE_DATA_FLAG_USE_SH_LIGHTMAP = 512,
    };

    static void _geometry_instance_dependency_changed(Dependency::DependencyChangedNotification p_notification, DependencyTracker *p_tracker);
    static void _geometry_instance_dependency_deleted(const RID &p_dependency, DependencyTracker *p_tracker);

    SelfList<GeometryInstanceGLES2>::List geometry_instance_dirty_list;

    PagedAllocator<GeometryInstanceGLES2> geometry_instance_alloc;
    PagedAllocator<GeometryInstanceSurface> geometry_instance_surface_alloc;

    void _geometry_instance_add_surface_with_material(GeometryInstanceGLES2 *ginstance, uint32_t p_surface, GLES2::MaterialData *p_material, uint32_t p_material_id, uint32_t p_shader_id, RID p_mesh);
    void _geometry_instance_add_surface_with_material_chain(GeometryInstanceGLES2 *ginstance, uint32_t p_surface, GLES2::MaterialData *p_material, RID p_mat_src, RID p_mesh);
    void _geometry_instance_add_surface(GeometryInstanceGLES2 *ginstance, uint32_t p_surface, RID p_material, RID p_mesh);
    void _geometry_instance_update(RenderGeometryInstance *p_geometry_instance);
    void _update_dirty_geometry_instances();

    struct SceneState {
        struct UBO {
            float projection_matrix[16];
            float inv_projection_matrix[16];
            float camera_matrix[16];
            float inv_camera_matrix[16];

            float viewport_size[2];
            float screen_pixel_size[2];

            float ambient_light_color[4];
            float bg_color[4];

            float fog_color_enabled[4];

            float fog_density;
            float fog_depth;
            float fog_height;
            float z_far;

            float subsurface_scatter_width;
            float ambient_occlusion_affect_light;
            uint32_t material_uv2_mode;
            float emissive_exposure_normalization;
        };

        struct MultiviewUBO {
            float projection_matrix_view[RendererSceneRender::MAX_RENDER_VIEWS][16];
            float inv_projection_matrix_view[RendererSceneRender::MAX_RENDER_VIEWS][16];
            float eye_offset[RendererSceneRender::MAX_RENDER_VIEWS][4];
        };

        UBO ubo;
        GLuint ubo_buffer = 0;
        MultiviewUBO multiview_ubo;
        GLuint multiview_buffer = 0;

        GLuint tonemap_buffer = 0;
        GLuint tonemap_fbo = 0;

        bool used_depth_prepass = false;

        bool used_screen_texture = false;
        bool used_normal_texture = false;
        bool used_depth_texture = false;

        LightData *omni_lights = nullptr;
        LightData *spot_lights = nullptr;

        InstanceSort<GLES2::LightInstance> *omni_light_sort;
        InstanceSort<GLES2::LightInstance> *spot_light_sort;
        GLuint omni_light_buffer = 0;
        GLuint spot_light_buffer = 0;
        uint32_t omni_light_count = 0;
        uint32_t spot_light_count = 0;

        DirectionalLightData *directional_lights = nullptr;
        GLuint directional_light_buffer = 0;
    } scene_state;

public:
    static RasterizerSceneGLES2 *get_singleton() { return singleton; }

    RasterizerCanvasGLES2 *canvas = nullptr;

    RenderGeometryInstance *geometry_instance_create(RID p_base) override;
    void geometry_instance_free(RenderGeometryInstance *p_geometry_instance) override;

    uint32_t geometry_instance_get_pair_mask() override;

    /* SDFGI UPDATE */

    void sdfgi_update(const Ref<RenderSceneBuffers> &p_render_buffers, RID p_environment, const Vector3 &p_world_position) override {}
    int sdfgi_get_pending_region_count(const Ref<RenderSceneBuffers> &p_render_buffers) const override { return 0; }
    AABB sdfgi_get_pending_region_bounds(const Ref<RenderSceneBuffers> &p_render_buffers, int p_region) const override { return AABB(); }
    uint32_t sdfgi_get_pending_region_cascade(const Ref<RenderSceneBuffers> &p_render_buffers, int p_region) const override { return 0; }

    /* SKY API */

    RID sky_allocate() override;
    void sky_initialize(RID p_rid) override;
    void sky_set_radiance_size(RID p_sky, int p_radiance_size) override;
    void sky_set_mode(RID p_sky, RS::SkyMode p_mode) override;
    void sky_set_material(RID p_sky, RID p_material) override;
    Ref<Image> sky_bake_panorama(RID p_sky, float p_energy, bool p_bake_irradiance, const Size2i &p_size) override;

    /* ENVIRONMENT API */

    void environment_glow_set_use_bicubic_upscale(bool p_enable) override;

    void environment_set_ssr_roughness_quality(RS::EnvironmentSSRRoughnessQuality p_quality) override;

    void environment_set_ssao_quality(RS::EnvironmentSSAOQuality p_quality, bool p_half_size, float p_adaptive_target, int p_blur_passes, float p_fadeout_from, float p_fadeout_to) override;

    void environment_set_ssil_quality(RS::EnvironmentSSILQuality p_quality, bool p_half_size, float p_adaptive_target, int p_blur_passes, float p_fadeout_from, float p_fadeout_to) override;

    void environment_set_sdfgi_ray_count(RS::EnvironmentSDFGIRayCount p_ray_count) override;
    void environment_set_sdfgi_frames_to_converge(RS::EnvironmentSDFGIFramesToConverge p_frames) override;
    void environment_set_sdfgi_frames_to_update_light(RS::EnvironmentSDFGIFramesToUpdateLight p_update) override;

    void environment_set_volumetric_fog_volume_size(int p_size, int p_depth) override;
    void environment_set_volumetric_fog_filter_active(bool p_enable) override;

    Ref<Image> environment_bake_panorama(RID p_env, bool p_bake_irradiance, const Size2i &p_size) override;

	void positional_soft_shadow_filter_set_quality(RS::ShadowQuality p_quality) override;
    void directional_soft_shadow_filter_set_quality(RS::ShadowQuality p_quality) override;

    RID fog_volume_instance_create(RID p_fog_volume) override;
    void fog_volume_instance_set_transform(RID p_fog_volume_instance, const Transform3D &p_transform) override;
    void fog_volume_instance_set_active(RID p_fog_volume_instance, bool p_active) override;
    RID fog_volume_instance_get_volume(RID p_fog_volume_instance) const override;
    Vector3 fog_volume_instance_get_position(RID p_fog_volume_instance) const override;

    RID voxel_gi_instance_create(RID p_voxel_gi) override;
    void voxel_gi_instance_set_transform_to_data(RID p_probe, const Transform3D &p_xform) override;
    bool voxel_gi_needs_update(RID p_probe) const override;
    void voxel_gi_update(RID p_probe, bool p_update_light_instances, const Vector<RID> &p_light_instances, const PagedArray<RenderGeometryInstance *> &p_dynamic_objects) override;

    void voxel_gi_set_quality(RS::VoxelGIQuality) override;

    void render_scene(const Ref<RenderSceneBuffers> &p_render_buffers, const CameraData *p_camera_data, const CameraData *p_prev_camera_data, const PagedArray<RenderGeometryInstance *> &p_instances, const PagedArray<RID> &p_lights, const PagedArray<RID> &p_reflection_probes, const PagedArray<RID> &p_voxel_gi_instances, const PagedArray<RID> &p_decals, const PagedArray<RID> &p_lightmaps, const PagedArray<RID> &p_fog_volumes, RID p_environment, RID p_camera_attributes, RID p_compositor, RID p_shadow_atlas, RID p_occluder_debug_tex, RID p_reflection_atlas, RID p_reflection_probe, int p_reflection_probe_pass, float p_screen_mesh_lod_threshold, const RenderShadowData *p_render_shadows, int p_render_shadow_count, const RenderSDFGIData *p_render_sdfgi_regions, int p_render_sdfgi_region_count, const RenderSDFGIUpdateData *p_sdfgi_update_data = nullptr, RenderingMethod::RenderInfo *r_render_info = nullptr) override;
    void render_material(const Transform3D &p_cam_transform, const Projection &p_cam_projection, bool p_cam_orthogonal, const PagedArray<RenderGeometryInstance *> &p_instances, RID p_framebuffer, const Rect2i &p_region) override;
    void render_particle_collider_heightfield(RID p_collider, const Transform3D &p_transform, const PagedArray<RenderGeometryInstance *> &p_instances) override;

    void set_scene_pass(uint64_t p_pass) override {
        scene_pass = p_pass;
    }

    _FORCE_INLINE_ uint64_t get_scene_pass() {
        return scene_pass;
    }

    void set_time(double p_time, double p_step) override;
    void set_debug_draw_mode(RS::ViewportDebugDraw p_debug_draw) override;
    _FORCE_INLINE_ RS::ViewportDebugDraw get_debug_draw_mode() const {
        return debug_draw;
    }

    Ref<RenderSceneBuffers> render_buffers_create() override;
    void gi_set_use_half_resolution(bool p_enable) override;

    void screen_space_roughness_limiter_set_active(bool p_enable, float p_amount, float p_curve) override;
    bool screen_space_roughness_limiter_is_active() const override;

    void sub_surface_scattering_set_quality(RS::SubSurfaceScatteringQuality p_quality) override;
    void sub_surface_scattering_set_scale(float p_scale, float p_depth_scale) override;

    TypedArray<Image> bake_render_uv2(RID p_base, const TypedArray<RID> &p_material_overrides, const Size2i &p_image_size) override;
    void _render_uv2(const PagedArray<RenderGeometryInstance *> &p_instances, GLuint p_framebuffer, const Rect2i &p_region);

    bool free(RID p_rid) override;
    void update() override;
    void sdfgi_set_debug_probe_select(const Vector3 &p_position, const Vector3 &p_dir) override;

    void decals_set_filter(RS::DecalFilter p_filter) override;
    void light_projectors_set_filter(RS::LightProjectorFilter p_filter) override;

    RasterizerSceneGLES2();
    ~RasterizerSceneGLES2();
};

#endif // GLES2_ENABLED

#endif // RASTERIZER_SCENE_GLES2_H