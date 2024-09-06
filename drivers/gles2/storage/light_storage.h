/**************************************************************************/
/*  light_storage.h                                                       */
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

#ifndef LIGHT_STORAGE_GLES2_H
#define LIGHT_STORAGE_GLES2_H

#ifdef GLES2_ENABLED

#include "servers/rendering/storage/light_storage.h"
#include "servers/rendering/renderer_compositor.h"
#include "servers/rendering/storage/utilities.h"
#include "drivers/gles2/shader_gles2.h"
#include "core/templates/local_vector.h"
#include "core/templates/rid_owner.h"
#include "core/templates/self_list.h"

class GLES2Context;

namespace GLES2 {

/* LIGHT */
struct Light {
    RS::LightType type;
    float param[RS::LIGHT_PARAM_MAX];
    Color color;
    Color shadow_color;
    RID projector;
    bool shadow = false;
    bool negative = false;
    bool reverse_cull = false;
    RS::LightBakeMode bake_mode = RS::LIGHT_BAKE_DYNAMIC;
    uint32_t cull_mask = 0xFFFFFFFF;
    bool distance_fade = false;
    float distance_fade_begin = 40.0;
    float distance_fade_shadow = 50.0;
    float distance_fade_length = 10.0;
    RS::LightOmniShadowMode omni_shadow_mode = RS::LIGHT_OMNI_SHADOW_DUAL_PARABOLOID;
    RS::LightDirectionalShadowMode directional_shadow_mode = RS::LIGHT_DIRECTIONAL_SHADOW_ORTHOGONAL;
    bool directional_blend_splits = false;
    uint64_t version = 0;

    Dependency dependency;
};

/* LIGHT INSTANCE */
struct LightInstance {
    RS::LightType light_type = RS::LIGHT_DIRECTIONAL;
    RID light;
    Transform3D transform;
    Color color;
    float energy = 1.0;
    float indirect_energy = 1.0;
    bool shadow = false;
    int32_t shadow_id = -1;

    Rect2 directional_rect;

    uint64_t last_scene_pass = 0;
    uint64_t last_scene_shadow_pass = 0;

    uint64_t last_pass = 0;
    uint32_t light_index = 0;
    uint32_t cull_mask = 0;

    // No shadow shadow atlases for GLES2
};

class LightStorage : public RendererLightStorage {
private:
    static LightStorage *singleton;

    mutable RID_Owner<Light, true> light_owner;
    mutable RID_Owner<LightInstance> light_instance_owner;

    /* OMNI/SPOT LIGHT DATA */
    struct LightData {
        float position[3];
        float inv_radius;
        float direction[3];
        float attenuation;
        float color[3];
        float shadow_color[3];
        float spot_attenuation;
        float spot_angle;
        float specular;
        uint32_t flags;
    };

    Vector<LightData> light_data;
    GLuint light_ubo = 0;

    /* DIRECTIONAL LIGHT DATA */
    struct DirectionalLightData {
        float direction[3];
        float energy;
        float color[3];
        float specular;
        uint32_t flags;
    };

    Vector<DirectionalLightData> directional_light_data;
    GLuint directional_light_ubo = 0;

public:
    static LightStorage *get_singleton();

    LightStorage();
    ~LightStorage();

    /* LIGHT */
    RID directional_light_allocate() override;
    void directional_light_initialize(RID p_light) override;
    RID omni_light_allocate() override;
    void omni_light_initialize(RID p_rid) override;
    RID spot_light_allocate() override;
    void spot_light_initialize(RID p_rid) override;

    void light_free(RID p_rid) override;

    void light_set_color(RID p_light, const Color &p_color) override;
    void light_set_param(RID p_light, RS::LightParam p_param, float p_value) override;
    void light_set_shadow(RID p_light, bool p_enabled) override;
    void light_set_shadow_color(RID p_light, const Color &p_color);
    void light_set_projector(RID p_light, RID p_texture) override;
    void light_set_negative(RID p_light, bool p_enable) override;
    void light_set_cull_mask(RID p_light, uint32_t p_mask) override;
    void light_set_distance_fade(RID p_light, bool p_enabled, float p_begin, float p_shadow, float p_length) override;
    void light_set_reverse_cull_face_mode(RID p_light, bool p_enabled) override;
    void light_set_bake_mode(RID p_light, RS::LightBakeMode p_bake_mode) override;

    void light_omni_set_shadow_mode(RID p_light, RS::LightOmniShadowMode p_mode) override;

    void light_directional_set_shadow_mode(RID p_light, RS::LightDirectionalShadowMode p_mode) override;
    void light_directional_set_blend_splits(RID p_light, bool p_enable) override;

    RS::LightDirectionalShadowMode light_directional_get_shadow_mode(RID p_light) override;
    RS::LightOmniShadowMode light_omni_get_shadow_mode(RID p_light) override;

    RS::LightType light_get_type(RID p_light) const override {
        const Light *light = light_owner.get_or_null(p_light);
        ERR_FAIL_NULL_V(light, RS::LIGHT_DIRECTIONAL);

        return light->type;
    }
    AABB light_get_aabb(RID p_light) const override;

    float light_get_param(RID p_light, RS::LightParam p_param) override {
        const Light *light = light_owner.get_or_null(p_light);
        ERR_FAIL_NULL_V(light, 0);

        return light->param[p_param];
    }

    _FORCE_INLINE_ RID light_get_projector(RID p_light) {
        const Light *light = light_owner.get_or_null(p_light);
        ERR_FAIL_NULL_V(light, RID());

        return light->projector;
    }

    Color light_get_color(RID p_light) override {
        const Light *light = light_owner.get_or_null(p_light);
        ERR_FAIL_NULL_V(light, Color());

        return light->color;
    }

    RS::LightBakeMode light_get_bake_mode(RID p_light) override;
    uint64_t light_get_version(RID p_light) const override;

    void update_light_buffers();
};

} // namespace GLES2

#endif // GLES2_ENABLED

#endif // LIGHT_STORAGE_GLES2_H
