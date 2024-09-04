/**************************************************************************/
/*  light_storage.cpp                                                     */
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

#ifdef GLES2_ENABLED

#include "light_storage.h"
#include "config.h"
#include "texture_storage.h"
#include "material_storage.h"
#include "utilities.h"

using namespace GLES2;

LightStorage *LightStorage::singleton = nullptr;

LightStorage *LightStorage::get_singleton() {
    return singleton;
}

LightStorage::LightStorage() {
    singleton = this;
}

LightStorage::~LightStorage() {
    singleton = nullptr;
}

/* LIGHT API */

RID LightStorage::directional_light_allocate() {
    return light_owner.allocate_rid();
}

void LightStorage::directional_light_initialize(RID p_rid) {
    light_owner.initialize_rid(p_rid, Light());
    Light *light = light_owner.get_or_null(p_rid);
    light->type = RS::LIGHT_DIRECTIONAL;
}

RID LightStorage::omni_light_allocate() {
    return light_owner.allocate_rid();
}

void LightStorage::omni_light_initialize(RID p_rid) {
    light_owner.initialize_rid(p_rid, Light());
    Light *light = light_owner.get_or_null(p_rid);
    light->type = RS::LIGHT_OMNI;
}

RID LightStorage::spot_light_allocate() {
    return light_owner.allocate_rid();
}

void LightStorage::spot_light_initialize(RID p_rid) {
    light_owner.initialize_rid(p_rid, Light());
    Light *light = light_owner.get_or_null(p_rid);
    light->type = RS::LIGHT_SPOT;
}

void LightStorage::light_free(RID p_rid) {
    Light *light = light_owner.get_or_null(p_rid);
    ERR_FAIL_NULL(light);

    light_owner.free(p_rid);
}

void LightStorage::light_set_color(RID p_light, const Color &p_color) {
    Light *light = light_owner.get_or_null(p_light);
    ERR_FAIL_NULL(light);

    light->color = p_color;
}

void LightStorage::light_set_param(RID p_light, RS::LightParam p_param, float p_value) {
    Light *light = light_owner.get_or_null(p_light);
    ERR_FAIL_NULL(light);
    ERR_FAIL_INDEX(p_param, RS::LIGHT_PARAM_MAX);

    light->param[p_param] = p_value;
}

void LightStorage::light_set_shadow(RID p_light, bool p_enabled) {
    Light *light = light_owner.get_or_null(p_light);
    ERR_FAIL_NULL(light);
    light->shadow = p_enabled;
}

void LightStorage::light_set_shadow_color(RID p_light, const Color &p_color) {
    Light *light = light_owner.get_or_null(p_light);
    ERR_FAIL_NULL(light);
    light->shadow_color = p_color;
}

void LightStorage::light_set_projector(RID p_light, RID p_texture) {
    Light *light = light_owner.get_or_null(p_light);
    ERR_FAIL_NULL(light);
    light->projector = p_texture;
}

void LightStorage::light_set_negative(RID p_light, bool p_enable) {
    Light *light = light_owner.get_or_null(p_light);
    ERR_FAIL_NULL(light);
    light->negative = p_enable;
}

void LightStorage::light_set_cull_mask(RID p_light, uint32_t p_mask) {
    Light *light = light_owner.get_or_null(p_light);
    ERR_FAIL_NULL(light);
    light->cull_mask = p_mask;
}

void LightStorage::light_set_distance_fade(RID p_light, bool p_enabled, float p_begin, float p_shadow, float p_length) {
    Light *light = light_owner.get_or_null(p_light);
    ERR_FAIL_NULL(light);
    light->distance_fade = p_enabled;
    light->distance_fade_begin = p_begin;
    light->distance_fade_shadow = p_shadow;
    light->distance_fade_length = p_length;
}

void LightStorage::light_set_reverse_cull_face_mode(RID p_light, bool p_enabled) {
    Light *light = light_owner.get_or_null(p_light);
    ERR_FAIL_NULL(light);
    light->reverse_cull = p_enabled;
}

void LightStorage::light_set_bake_mode(RID p_light, RS::LightBakeMode p_bake_mode) {
    Light *light = light_owner.get_or_null(p_light);
    ERR_FAIL_NULL(light);
    light->bake_mode = p_bake_mode;
}

void LightStorage::light_omni_set_shadow_mode(RID p_light, RS::LightOmniShadowMode p_mode) {
    Light *light = light_owner.get_or_null(p_light);
    ERR_FAIL_NULL(light);
    light->omni_shadow_mode = p_mode;
}

void LightStorage::light_directional_set_shadow_mode(RID p_light, RS::LightDirectionalShadowMode p_mode) {
    Light *light = light_owner.get_or_null(p_light);
    ERR_FAIL_NULL(light);
    light->directional_shadow_mode = p_mode;
}

void LightStorage::light_directional_set_blend_splits(RID p_light, bool p_enable) {
    Light *light = light_owner.get_or_null(p_light);
    ERR_FAIL_NULL(light);
    light->directional_blend_splits = p_enable;
}

RS::LightDirectionalShadowMode LightStorage::light_directional_get_shadow_mode(RID p_light) {
    const Light *light = light_owner.get_or_null(p_light);
    ERR_FAIL_NULL_V(light, RS::LIGHT_DIRECTIONAL_SHADOW_ORTHOGONAL);
    return light->directional_shadow_mode;
}

RS::LightOmniShadowMode LightStorage::light_omni_get_shadow_mode(RID p_light) {
    const Light *light = light_owner.get_or_null(p_light);
    ERR_FAIL_NULL_V(light, RS::LIGHT_OMNI_SHADOW_DUAL_PARABOLOID);
    return light->omni_shadow_mode;
}

RS::LightBakeMode LightStorage::light_get_bake_mode(RID p_light) {
    const Light *light = light_owner.get_or_null(p_light);
    ERR_FAIL_NULL_V(light, RS::LIGHT_BAKE_DISABLED);
    return light->bake_mode;
}

uint64_t LightStorage::light_get_version(RID p_light) const {
    const Light *light = light_owner.get_or_null(p_light);
    ERR_FAIL_NULL_V(light, 0);
    return light->version;
}

AABB LightStorage::light_get_aabb(RID p_light) const {
    const Light *light = light_owner.get_or_null(p_light);
    ERR_FAIL_NULL_V(light, AABB());

    switch (light->type) {
        case RS::LIGHT_SPOT: {
            float len = light->param[RS::LIGHT_PARAM_RANGE];
            float size = Math::tan(Math::deg2rad(light->param[RS::LIGHT_PARAM_SPOT_ANGLE])) * len;
            return AABB(Vector3(-size, -size, -len), Vector3(size * 2, size * 2, len));
        }
        case RS::LIGHT_OMNI: {
            float r = light->param[RS::LIGHT_PARAM_RANGE];
            return AABB(-Vector3(r, r, r), Vector3(r, r, r) * 2);
        }
        case RS::LIGHT_DIRECTIONAL: {
            return AABB();
        }
    }

    ERR_FAIL_V(AABB());
}

void LightStorage::update_light_buffers() {
    // Reset light counts
    int light_count = 0;
    int directional_light_count = 0;

    // Prepare light data for non-directional lights
    light_data.clear();

    for (int i = 0; i < light_instance_owner.get_rid_count(); i++) {
        if (!light_instance_owner.is_rid_valid(i)) {
            continue;
        }

        LightInstance *li = light_instance_owner.get_ptr(i);
        Light *l = light_owner.get_or_null(li->light);

        if (l->type != RS::LIGHT_DIRECTIONAL) {
            if (light_count >= Config::get_singleton()->max_renderable_lights) {
                continue;
            }

            LightData &ld = light_data.write[light_count];
            Transform3D light_transform = li->transform;

            ld.position[0] = light_transform.origin.x;
            ld.position[1] = light_transform.origin.y;
            ld.position[2] = light_transform.origin.z;

            ld.color[0] = l->color.r * l->energy;
            ld.color[1] = l->color.g * l->energy;
            ld.color[2] = l->color.b * l->energy;

            ld.flags = l->negative ? 1 : 0;
            if (l->shadow) {
                ld.flags |= 2;
            }

            ld.inv_radius = 1.0f / l->param[RS::LIGHT_PARAM_RANGE];

            if (l->type == RS::LIGHT_SPOT) {
                Vector3 direction = light_transform.basis.xform(Vector3(0, 0, -1));
                ld.direction[0] = direction.x;
                ld.direction[1] = direction.y;
                ld.direction[2] = direction.z;

                ld.spot_attenuation = l->param[RS::LIGHT_PARAM_SPOT_ATTENUATION];
                ld.spot_angle = Math::cos(Math::deg2rad(l->param[RS::LIGHT_PARAM_SPOT_ANGLE]));
            }

            ld.specular = l->param[RS::LIGHT_PARAM_SPECULAR];
            ld.shadow_color[0] = l->shadow_color.r;
            ld.shadow_color[1] = l->shadow_color.g;
            ld.shadow_color[2] = l->shadow_color.b;

            light_count++;
        } else {
            if (directional_light_count >= Config::get_singleton()->max_renderable_lights) {
                continue;
            }

            DirectionalLightData &dld = directional_light_data.write[directional_light_count];
            Vector3 direction = li->transform.basis.xform(Vector3(0, 0, -1));

            dld.direction[0] = direction.x;
            dld.direction[1] = direction.y;
            dld.direction[2] = direction.z;
            dld.energy = l->param[RS::LIGHT_PARAM_ENERGY];

            dld.color[0] = l->color.r;
            dld.color[1] = l->color.g;
            dld.color[2] = l->color.b;

            dld.specular = l->param[RS::LIGHT_PARAM_SPECULAR];
            dld.flags = l->negative ? 1 : 0;
            if (l->shadow) {
                dld.flags |= 2;
            }

            directional_light_count++;
        }
    }

    // Update UBOs
    if (light_count > 0) {
        glBindBuffer(GL_UNIFORM_BUFFER, light_ubo);
        glBufferData(GL_UNIFORM_BUFFER, sizeof(LightData) * light_count, light_data.ptr(), GL_DYNAMIC_DRAW);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }

    if (directional_light_count > 0) {
        glBindBuffer(GL_UNIFORM_BUFFER, directional_light_ubo);
        glBufferData(GL_UNIFORM_BUFFER, sizeof(DirectionalLightData) * directional_light_count, directional_light_data.ptr(), GL_DYNAMIC_DRAW);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }
}

/* LIGHT INSTANCE API */

RID LightStorage::light_instance_create(RID p_light) {
    RID rid = light_instance_owner.make_rid();
    LightInstance *light_instance = light_instance_owner.get_or_null(rid);
    light_instance->light = p_light;
    light_instance->light_type = light_get_type(p_light);

    return rid;
}

void LightStorage::light_instance_free(RID p_light_instance) {
    LightInstance *light_instance = light_instance_owner.get_or_null(p_light_instance);
    ERR_FAIL_NULL(light_instance);
    light_instance_owner.free(p_light_instance);
}

void LightStorage::light_instance_set_transform(RID p_light_instance, const Transform3D &p_transform) {
    LightInstance *light_instance = light_instance_owner.get_or_null(p_light_instance);
    ERR_FAIL_NULL(light_instance);

    light_instance->transform = p_transform;
}

void LightStorage::light_instance_set_aabb(RID p_light_instance, const AABB &p_aabb) {
    LightInstance *light_instance = light_instance_owner.get_or_null(p_light_instance);
    ERR_FAIL_NULL(light_instance);

    // TODO: Store AABB if needed for culling or other purposes
}

void LightStorage::light_instance_mark_visible(RID p_light_instance) {
    LightInstance *light_instance = light_instance_owner.get_null(p_light_instance);
    ERR_FAIL_NULL(light_instance);

    light_instance->last_scene_pass = RenderingServer::get_singleton()->get_frame_number();
}

/* PROBE API */

RID LightStorage::reflection_probe_allocate() {
    return RID();
}

void LightStorage::reflection_probe_initialize(RID p_rid) {
}

void LightStorage::reflection_probe_free(RID p_rid) {
}

/* REFLECTION PROBE INSTANCE */

RID LightStorage::reflection_probe_instance_create(RID p_probe) {
    return RID();
}

void LightStorage::reflection_probe_instance_free(RID p_instance) {
}

void LightStorage::reflection_probe_instance_set_transform(RID p_instance, const Transform3D &p_transform) {
}

void LightStorage::reflection_probe_release_atlas_index(RID p_instance) {
}

bool LightStorage::reflection_probe_instance_needs_redraw(RID p_instance) {
    return false;
}

bool LightStorage::reflection_probe_instance_has_reflection(RID p_instance) {
    return false;
}

bool LightStorage::reflection_probe_instance_begin_render(RID p_instance, RID p_reflection_atlas) {
    return false;
}

Ref<RenderSceneBuffers> LightStorage::reflection_probe_atlas_get_render_buffers(RID p_reflection_atlas) {
    return Ref<RenderSceneBuffers>();
}

bool LightStorage::reflection_probe_instance_postprocess_step(RID p_instance) {
    return true;
}

/* LIGHTMAP */

RID LightStorage::lightmap_allocate() {
    return RID();
}

void LightStorage::lightmap_initialize(RID p_rid) {
}

void LightStorage::lightmap_free(RID p_rid) {
}

void LightStorage::lightmap_set_textures(RID p_lightmap, RID p_light, bool p_uses_spherical_haromics) {
}

void LightStorage::lightmap_set_probe_capture_data(RID p_lightmap, const PackedVector3Array &p_points, const PackedColorArray &p_point_sh, const PackedInt32Array &p_tetrahedra, const PackedInt32Array &p_bsp_tree) {
}

void LightStorage::lightmap_set_probe_bounds(RID p_lightmap, const AABB &p_bounds) {
}

void LightStorage::lightmap_set_probe_interior(RID p_lightmap, bool p_interior) {
}

void LightStorage::lightmap_set_baked_exposure_normalization(RID p_lightmap, float p_exposure) {
}

PackedVector3Array LightStorage::lightmap_get_probe_capture_points(RID p_lightmap) const {
    return PackedVector3Array();
}

PackedColorArray LightStorage::lightmap_get_probe_capture_sh(RID p_lightmap) const {
    return PackedColorArray();
}

PackedInt32Array LightStorage::lightmap_get_probe_capture_tetrahedra(RID p_lightmap) const {
    return PackedInt32Array();
}

PackedInt32Array LightStorage::lightmap_get_probe_capture_bsp_tree(RID p_lightmap) const {
    return PackedInt32Array();
}

AABB LightStorage::lightmap_get_aabb(RID p_lightmap) const {
    return AABB();
}

void LightStorage::lightmap_tap_sh_light(RID p_lightmap, const Vector3 &p_point, Color *r_sh) {
}

bool LightStorage::lightmap_is_interior(RID p_lightmap) const {
    return false;
}

void LightStorage::lightmap_set_probe_capture_update_speed(float p_speed) {
}

float LightStorage::lightmap_get_probe_capture_update_speed() const {
    return 0;
}

/* LIGHTMAP INSTANCE */

RID LightStorage::lightmap_instance_create(RID p_lightmap) {
    return RID();
}

void LightStorage::lightmap_instance_free(RID p_lightmap) {
}

void LightStorage::lightmap_instance_set_transform(RID p_lightmap, const Transform3D &p_transform) {
}

/* SHADOW ATLAS API */

RID LightStorage::shadow_atlas_create() {
    return RID();
}

void LightStorage::shadow_atlas_free(RID p_atlas) {
}

void LightStorage::shadow_atlas_set_size(RID p_atlas, int p_size, bool p_16_bits) {
}

void LightStorage::shadow_atlas_set_quadrant_subdivision(RID p_atlas, int p_quadrant, int p_subdivision) {
}

bool LightStorage::shadow_atlas_update_light(RID p_atlas, RID p_light_intance, float p_coverage, uint64_t p_light_version) {
    return false;
}

void LightStorage::shadow_atlas_update(RID p_atlas) {
}

void LightStorage::directional_shadow_atlas_set_size(int p_size, bool p_16_bits) {
}

int LightStorage::get_directional_light_shadow_size(RID p_light_intance) {
    return 0;
}

void LightStorage::set_directional_shadow_count(int p_count) {
}

void LightStorage::update_directional_shadow_atlas() {
    if (directional_shadow.size == 0 || directional_shadow.fbo == 0) {
        return;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, directional_shadow.fbo);
    glViewport(0, 0, directional_shadow.size, directional_shadow.size);
    glClearColor(1.0, 1.0, 1.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void LightStorage::directional_shadow_atlas_set_size(int p_size, bool p_16_bits) {
    if (directional_shadow.size == p_size && directional_shadow.use_16_bits == p_16_bits) {
        return;
    }

    directional_shadow.size = p_size;
    directional_shadow.use_16_bits = p_16_bits;

    if (directional_shadow.fbo != 0) {
        glDeleteFramebuffers(1, &directional_shadow.fbo);
        directional_shadow.fbo = 0;
    }

    if (directional_shadow.depth != 0) {
        glDeleteTextures(1, &directional_shadow.depth);
        directional_shadow.depth = 0;
    }

    if (p_size > 0) {
        glGenFramebuffers(1, &directional_shadow.fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, directional_shadow.fbo);

        glGenTextures(1, &directional_shadow.depth);
        glBindTexture(GL_TEXTURE_2D, directional_shadow.depth);

        if (p_16_bits) {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16, p_size, p_size, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, NULL);
        } else {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, p_size, p_size, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, NULL);
        }

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, directional_shadow.depth, 0);

        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            ERR_PRINT("Directional shadow framebuffer status invalid");
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}

void LightStorage::set_directional_shadow_count(int p_count) {
    directional_shadow.light_count = p_count;
    directional_shadow.current_light = 0;
}

int LightStorage::get_directional_light_shadow_size(RID p_light_instance) {
    ERR_FAIL_COND_V(directional_shadow.light_count == 0, 0);

    int shadow_size = directional_shadow.size;

    LightInstance *light_instance = light_instance_owner.get_or_null(p_light_instance);
    ERR_FAIL_NULL_V(light_instance, 0);

    Light *light = light_owner.get_or_null(light_instance->light);
    ERR_FAIL_NULL_V(light, 0);

    switch (light->directional_shadow_mode) {
        case RS::LIGHT_DIRECTIONAL_SHADOW_ORTHOGONAL:
            break;
        case RS::LIGHT_DIRECTIONAL_SHADOW_PARALLEL_2_SPLITS:
        case RS::LIGHT_DIRECTIONAL_SHADOW_PARALLEL_4_SPLITS:
            shadow_size /= 2;
            break;
    }

    return shadow_size;
}

Rect2i LightStorage::get_directional_shadow_rect() {
    Rect2i ret;

    int shadow_size = directional_shadow.size;
    switch (directional_shadow.light_count) {
        case 1: {
            ret = Rect2i(0, 0, shadow_size, shadow_size);
        } break;
        case 2: {
            ret = Rect2i(0, 0, shadow_size, shadow_size / 2);
            if (directional_shadow.current_light == 1) {
                ret.position.y += ret.size.y;
            }
        } break;
        case 3:
        case 4: {
            ret = Rect2i(0, 0, shadow_size / 2, shadow_size / 2);
            if (directional_shadow.current_light & 1) {
                ret.position.x += ret.size.x;
            }
            if (directional_shadow.current_light / 2) {
                ret.position.y += ret.size.y;
            }
        } break;
    }

    return ret;
}

void LightStorage::render_shadow_pass(RID p_light, RID p_shadow_atlas, int p_pass, const PagedArray<RenderGeometryInstance *> &p_instances, const Plane &p_camera_plane, float p_lod_distance_multiplier, float p_screen_mesh_lod_threshold, bool p_open_pass, bool p_close_pass, bool p_clear_region) {
    LightInstance *light_instance = light_instance_owner.get_or_null(p_light);
    ERR_FAIL_NULL(light_instance);

    Light *light = light_owner.get_or_null(light_instance->light);
    ERR_FAIL_NULL(light);

    if (light->type == RS::LIGHT_DIRECTIONAL) {
        // Render directional light shadow
        Rect2i rect = get_directional_shadow_rect();

        glBindFramebuffer(GL_FRAMEBUFFER, directional_shadow.fbo);
        glViewport(rect.position.x, rect.position.y, rect.size.width, rect.size.height);

        if (p_clear_region) {
            glEnable(GL_SCISSOR_TEST);
            glScissor(rect.position.x, rect.position.y, rect.size.width, rect.size.height);
            glClearColor(1.0, 1.0, 1.0, 1.0);
            glClear(GL_DEPTH_BUFFER_BIT);
            glDisable(GL_SCISSOR_TEST);
        }

        // TODO: Render shadow casters

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    } else {
        // TODO: Render omni or spot light shadow
    }
}

void LightStorage::update_shadow_atlas(RID p_atlas) {
}

void LightStorage::update_configurations() {
    // Update light configurations if needed
    update_light_buffers();
}

void LightStorage::setup_added_light(RID p_light) {
    Light *light = light_owner.get_or_null(p_light);
    ERR_FAIL_NULL(light);

    // Initialize default light parameters
    light->param[RS::LIGHT_PARAM_ENERGY] = 1.0;
    light->param[RS::LIGHT_PARAM_INDIRECT_ENERGY] = 1.0;
    light->param[RS::LIGHT_PARAM_SPECULAR] = 0.5;
    light->param[RS::LIGHT_PARAM_RANGE] = 5.0;
    light->param[RS::LIGHT_PARAM_SPOT_ANGLE] = 45.0;
    light->param[RS::LIGHT_PARAM_SPOT_ATTENUATION] = 1.0;
    light->param[RS::LIGHT_PARAM_ATTENUATION] = 1.0;
    light->param[RS::LIGHT_PARAM_SHADOW_MAX_DISTANCE] = 0.0;
    light->param[RS::LIGHT_PARAM_SHADOW_SPLIT_1_OFFSET] = 0.1;
    light->param[RS::LIGHT_PARAM_SHADOW_SPLIT_2_OFFSET] = 0.3;
    light->param[RS::LIGHT_PARAM_SHADOW_SPLIT_3_OFFSET] = 0.6;
    light->param[RS::LIGHT_PARAM_SHADOW_FADE_START] = 0.8;
    light->param[RS::LIGHT_PARAM_SHADOW_BIAS] = 0.02;
    light->param[RS::LIGHT_PARAM_SHADOW_NORMAL_BIAS] = 1.0;

    light->color = Color(1, 1, 1, 1);
    light->shadow = false;
    light->negative = false;
    light->cull_mask = 0xFFFFFFFF;
    light->directional_shadow_mode = RS::LIGHT_DIRECTIONAL_SHADOW_ORTHOGONAL;
    light->directional_blend_splits = false;
    light->omni_shadow_mode = RS::LIGHT_OMNI_SHADOW_DUAL_PARABOLOID;
    light->bake_mode = RS::LIGHT_BAKE_DYNAMIC;
    light->version = 0;
}

#endif // GLES2_ENABLED
