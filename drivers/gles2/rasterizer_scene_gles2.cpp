/**************************************************************************/
/*  rasterizer_scene_gles2.cpp                                            */
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

#include "rasterizer_scene_gles2.h"

#ifdef GLES2_ENABLED

#include "drivers/gles2/effects/copy_effects.h"
#include "rasterizer_gles2.h"
#include "storage/config.h"
#include "storage/mesh_storage.h"
#include "storage/particles_storage.h"
#include "storage/texture_storage.h"

#include "core/config/project_settings.h"
#include "core/templates/sort_array.h"
#include "servers/rendering/rendering_server_default.h"
#include "servers/rendering/rendering_server_globals.h"

RasterizerSceneGLES2 *RasterizerSceneGLES2::singleton = nullptr;

RenderGeometryInstance *RasterizerSceneGLES2::geometry_instance_create(RID p_base) {
    RS::InstanceType type = RSG::utilities->get_base_type(p_base);
    ERR_FAIL_COND_V(!((1 << type) & RS::INSTANCE_GEOMETRY_MASK), nullptr);

    GeometryInstanceGLES2 *ginstance = geometry_instance_alloc.alloc();
    ginstance->data = memnew(GeometryInstanceGLES2::Data);

    ginstance->data->base = p_base;
    ginstance->data->base_type = type;
    ginstance->data->dependency_tracker.userdata = ginstance;
    ginstance->data->dependency_tracker.changed_callback = _geometry_instance_dependency_changed;
    ginstance->data->dependency_tracker.deleted_callback = _geometry_instance_dependency_deleted;

    ginstance->_mark_dirty();

    return ginstance;
}

uint32_t RasterizerSceneGLES2::geometry_instance_get_pair_mask() {
    return (1 << RS::INSTANCE_LIGHT) | (1 << RS::INSTANCE_REFLECTION_PROBE);
}

void RasterizerSceneGLES2::GeometryInstanceGLES2::pair_light_instances(const RID *p_light_instances, uint32_t p_light_instance_count) {
    // Simplified for GLES2
    paired_omni_light_count = 0;
    paired_spot_light_count = 0;
    paired_omni_lights.clear();
    paired_spot_lights.clear();

    for (uint32_t i = 0; i < p_light_instance_count; i++) {
        RS::LightType type = GLES2::LightStorage::get_singleton()->light_instance_get_type(p_light_instances[i]);
        switch (type) {
            case RS::LIGHT_OMNI: {
                if (paired_omni_light_count < (uint32_t)GLES2::Config::get_singleton()->max_lights_per_object) {
                    paired_omni_lights.push_back(p_light_instances[i]);
                    paired_omni_light_count++;
                }
            } break;
            case RS::LIGHT_SPOT: {
                if (paired_spot_light_count < (uint32_t)GLES2::Config::get_singleton()->max_lights_per_object) {
                    paired_spot_lights.push_back(p_light_instances[i]);
                    paired_spot_light_count++;
                }
            } break;
            default:
                break;
        }
    }
}

void RasterizerSceneGLES2::GeometryInstanceGLES2::pair_reflection_probe_instances(const RID *p_reflection_probe_instances, uint32_t p_reflection_probe_instance_count) {
    paired_reflection_probes.clear();

    for (uint32_t i = 0; i < p_reflection_probe_instance_count; i++) {
        paired_reflection_probes.push_back(p_reflection_probe_instances[i]);
    }
}

void RasterizerSceneGLES2::GeometryInstanceGLES2::set_use_lightmap(RID p_lightmap_instance, const Rect2 &p_lightmap_uv_scale, int p_lightmap_slice_index) {
    lightmap_instance = p_lightmap_instance;
    lightmap_uv_scale = p_lightmap_uv_scale;
    lightmap_slice_index = p_lightmap_slice_index;

    _mark_dirty();
}

void RasterizerSceneGLES2::GeometryInstanceGLES2::set_lightmap_capture(const Color *p_sh9) {
    if (p_sh9) {
        if (lightmap_sh == nullptr) {
            lightmap_sh = memnew(GeometryInstanceLightmapSH);
        }

        memcpy(lightmap_sh->sh, p_sh9, sizeof(Color) * 9);
    } else {
        if (lightmap_sh != nullptr) {
            memdelete(lightmap_sh);
            lightmap_sh = nullptr;
        }
    }
    _mark_dirty();
}

void RasterizerSceneGLES2::_update_dirty_geometry_instances() {
    while (geometry_instance_dirty_list.first()) {
        _geometry_instance_update(geometry_instance_dirty_list.first()->self());
    }
}

void RasterizerSceneGLES2::_geometry_instance_dependency_changed(Dependency::DependencyChangedNotification p_notification, DependencyTracker *p_tracker) {
    switch (p_notification) {
        case Dependency::DEPENDENCY_CHANGED_MATERIAL:
        case Dependency::DEPENDENCY_CHANGED_MESH:
        case Dependency::DEPENDENCY_CHANGED_PARTICLES:
        case Dependency::DEPENDENCY_CHANGED_MULTIMESH:
        case Dependency::DEPENDENCY_CHANGED_SKELETON_DATA: {
            static_cast<RenderGeometryInstance *>(p_tracker->userdata)->_mark_dirty();
            static_cast<GeometryInstanceGLES2 *>(p_tracker->userdata)->data->dirty_dependencies = true;
        } break;
        case Dependency::DEPENDENCY_CHANGED_MULTIMESH_VISIBLE_INSTANCES: {
            GeometryInstanceGLES2 *ginstance = static_cast<GeometryInstanceGLES2 *>(p_tracker->userdata);
            if (ginstance->data->base_type == RS::INSTANCE_MULTIMESH) {
                ginstance->instance_count = GLES2::MeshStorage::get_singleton()->multimesh_get_instances_to_draw(ginstance->data->base);
            }
        } break;
        default: {
            //rest of notifications of no interest
        } break;
    }
}

void RasterizerSceneGLES2::_geometry_instance_dependency_deleted(const RID &p_dependency, DependencyTracker *p_tracker) {
    static_cast<RenderGeometryInstance *>(p_tracker->userdata)->_mark_dirty();
    static_cast<GeometryInstanceGLES2 *>(p_tracker->userdata)->data->dirty_dependencies = true;
}

void RasterizerSceneGLES2::_geometry_instance_add_surface_with_material(GeometryInstanceGLES2 *ginstance, uint32_t p_surface, GLES2::SceneMaterialData *p_material, uint32_t p_material_id, uint32_t p_shader_id, RID p_mesh) {
    GLES2::MeshStorage *mesh_storage = GLES2::MeshStorage::get_singleton();

    bool has_alpha = p_material->shader_data->uses_alpha || p_material->shader_data->uses_depth_pre_pass;

    uint32_t flags = 0;

    if (p_material->shader_data->uses_screen_texture) {
        flags |= GeometryInstanceSurface::FLAG_USES_SCREEN_TEXTURE;
    }

    if (p_material->shader_data->uses_depth_texture) {
        flags |= GeometryInstanceSurface::FLAG_USES_DEPTH_TEXTURE;
    }

    if (p_material->shader_data->uses_normal_texture) {
        flags |= GeometryInstanceSurface::FLAG_USES_NORMAL_TEXTURE;
    }

    if (ginstance->data->cast_double_sided_shadows) {
        flags |= GeometryInstanceSurface::FLAG_USES_DOUBLE_SIDED_SHADOWS;
    }

    if (has_alpha || p_material->shader_data->depth_draw == GLES2::SceneShaderData::DEPTH_DRAW_DISABLED) {
        flags |= GeometryInstanceSurface::FLAG_PASS_ALPHA;
    } else {
        flags |= GeometryInstanceSurface::FLAG_PASS_OPAQUE;
    }

    flags |= GeometryInstanceSurface::FLAG_PASS_DEPTH;
    flags |= GeometryInstanceSurface::FLAG_PASS_SHADOW;

    GeometryInstanceSurface *sdcache = geometry_instance_surface_alloc.alloc();

    sdcache->flags = flags;

    sdcache->shader = p_material->shader_data;
    sdcache->material = p_material;
    sdcache->surface = mesh_storage->mesh_get_surface(p_mesh, p_surface);
    sdcache->primitive = mesh_storage->mesh_surface_get_primitive(sdcache->surface);
    sdcache->surface_index = p_surface;

    if (ginstance->data->dirty_dependencies) {
        RSG::utilities->base_update_dependency(p_mesh, &ginstance->data->dependency_tracker);
    }

    sdcache->owner = ginstance;

    sdcache->next = ginstance->surface_caches;
    ginstance->surface_caches = sdcache;

    //sortkey

    sdcache->sort_key = 0;
    sdcache->sort_key |= ((uint64_t)p_material_id) << (61 - 16);
    sdcache->sort_key |= ((uint64_t)p_shader_id) << (45 - 16);
    sdcache->sort_key |= ((uint64_t)p_surface) << (45 - 32);

    GLES2::Mesh::Surface *s = reinterpret_cast<GLES2::Mesh::Surface *>(sdcache->surface);
    if (p_material->shader_data->uses_tangent && !(s->format & RS::ARRAY_FORMAT_TANGENT)) {
        // Avoid warnings about missing tangents in GLES2
    }
}

void RasterizerSceneGLES2::_geometry_instance_add_surface_with_material_chain(GeometryInstanceGLES2 *ginstance, uint32_t p_surface, GLES2::SceneMaterialData *p_material_data, RID p_mat_src, RID p_mesh) {
    GLES2::SceneMaterialData *material_data = p_material_data;
    GLES2::MaterialStorage *material_storage = GLES2::MaterialStorage::get_singleton();

    _geometry_instance_add_surface_with_material(ginstance, p_surface, material_data, p_mat_src.get_local_index(), material_storage->material_get_shader_id(p_mat_src), p_mesh);

    while (material_data->next_pass.is_valid()) {
        RID next_pass = material_data->next_pass;
        material_data = static_cast<GLES2::SceneMaterialData *>(material_storage->material_get_data(next_pass, RS::SHADER_SPATIAL));
        if (!material_data || !material_data->shader_data->valid) {
            break;
        }
        if (ginstance->data->dirty_dependencies) {
            material_storage->material_update_dependency(next_pass, &ginstance->data->dependency_tracker);
        }
        _geometry_instance_add_surface_with_material(ginstance, p_surface, material_data, next_pass.get_local_index(), material_storage->material_get_shader_id(next_pass), p_mesh);
    }
}

void RasterizerSceneGLES2::_geometry_instance_add_surface(GeometryInstanceGLES2 *ginstance, uint32_t p_surface, RID p_material, RID p_mesh) {
    GLES2::MaterialStorage *material_storage = GLES2::MaterialStorage::get_singleton();
    RID m_src;

    m_src = ginstance->data->material_override.is_valid() ? ginstance->data->material_override : p_material;

    GLES2::SceneMaterialData *material_data = nullptr;

    if (m_src.is_valid()) {
        material_data = static_cast<GLES2::SceneMaterialData *>(material_storage->material_get_data(m_src, RS::SHADER_SPATIAL));
        if (!material_data || !material_data->shader_data->valid) {
            material_data = nullptr;
        }
    }

    if (material_data) {
        if (ginstance->data->dirty_dependencies) {
            material_storage->material_update_dependency(m_src, &ginstance->data->dependency_tracker);
        }
    } else {
        material_data = static_cast<GLES2::SceneMaterialData *>(material_storage->material_get_data(scene_globals.default_material, RS::SHADER_SPATIAL));
        m_src = scene_globals.default_material;
    }

    ERR_FAIL_NULL(material_data);

    _geometry_instance_add_surface_with_material_chain(ginstance, p_surface, material_data, m_src, p_mesh);

    if (ginstance->data->material_overlay.is_valid()) {
        m_src = ginstance->data->material_overlay;

        material_data = static_cast<GLES2::SceneMaterialData *>(material_storage->material_get_data(m_src, RS::SHADER_SPATIAL));
        if (material_data && material_data->shader_data->valid) {
            if (ginstance->data->dirty_dependencies) {
                material_storage->material_update_dependency(m_src, &ginstance->data->dependency_tracker);
            }

            _geometry_instance_add_surface_with_material_chain(ginstance, p_surface, material_data, m_src, p_mesh);
        }
    }
}

void RasterizerSceneGLES2::_geometry_instance_update(RenderGeometryInstance *p_geometry_instance) {
    GLES2::MeshStorage *mesh_storage = GLES2::MeshStorage::get_singleton();
    GLES2::ParticlesStorage *particles_storage = GLES2::ParticlesStorage::get_singleton();

    GeometryInstanceGLES2 *ginstance = static_cast<GeometryInstanceGLES2 *>(p_geometry_instance);

    if (ginstance->data->dirty_dependencies) {
        ginstance->data->dependency_tracker.update_begin();
    }

    //add geometry for drawing
    switch (ginstance->data->base_type) {
        case RS::INSTANCE_MESH: {
            const RID *materials = nullptr;
            uint32_t surface_count;
            RID mesh = ginstance->data->base;

            materials = mesh_storage->mesh_get_surface_count_and_materials(mesh, surface_count);
            if (materials) {
                //if no materials, no surfaces.
                const RID *inst_materials = ginstance->data->surface_materials.ptr();
                uint32_t surf_mat_count = ginstance->data->surface_materials.size();

                for (uint32_t j = 0; j < surface_count; j++) {
                    RID material = (j < surf_mat_count && inst_materials[j].is_valid()) ? inst_materials[j] : materials[j];
                    _geometry_instance_add_surface(ginstance, j, material, mesh);
                }
            }

            ginstance->instance_count = 1;

        } break;

        case RS::INSTANCE_MULTIMESH: {
            RID mesh = mesh_storage->multimesh_get_mesh(ginstance->data->base);
            if (mesh.is_valid()) {
                const RID *materials = nullptr;
                uint32_t surface_count;

                materials = mesh_storage->mesh_get_surface_count_and_materials(mesh, surface_count);
                if (materials) {
                    for (uint32_t j = 0; j < surface_count; j++) {
                        _geometry_instance_add_surface(ginstance, j, materials[j], mesh);
                    }
                }

                ginstance->instance_count = mesh_storage->multimesh_get_instances_to_draw(ginstance->data->base);
            }

        } break;
        case RS::INSTANCE_PARTICLES: {
            int draw_passes = particles_storage->particles_get_draw_passes(ginstance->data->base);

            for (int j = 0; j < draw_passes; j++) {
                RID mesh = particles_storage->particles_get_draw_pass_mesh(ginstance->data->base, j);
                if (!mesh.is_valid()) {
                    continue;
                }

                const RID *materials = nullptr;
                uint32_t surface_count;

                materials = mesh_storage->mesh_get_surface_count_and_materials(mesh, surface_count);
                if (materials) {
                    for (uint32_t k = 0; k < surface_count; k++) {
                        _geometry_instance_add_surface(ginstance, k, materials[k], mesh);
                    }
                }
            }

            ginstance->instance_count = particles_storage->particles_get_amount(ginstance->data->base);
        } break;

        default: {
        }
    }

    //Fill owners
    for (GeometryInstanceSurface *surf : ginstance->surface_caches) {
        surf->owner = ginstance;
    }

    ginstance->sorted_based_on_dependency = false;

    if (ginstance->data->dirty_dependencies) {
        ginstance->data->dependency_tracker.update_end();
        ginstance->data->dirty_dependencies = false;
    }

    ginstance->dirty_list_element.remove_from_list();
}

void RasterizerSceneGLES2::_setup_lights(const RenderDataGLES2 *p_render_data, bool p_using_shadows, uint32_t &r_directional_light_count, uint32_t &r_omni_light_count, uint32_t &r_spot_light_count) {
    GLES2::LightStorage *light_storage = GLES2::LightStorage::get_singleton();
    GLES2::Config *config = GLES2::Config::get_singleton();

    const Transform3D inverse_transform = p_render_data->inv_cam_transform;

    const PagedArray<RID> &lights = *p_render_data->lights;

    r_directional_light_count = 0;
    r_omni_light_count = 0;
    r_spot_light_count = 0;

    for (int i = 0; i < (int)lights.size(); i++) {
        GLES2::LightInstance *li = GLES2::LightStorage::get_singleton()->get_light_instance(lights[i]);
        if (!li) {
            continue;
        }
        RID base = li->light;

        ERR_CONTINUE(base.is_null());

        RS::LightType type = light_storage->light_get_type(base);
        switch (type) {
            case RS::LIGHT_DIRECTIONAL: {
                if (r_directional_light_count < scene_state.max_directional_lights) {
                    DirectionalLightData &light_data = scene_state.directional_lights[r_directional_light_count];

                    Transform3D light_transform = li->transform;

                    Vector3 direction = inverse_transform.basis.xform(light_transform.basis.xform(Vector3(0, 0, 1))).normalized();

                    light_data.direction[0] = direction.x;
                    light_data.direction[1] = direction.y;
                    light_data.direction[2] = direction.z;

                    float sign = light_storage->light_is_negative(base) ? -1 : 1;

                    light_data.energy = sign * light_storage->light_get_param(base, RS::LIGHT_PARAM_ENERGY);

                    Color linear_col = light_storage->light_get_color(base);
                    light_data.color[0] = linear_col.r;
                    light_data.color[1] = linear_col.g;
                    light_data.color[2] = linear_col.b;

                    light_data.specular = light_storage->light_get_param(base, RS::LIGHT_PARAM_SPECULAR);

                    light_data.bake_mode = light_storage->light_get_bake_mode(base);

                    r_directional_light_count++;
                }
            } break;
            case RS::LIGHT_OMNI: {
                if (r_omni_light_count < config->max_renderable_lights) {
                    LightData &light_data = scene_state.omni_lights[r_omni_light_count];

                    Transform3D light_transform = li->transform;
                    Vector3 pos = inverse_transform.xform(light_transform.origin);

                    light_data.position[0] = pos.x;
                    light_data.position[1] = pos.y;
                    light_data.position[2] = pos.z;

                    float radius = light_storage->light_get_param(base, RS::LIGHT_PARAM_RANGE);
                    light_data.inv_radius = 1.0 / radius;

                    float sign = light_storage->light_is_negative(base) ? -1 : 1;

                    light_data.energy = sign * light_storage->light_get_param(base, RS::LIGHT_PARAM_ENERGY);

                    Color linear_col = light_storage->light_get_color(base);
                    light_data.color[0] = linear_col.r;
                    light_data.color[1] = linear_col.g;
                    light_data.color[2] = linear_col.b;

                    light_data.specular = light_storage->light_get_param(base, RS::LIGHT_PARAM_SPECULAR);

                    light_data.bake_mode = light_storage->light_get_bake_mode(base);

                    r_omni_light_count++;
                }
            } break;
            case RS::LIGHT_SPOT: {
                if (r_spot_light_count < config->max_renderable_lights) {
                    LightData &light_data = scene_state.spot_lights[r_spot_light_count];

                    Transform3D light_transform = li->transform;
                    Vector3 pos = inverse_transform.xform(light_transform.origin);
                    Vector3 direction = inverse_transform.basis.xform(light_transform.basis.xform(Vector3(0, 0, -1))).normalized();

                    light_data.position[0] = pos.x;
                    light_data.position[1] = pos.y;
                    light_data.position[2] = pos.z;

                    light_data.direction[0] = direction.x;
                    light_data.direction[1] = direction.y;
                    light_data.direction[2] = direction.z;

                    float radius = light_storage->light_get_param(base, RS::LIGHT_PARAM_RANGE);
                    light_data.inv_radius = 1.0 / radius;

                    float sign = light_storage->light_is_negative(base) ? -1 : 1;

                    light_data.energy = sign * light_storage->light_get_param(base, RS::LIGHT_PARAM_ENERGY);

                    Color linear_col = light_storage->light_get_color(base);
                    light_data.color[0] = linear_col.r;
                    light_data.color[1] = linear_col.g;
                    light_data.color[2] = linear_col.b;

                    light_data.specular = light_storage->light_get_param(base, RS::LIGHT_PARAM_SPECULAR);

                    light_data.spot_angle = light_storage->light_get_param(base, RS::LIGHT_PARAM_SPOT_ANGLE);
                    light_data.spot_attenuation = light_storage->light_get_param(base, RS::LIGHT_PARAM_SPOT_ATTENUATION);

                    light_data.bake_mode = light_storage->light_get_bake_mode(base);

                    r_spot_light_count++;
                }
            } break;
        }
    }
}

void RasterizerSceneGLES2::_setup_environment(const RenderDataGLES2 *p_render_data, bool p_no_fog, const Size2i &p_screen_size, bool p_flip_y, const Color &p_default_bg_color, bool p_pancake_shadows) {
    // Setup UBO
    GLES2::MaterialStorage::store_camera(p_render_data->cam_projection, scene_state.ubo.projection_matrix);
    GLES2::MaterialStorage::store_camera(p_render_data->inv_cam_projection, scene_state.ubo.inv_projection_matrix);
    GLES2::MaterialStorage::store_transform(p_render_data->cam_transform, scene_state.ubo.camera_matrix);
    GLES2::MaterialStorage::store_transform(p_render_data->inv_cam_transform, scene_state.ubo.inv_camera_matrix);

    scene_state.ubo.viewport_size[0] = p_screen_size.x;
    scene_state.ubo.viewport_size[1] = p_screen_size.y;

    Size2 screen_pixel_size = Vector2(1.0, 1.0) / Size2(p_screen_size);
    scene_state.ubo.screen_pixel_size[0] = screen_pixel_size.x;
    scene_state.ubo.screen_pixel_size[1] = screen_pixel_size.y;

    if (p_render_data->camera_attributes.is_valid()) {
        // Setup camera attributes
    } else {
        // Default camera attributes
    }

    // Setup environment
    RID environment = p_render_data->environment;
    if (environment.is_valid()) {
        // Fog
        scene_state.ubo.fog_enabled = environment_get_fog_enabled(environment);
        if (!p_no_fog && scene_state.ubo.fog_enabled) {
            scene_state.ubo.fog_density = environment_get_fog_density(environment);
            scene_state.ubo.fog_height = environment_get_fog_height(environment);
            scene_state.ubo.fog_height_density = environment_get_fog_height_density(environment);
            scene_state.ubo.fog_depth = environment_get_fog_depth_end(environment);

            Color fog_color = environment_get_fog_light_color(environment).to_linear();
            float fog_energy = environment_get_fog_light_energy(environment);

            scene_state.ubo.fog_color_enabled[0] = fog_color.r * fog_energy;
            scene_state.ubo.fog_color_enabled[1] = fog_color.g * fog_energy;
            scene_state.ubo.fog_color_enabled[2] = fog_color.b * fog_energy;
        } else {
            scene_state.ubo.fog_color_enabled[3] = 0;
        }

        // Ambient light
        Color clear_color = environment_get_bg_color(environment);
        clear_color = clear_color.to_linear();

        scene_state.ubo.ambient_light_color_energy[0] = clear_color.r;
        scene_state.ubo.ambient_light_color_energy[1] = clear_color.g;
        scene_state.ubo.ambient_light_color_energy[2] = clear_color.b;
        scene_state.ubo.ambient_light_color_energy[3] = environment_get_bg_energy(environment);
    } else {
        // Default clear color
        Color clear_color = p_default_bg_color;
        clear_color = clear_color.to_linear();

        scene_state.ubo.ambient_light_color_energy[0] = clear_color.r;
        scene_state.ubo.ambient_light_color_energy[1] = clear_color.g;
        scene_state.ubo.ambient_light_color_energy[2] = clear_color.b;
        scene_state.ubo.ambient_light_color_energy[3] = 1.0;

        scene_state.ubo.fog_color_enabled[3] = 0;
    }

    // Update UBO
    glBindBufferBase(GL_UNIFORM_BUFFER, SCENE_DATA_UNIFORM_LOCATION, scene_state.ubo_buffer);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(SceneState::UBO), &scene_state.ubo, GL_STREAM_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    if (p_render_data->view_count > 1) {
        for (uint32_t v = 0; v < p_render_data->view_count; v++) {
            GLES2::MaterialStorage::store_camera(p_render_data->view_projection[v], scene_state.multiview_ubo.projection_matrix_view[v]);
            GLES2::MaterialStorage::store_camera(p_render_data->inv_view_projection[v], scene_state.multiview_ubo.inv_projection_matrix_view[v]);

            scene_state.multiview_ubo.eye_offset[v][0] = p_render_data->view_eye_offset[v].x;
            scene_state.multiview_ubo.eye_offset[v][1] = p_render_data->view_eye_offset[v].y;
            scene_state.multiview_ubo.eye_offset[v][2] = p_render_data->view_eye_offset[v].z;
            scene_state.multiview_ubo.eye_offset[v][3] = 0.0;
        }

        glBindBufferBase(GL_UNIFORM_BUFFER, SCENE_MULTIVIEW_UNIFORM_LOCATION, scene_state.multiview_buffer);
        glBufferData(GL_UNIFORM_BUFFER, sizeof(SceneState::MultiviewUBO), &scene_state.multiview_ubo, GL_STREAM_DRAW);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
    }
}

void RasterizerSceneGLES2::render_scene(const Ref<RenderSceneBuffers> &p_render_buffers, const CameraData *p_camera_data, const CameraData *p_prev_camera_data, const PagedArray<RenderGeometryInstance *> &p_instances, const PagedArray<RID> &p_lights, const PagedArray<RID> &p_reflection_probes, const PagedArray<RID> &p_voxel_gi_instances, const PagedArray<RID> &p_decals, const PagedArray<RID> &p_lightmaps, const PagedArray<RID> &p_fog_volumes, RID p_environment, RID p_camera_attributes, RID p_compositor, RID p_shadow_atlas, RID p_occluder_debug_tex, RID p_reflection_atlas, RID p_reflection_probe, int p_reflection_probe_pass, float p_screen_mesh_lod_threshold, const RenderShadowData *p_render_shadows, int p_render_shadow_count, const RenderSDFGIData *p_render_sdfgi_regions, int p_render_sdfgi_region_count, const RenderSDFGIUpdateData *p_sdfgi_update_data, RenderingMethod::RenderInfo *r_render_info) {
    GLES2::TextureStorage *texture_storage = GLES2::TextureStorage::get_singleton();
    GLES2::Config *config = GLES2::Config::get_singleton();

    RENDER_TIMESTAMP("Setup 3D Scene");

    Ref<RenderSceneBuffersGLES2> rb = p_render_buffers;
    ERR_FAIL_COND(rb.is_null());

    GLES2::RenderTarget *rt = texture_storage->get_render_target(rb->render_target);
    ERR_FAIL_NULL(rt);

    // Assign render data
    RenderDataGLES2 render_data;
    {
        render_data.render_buffers = rb;
        render_data.transparent_bg = rt->is_transparent;
        render_data.cam_transform = p_camera_data->main_transform;
        render_data.inv_cam_transform = render_data.cam_transform.affine_inverse();
        render_data.cam_projection = p_camera_data->main_projection;
        render_data.cam_orthogonal = p_camera_data->is_orthogonal;
        render_data.camera_visible_layers = p_camera_data->visible_layers;

        render_data.view_count = p_camera_data->view_count;
        for (uint32_t v = 0; v < p_camera_data->view_count; v++) {
            render_data.view_eye_offset[v] = p_camera_data->view_offset[v].origin;
            render_data.view_projection[v] = p_camera_data->view_projection[v];
        }

        render_data.z_near = p_camera_data->main_projection.get_z_near();
        render_data.z_far = p_camera_data->main_projection.get_z_far();

        render_data.instances = &p_instances;
        render_data.lights = &p_lights;
        render_data.reflection_probes = &p_reflection_probes;
        render_data.environment = p_environment;
        render_data.camera_attributes = p_camera_attributes;
        render_data.shadow_atlas = p_shadow_atlas;
        render_data.reflection_probe = p_reflection_probe;
        render_data.reflection_probe_pass = p_reflection_probe_pass;

        render_data.lod_distance_multiplier = p_camera_data->main_projection.get_lod_multiplier();

        render_data.screen_mesh_lod_threshold = p_screen_mesh_lod_threshold;
        render_data.render_info = r_render_info;
        render_data.render_shadows = p_render_shadows;
        render_data.render_shadow_count = p_render_shadow_count;
    }

    PagedArray<RID> empty;

    if (get_debug_draw_mode() == RS::VIEWPORT_DEBUG_DRAW_UNSHADED) {
        render_data.lights = &empty;
        render_data.reflection_probes = &empty;
    }

    // Setup environment
    _setup_environment(&render_data, false, rb->internal_size, p_camera_data->main_transform.basis.determinant() < 0, texture_storage->get_default_clear_color(), false);

    // Setup lights
    uint32_t directional_light_count = 0;
    uint32_t omni_light_count = 0;
    uint32_t spot_light_count = 0;

    _setup_lights(&render_data, directional_light_count, omni_light_count, spot_light_count);

    // Render shadows
    _render_shadows(&render_data, directional_light_count);

    RENDER_TIMESTAMP("Render Scene");

    GLES2::TextureStorage *texture_storage = GLES2::TextureStorage::get_singleton();
    GLES2::Config *config = GLES2::Config::get_singleton();

    Ref<RenderSceneBuffersGLES2> rb = p_render_buffers;
    ERR_FAIL_COND(rb.is_null());

    GLES2::RenderTarget *rt = texture_storage->get_render_target(rb->render_target);
    ERR_FAIL_NULL(rt);

    // Bind framebuffer
    glBindFramebuffer(GL_FRAMEBUFFER, rt->fbo);
    glViewport(0, 0, rb->internal_size.x, rb->internal_size.y);

    // Clear buffers
    GLbitfield clear_mask = 0;
    if (render_data.transparent_bg) {
        clear_mask |= GL_COLOR_BUFFER_BIT;
    }

    clear_mask |= GL_DEPTH_BUFFER_BIT;
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glClearDepth(1.0f);

    Color clear_color = texture_storage->get_default_clear_color();
    glClearColor(clear_color.r, clear_color.g, clear_color.b, clear_color.a);

    glClear(clear_mask);

    // Fill render lists
    _fill_render_list(RENDER_LIST_OPAQUE, &render_data, PASS_MODE_COLOR);
    render_list[RENDER_LIST_OPAQUE].sort_by_key();
    render_list[RENDER_LIST_ALPHA].sort_by_reverse_depth_and_priority();

    // Setup state
    glFrontFace(GL_CCW);
    glCullFace(GL_BACK);
    glEnable(GL_CULL_FACE);
    glDisable(GL_BLEND);

    // Render opaque objects
    RenderListParameters render_list_params(render_list[RENDER_LIST_OPAQUE].elements.ptr(), render_list[RENDER_LIST_OPAQUE].elements.size(), false, 0, false);
    _render_list_template<PASS_MODE_COLOR>(&render_list_params, &render_data, 0, render_list[RENDER_LIST_OPAQUE].elements.size());

    // Render sky
    if (render_data.environment.is_valid() && environment_get_background(render_data.environment) == RS::ENV_BG_SKY) {
        RENDER_TIMESTAMP("Render Sky");
        _draw_sky(&render_data, render_data.cam_projection, render_data.cam_transform);
    }

    // Render transparent objects
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    RenderListParameters render_list_params_alpha(render_list[RENDER_LIST_ALPHA].elements.ptr(), render_list[RENDER_LIST_ALPHA].elements.size(), false, 0, false);
    _render_list_template<PASS_MODE_COLOR_TRANSPARENT>(&render_list_params_alpha, &render_data, 0, render_list[RENDER_LIST_ALPHA].elements.size());

    // Render debug
    if (get_debug_draw_mode() != RS::VIEWPORT_DEBUG_DRAW_DISABLED) {
        _render_debug(&render_data);
    }

    // Resolve MSAA if needed
    if (rt->msaa != RS::VIEWPORT_MSAA_DISABLED) {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, rt->fbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, rt->fbo_blit);
        glBlitFramebuffer(0, 0, rb->internal_size.x, rb->internal_size.y, 0, 0, rb->internal_size.x, rb->internal_size.y, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    }

    // Apply tonemapping
    if (render_data.environment.is_valid()) {
        _apply_tonemap(&render_data);
    }

    // Cleanup
    glBindFramebuffer(GL_FRAMEBUFFER, GLES2::TextureStorage::system_fbo);
    glViewport(0, 0, rb->width, rb->height);
}

template <PassMode p_pass_mode>
void RasterizerSceneGLES2::_render_list_template(RenderListParameters *p_params, const RenderDataGLES2 *p_render_data, uint32_t p_from_element, uint32_t p_to_element) {
    GLES2::MaterialStorage *material_storage = GLES2::MaterialStorage::get_singleton();

    GLuint current_vertex_array = 0;
    GLuint current_index_array = 0;
    GLES2::SceneMaterialData *current_material = nullptr;
    GLES2::SceneShaderData *current_shader = nullptr;
    uint64_t current_blend_mode = 0;

    for (uint32_t i = p_from_element; i < p_to_element; i++) {
        GeometryInstanceSurface *surf = p_params->elements[i];

        if (!surf->material->shader_data->valid) {
            continue;
        }

        // Setup material
        if (current_material != surf->material) {
            current_material = surf->material;
            current_material->bind_uniforms();
        }

        // Setup shader
        if (current_shader != surf->shader) {
            current_shader = surf->shader;
            material_storage->shaders.scene_shader.version_set_uniform(SceneShaderGLES2::MATERIAL_UNIFORMS_ARRAY_SIZE, current_shader->uniform_count, current_shader->version, current_shader->uniforms);
        }

        // Setup blend mode
        if (current_blend_mode != surf->material->blend_mode) {
            current_blend_mode = surf->material->blend_mode;

            switch (current_blend_mode) {
                case GLES2::SceneShaderData::BLEND_MODE_MIX: {
                    glBlendEquation(GL_FUNC_ADD);
                    if (p_render_data->transparent_bg) {
						glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
                    } else {
                        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                    }
                } break;
                case GLES2::SceneShaderData::BLEND_MODE_ADD: {
                    glBlendEquation(GL_FUNC_ADD);
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
                } break;
                case GLES2::SceneShaderData::BLEND_MODE_SUB: {
                    glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
                } break;
                case GLES2::SceneShaderData::BLEND_MODE_MUL: {
                    glBlendEquation(GL_FUNC_ADD);
                    if (p_render_data->transparent_bg) {
                        glBlendFuncSeparate(GL_DST_COLOR, GL_ZERO, GL_DST_ALPHA, GL_ZERO);
                    } else {
                        glBlendFuncSeparate(GL_DST_COLOR, GL_ZERO, GL_ZERO, GL_ONE);
                    }
                } break;
            }
        }

        // Setup cull mode
        GLES2::SceneShaderData::Cull cull_mode = surf->shader->cull_mode;
        if (p_render_data->cam_transform.basis.determinant() < 0) {
            if (cull_mode == GLES2::SceneShaderData::CULL_FRONT) {
                cull_mode = GLES2::SceneShaderData::CULL_BACK;
            } else if (cull_mode == GLES2::SceneShaderData::CULL_BACK) {
                cull_mode = GLES2::SceneShaderData::CULL_FRONT;
            }
        }

        if (cull_mode == GLES2::SceneShaderData::CULL_DISABLED) {
            glDisable(GL_CULL_FACE);
        } else {
            glEnable(GL_CULL_FACE);
            glCullFace(cull_mode == GLES2::SceneShaderData::CULL_FRONT ? GL_FRONT : GL_BACK);
        }

        // Setup vertex array and index buffer
        if (current_vertex_array != surf->vertex_array) {
            if (surf->vertex_array == 0) {
                // Surface is built-in primitive
                glBindVertexArray(0);
                glDisableVertexAttribArray(RS::ARRAY_VERTEX);
                glVertexAttrib4f(RS::ARRAY_VERTEX, 0, 0, 0, 1);
            } else {
                glBindVertexArray(surf->vertex_array);
            }
            current_vertex_array = surf->vertex_array;
        }

        if (current_index_array != surf->index_array) {
            if (surf->index_array == 0) {
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
            } else {
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, surf->index_array);
            }
            current_index_array = surf->index_array;
        }

        // Setup uniforms
        _setup_uniforms(surf, p_render_data);

        // Draw
        if (surf->index_array) {
            glDrawElements(surf->primitive, surf->index_count, GL_UNSIGNED_INT, 0);
        } else if (surf->vertex_array) {
            glDrawArrays(surf->primitive, 0, surf->vertex_count);
        } else {
            // Built-in primitive
            switch (surf->primitive) {
                case GL_POINTS: {
                    glDrawArrays(GL_POINTS, 0, 1);
                } break;
                case GL_LINES: {
                    glDrawArrays(GL_LINES, 0, 2);
                } break;
                case GL_TRIANGLES: {
                    glDrawArrays(GL_TRIANGLES, 0, 3);
                } break;
                default: {
                    ERR_PRINT("Invalid primitive type.");
                }
            }
        }

        if (p_render_data->render_info) {
            p_render_data->render_info->info[RS::VIEWPORT_RENDER_INFO_TYPE_VISIBLE][RS::VIEWPORT_RENDER_INFO_DRAW_CALLS_IN_FRAME]++;
            p_render_data->render_info->info[RS::VIEWPORT_RENDER_INFO_TYPE_VISIBLE][RS::VIEWPORT_RENDER_INFO_PRIMITIVES_IN_FRAME] += surf->index_count ? surf->index_count : surf->vertex_count;
        }
    }

    // Reset state
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void RasterizerSceneGLES2::_setup_uniforms(GeometryInstanceSurface *p_surf, const RenderDataGLES2 *p_render_data) {
    GLES2::MaterialStorage *material_storage = GLES2::MaterialStorage::get_singleton();
    GLES2::Config *config = GLES2::Config::get_singleton();

    // Set camera uniforms
    material_storage->shaders.scene_shader.version_set_uniform(SceneShaderGLES2::CAMERA_MATRIX, p_render_data->cam_transform, p_surf->shader->version, p_surf->shader->uniforms);
    material_storage->shaders.scene_shader.version_set_uniform(SceneShaderGLES2::PROJECTION_MATRIX, p_render_data->cam_projection, p_surf->shader->version, p_surf->shader->uniforms);

    // Set model uniforms
    material_storage->shaders.scene_shader.version_set_uniform(SceneShaderGLES2::MODEL_MATRIX, p_surf->owner->data->transform, p_surf->shader->version, p_surf->shader->uniforms);

    // Set light uniforms
    if (p_surf->shader->uses_lighting) {
        uint32_t light_count = MIN(p_render_data->directional_light_count + p_render_data->omni_light_count + p_render_data->spot_light_count, config->max_lights_per_object);
        material_storage->shaders.scene_shader.version_set_uniform(SceneShaderGLES2::LIGHT_COUNT, light_count, p_surf->shader->version, p_surf->shader->uniforms);

        if (light_count > 0) {
            // Set directional lights
            for (uint32_t i = 0; i < p_render_data->directional_light_count && i < config->max_lights_per_object; i++) {
                String light_uniform = "directional_lights[" + itos(i) + "]";
                material_storage->shaders.scene_shader.version_set_uniform(SceneShaderGLES2::LIGHT_DIRECTION, scene_state.directional_lights[i].direction, p_surf->shader->version, p_surf->shader->uniforms, light_uniform);
                material_storage->shaders.scene_shader.version_set_uniform(SceneShaderGLES2::LIGHT_COLOR, scene_state.directional_lights[i].color, p_surf->shader->version, p_surf->shader->uniforms, light_uniform);
                material_storage->shaders.scene_shader.version_set_uniform(SceneShaderGLES2::LIGHT_ENERGY, scene_state.directional_lights[i].energy, p_surf->shader->version, p_surf->shader->uniforms, light_uniform);
            }

            // Set omni lights
            for (uint32_t i = 0; i < p_render_data->omni_light_count && i < config->max_lights_per_object; i++) {
                String light_uniform = "omni_lights[" + itos(i) + "]";
                material_storage->shaders.scene_shader.version_set_uniform(SceneShaderGLES2::LIGHT_POSITION, scene_state.omni_lights[i].position, p_surf->shader->version, p_surf->shader->uniforms, light_uniform);
                material_storage->shaders.scene_shader.version_set_uniform(SceneShaderGLES2::LIGHT_COLOR, scene_state.omni_lights[i].color, p_surf->shader->version, p_surf->shader->uniforms, light_uniform);
                material_storage->shaders.scene_shader.version_set_uniform(SceneShaderGLES2::LIGHT_ENERGY, scene_state.omni_lights[i].energy, p_surf->shader->version, p_surf->shader->uniforms, light_uniform);
                material_storage->shaders.scene_shader.version_set_uniform(SceneShaderGLES2::LIGHT_RANGE, 1.0f / scene_state.omni_lights[i].inv_radius, p_surf->shader->version, p_surf->shader->uniforms, light_uniform);
            }

            // Set spot lights
            for (uint32_t i = 0; i < p_render_data->spot_light_count && i < config->max_lights_per_object; i++) {
                String light_uniform = "spot_lights[" + itos(i) + "]";
                material_storage->shaders.scene_shader.version_set_uniform(SceneShaderGLES2::LIGHT_POSITION, scene_state.spot_lights[i].position, p_surf->shader->version, p_surf->shader->uniforms, light_uniform);
                material_storage->shaders.scene_shader.version_set_uniform(SceneShaderGLES2::LIGHT_DIRECTION, scene_state.spot_lights[i].direction, p_surf->shader->version, p_surf->shader->uniforms, light_uniform);
                material_storage->shaders.scene_shader.version_set_uniform(SceneShaderGLES2::LIGHT_COLOR, scene_state.spot_lights[i].color, p_surf->shader->version, p_surf->shader->uniforms, light_uniform);
                material_storage->shaders.scene_shader.version_set_uniform(SceneShaderGLES2::LIGHT_ENERGY, scene_state.spot_lights[i].energy, p_surf->shader->version, p_surf->shader->uniforms, light_uniform);
                material_storage->shaders.scene_shader.version_set_uniform(SceneShaderGLES2::LIGHT_RANGE, 1.0f / scene_state.spot_lights[i].inv_radius, p_surf->shader->version, p_surf->shader->uniforms, light_uniform);
                material_storage->shaders.scene_shader.version_set_uniform(SceneShaderGLES2::LIGHT_SPOT_ANGLE, scene_state.spot_lights[i].spot_angle, p_surf->shader->version, p_surf->shader->uniforms, light_uniform);
                material_storage->shaders.scene_shader.version_set_uniform(SceneShaderGLES2::LIGHT_SPOT_ATTENUATION, scene_state.spot_lights[i].spot_attenuation, p_surf->shader->version, p_surf->shader->uniforms, light_uniform);
            }
        }
    }

    // Set custom uniforms
    p_surf->material->set_custom_uniforms(p_surf->shader);
}

void RasterizerSceneGLES2::_post_process(const RenderDataGLES2 *p_render_data) {
    GLES2::TextureStorage *texture_storage = GLES2::TextureStorage::get_singleton();
    GLES2::Config *config = GLES2::Config::get_singleton();

    Ref<RenderSceneBuffersGLES2> rb = p_render_data->render_buffers;
    ERR_FAIL_COND(rb.is_null());

    GLES2::RenderTarget *rt = texture_storage->get_render_target(rb->render_target);
    ERR_FAIL_NULL(rt);

    // Apply tone mapping
    if (p_render_data->environment.is_valid()) {
        RS::EnvironmentToneMapper tone_mapper = environment_get_tone_mapper(p_render_data->environment);
        float exposure = environment_get_exposure(p_render_data->environment);
        float white = environment_get_white(p_render_data->environment);

        glBindFramebuffer(GL_FRAMEBUFFER, rt->fbo);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, rt->color);

        material_storage->shaders.tonemap.set_conditional(TonemapShaderGLES2::USE_AUTO_EXPOSURE, false);
        material_storage->shaders.tonemap.set_conditional(TonemapShaderGLES2::USE_FILMIC_TONEMAPPING, tone_mapper == RS::ENV_TONE_MAPPER_FILMIC);
        material_storage->shaders.tonemap.set_conditional(TonemapShaderGLES2::USE_ACES_TONEMAPPING, tone_mapper == RS::ENV_TONE_MAPPER_ACES);

        material_storage->shaders.tonemap.bind();

        material_storage->shaders.tonemap.set_uniform(TonemapShaderGLES2::EXPOSURE, exposure);
        material_storage->shaders.tonemap.set_uniform(TonemapShaderGLES2::WHITE, white);

        _draw_gui_primitive(4, nullptr, nullptr);

        glBindTexture(GL_TEXTURE_2D, 0);
    }

    // Apply FXAA if enabled
    if (p_render_data->environment.is_valid() && environment_get_fxaa(p_render_data->environment)) {
        glBindFramebuffer(GL_FRAMEBUFFER, rt->fbo_blit);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, rt->color);

        material_storage->shaders.fxaa.bind();

        material_storage->shaders.fxaa.set_uniform(FXAAShaderGLES2::INVERSE_VIEWPORT_SIZE, Vector2(1.0f / rt->width, 1.0f / rt->height));

        _draw_gui_primitive(4, nullptr, nullptr);

        glBindTexture(GL_TEXTURE_2D, 0);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, rt->fbo_blit);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, rt->fbo);
        glBlitFramebuffer(0, 0, rt->width, rt->height, 0, 0, rt->width, rt->height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    }
}

void RasterizerSceneGLES2::_render_debug(const RenderDataGLES2 *p_render_data) {
    GLES2::TextureStorage *texture_storage = GLES2::TextureStorage::get_singleton();

    Ref<RenderSceneBuffersGLES2> rb = p_render_data->render_buffers;
    ERR_FAIL_COND(rb.is_null());

    GLES2::RenderTarget *rt = texture_storage->get_render_target(rb->render_target);
    ERR_FAIL_NULL(rt);

    glBindFramebuffer(GL_FRAMEBUFFER, rt->fbo);
    glViewport(0, 0, rb->width, rb->height);

    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    switch (get_debug_draw_mode()) {
        case RS::VIEWPORT_DEBUG_DRAW_WIREFRAME: {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            _render_list_template<PASS_MODE_COLOR>(&render_list[RENDER_LIST_OPAQUE], p_render_data, 0, render_list[RENDER_LIST_OPAQUE].elements.size());
            _render_list_template<PASS_MODE_COLOR>(&render_list[RENDER_LIST_ALPHA], p_render_data, 0, render_list[RENDER_LIST_ALPHA].elements.size());
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        } break;
        case RS::VIEWPORT_DEBUG_DRAW_OVERDRAW: {
            glEnable(GL_BLEND);
            glBlendFunc(GL_ONE, GL_ONE);
            glDepthMask(GL_FALSE);
            glDisable(GL_DEPTH_TEST);

            _render_list_template<PASS_MODE_COLOR>(&render_list[RENDER_LIST_OPAQUE], p_render_data, 0, render_list[RENDER_LIST_OPAQUE].elements.size());
            _render_list_template<PASS_MODE_COLOR>(&render_list[RENDER_LIST_ALPHA], p_render_data, 0, render_list[RENDER_LIST_ALPHA].elements.size());

            glDisable(GL_BLEND);
            glDepthMask(GL_TRUE);
            glEnable(GL_DEPTH_TEST);
        } break;
        case RS::VIEWPORT_DEBUG_DRAW_UNSHADED: {
            material_storage->shaders.unshaded.bind();
            _render_list_template<PASS_MODE_COLOR>(&render_list[RENDER_LIST_OPAQUE], p_render_data, 0, render_list[RENDER_LIST_OPAQUE].elements.size());
            _render_list_template<PASS_MODE_COLOR>(&render_list[RENDER_LIST_ALPHA], p_render_data, 0, render_list[RENDER_LIST_ALPHA].elements.size());
        } break;
        default: break;
    }
}

void RasterizerSceneGLES2::_draw_sky(const RenderDataGLES2 *p_render_data, const Projection &p_projection, const Transform3D &p_transform) {
    GLES2::TextureStorage *texture_storage = GLES2::TextureStorage::get_singleton();
    GLES2::MaterialStorage *material_storage = GLES2::MaterialStorage::get_singleton();

    ERR_FAIL_COND(p_render_data->environment.is_null());

    Sky *sky = sky_owner.get_or_null(environment_get_sky(p_render_data->environment));
    ERR_FAIL_NULL(sky);

    if (sky->radiance == 0) {
        return; // No radiance texture available, don't draw sky.
    }

    material_storage->shaders.sky.set_conditional(SkyShaderGLES2::USE_CUBEMAP_ARRAY, false);
    material_storage->shaders.sky.set_conditional(SkyShaderGLES2::USE_HALF_RES_PASS, false);
    material_storage->shaders.sky.set_conditional(SkyShaderGLES2::USE_QUARTER_RES_PASS, false);
    material_storage->shaders.sky.bind();

    material_storage->shaders.sky.set_uniform(SkyShaderGLES2::RADIANCE, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, sky->radiance);

    material_storage->shaders.sky.set_uniform(SkyShaderGLES2::PROJECTION, p_projection);
    material_storage->shaders.sky.set_uniform(SkyShaderGLES2::INVERSE_VIEW, p_transform.affine_inverse());

    // Draw sky
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);

    _draw_gui_primitive(4, nullptr, nullptr);

    // Reset state
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
}

void RasterizerSceneGLES2::_draw_gui_primitive(int p_points, const Vector2 *p_vertices, const Vector2 *p_uvs, const Color *p_colors, const int *p_indices, int p_count) {
    static const Vector2 quad_array[4] = {
        Vector2(-1, -1),
        Vector2(-1, 1),
        Vector2(1, 1),
        Vector2(1, -1),
    };
    static const Vector2 quad_array_uv[4] = {
        Vector2(0, 0),
        Vector2(0, 1),
        Vector2(1, 1),
        Vector2(1, 0),
    };

    if (!p_vertices) {
        p_vertices = quad_array;
    }
    if (!p_uvs) {
        p_uvs = quad_array_uv;
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    glEnableVertexAttribArray(RS::ARRAY_VERTEX);
    glVertexAttribPointer(RS::ARRAY_VERTEX, 2, GL_FLOAT, GL_FALSE, sizeof(Vector2), p_vertices);

    if (p_colors) {
        glEnableVertexAttribArray(RS::ARRAY_COLOR);
        glVertexAttribPointer(RS::ARRAY_COLOR, 4, GL_FLOAT, GL_FALSE, sizeof(Color), p_colors);
    } else {
        glDisableVertexAttribArray(RS::ARRAY_COLOR);
    }

    if (p_uvs) {
        glEnableVertexAttribArray(RS::ARRAY_TEX_UV);
        glVertexAttribPointer(RS::ARRAY_TEX_UV, 2, GL_FLOAT, GL_FALSE, sizeof(Vector2), p_uvs);
    } else {
        glDisableVertexAttribArray(RS::ARRAY_TEX_UV);
    }

    if (p_indices) {
        glDrawElements(GL_TRIANGLES, p_count, GL_UNSIGNED_INT, p_indices);
    } else {
        glDrawArrays(GL_TRIANGLE_FAN, 0, p_points);
    }

    glDisableVertexAttribArray(RS::ARRAY_VERTEX);
    glDisableVertexAttribArray(RS::ARRAY_COLOR);
    glDisableVertexAttribArray(RS::ARRAY_TEX_UV);
}

void RasterizerSceneGLES2::_render_shadows(const RenderDataGLES2 *p_render_data, uint32_t p_directional_light_count) {
    GLES2::LightStorage *light_storage = GLES2::LightStorage::get_singleton();
    GLES2::Config *config = GLES2::Config::get_singleton();

    for (uint32_t i = 0; i < p_render_data->render_shadow_count; i++) {
        RID light_instance = p_render_data->render_shadows[i].light_instance;
        RID light = light_storage->light_instance_get_base_light(light_instance);

        RS::LightType type = light_storage->light_get_type(light);

        switch (type) {
            case RS::LIGHT_DIRECTIONAL: {
                _render_shadow_directional(light_instance, p_render_data, i, p_directional_light_count);
            } break;
            case RS::LIGHT_OMNI: {
                _render_shadow_omni(light_instance, p_render_data, i);
            } break;
            case RS::LIGHT_SPOT: {
                _render_shadow_spot(light_instance, p_render_data, i);
            } break;
            default:
                break;
        }
    }
}

void RasterizerSceneGLES2::_render_shadow_directional(RID p_light_instance, const RenderDataGLES2 *p_render_data, int p_shadow_index, uint32_t p_directional_light_count) {
    GLES2::LightStorage *light_storage = GLES2::LightStorage::get_singleton();

    ERR_FAIL_COND(p_shadow_index >= GLES2::Config::get_singleton()->max_shadows);

    GLES2::LightInstance *light_instance = light_storage->get_light_instance(p_light_instance);
    ERR_FAIL_NULL(light_instance);

    RID light = light_instance->light;

    GLES2::DirectionalLightData &light_data = scene_state.directional_lights[p_shadow_index];

    Transform3D light_transform = light_instance->transform;
    Vector3 light_direction = -light_transform.basis.get_axis(2);

    float shadow_max_distance = light_storage->light_get_param(light, RS::LIGHT_PARAM_SHADOW_MAX_DISTANCE);
    float shadow_split_offsets[4];
    light_storage->light_directional_get_shadow_split_offsets(light, shadow_split_offsets);

    Projection camera_projection = p_render_data->cam_projection;

    // Calculate split depths
    float split_distances[5];
    split_distances[0] = p_render_data->z_near;
    split_distances[4] = shadow_max_distance;

    for (int i = 1; i < 4; i++) {
        float linear_depth = split_distances[0] + (shadow_max_distance - split_distances[0]) * shadow_split_offsets[i - 1];
        float log_depth = split_distances[0] * Math::pow(shadow_max_distance / split_distances[0], shadow_split_offsets[i - 1]);
        float depth = Math::lerp(linear_depth, log_depth, 0.7f);
        split_distances[i] = depth;
    }

    for (int i = 0; i < 4; i++) {
        RENDER_TIMESTAMP("Render Directional Light Shadow " + itos(i));

        Rect2i atlas_rect = light_storage->directional_shadow_get_atlas_rect(p_directional_light_count, i);
        float bias_scale = light_storage->light_get_param(light, RS::LIGHT_PARAM_SHADOW_BIAS_SCALE);
        float range_begin = split_distances[i];
        float range_end = split_distances[i + 1];

        Projection projection = _calculate_directional_shadow_projection(camera_projection, p_render_data->cam_transform, range_begin, range_end, light_direction, light_transform.origin);

        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glDepthMask(GL_TRUE);
        glEnable(GL_SCISSOR_TEST);
        glScissor(atlas_rect.position.x, atlas_rect.position.y, atlas_rect.size.width, atlas_rect.size.height);
        glViewport(atlas_rect.position.x, atlas_rect.position.y, atlas_rect.size.width, atlas_rect.size.height);

        glClearDepth(1.0f);
        glClear(GL_DEPTH_BUFFER_BIT);

        RenderListParameters render_list_params(render_list[RENDER_LIST_SECONDARY].elements.ptr(), render_list[RENDER_LIST_SECONDARY].elements.size(), true, 0, true);
        _render_list_template<PASS_MODE_SHADOW>(&render_list_params, p_render_data, 0, render_list[RENDER_LIST_SECONDARY].elements.size());

        // Store shadow information
        light_data.shadow_split_offsets[i] = range_end;
        light_data.shadow_normal_bias[i] = light_storage->light_get_param(light, RS::LIGHT_PARAM_SHADOW_NORMAL_BIAS) * bias_scale;
        GLES2::MaterialStorage::store_camera(projection, light_data.shadow_matrices[i]);
    }

    glDisable(GL_SCISSOR_TEST);
}

void RasterizerSceneGLES2::_render_shadow_omni(RID p_light_instance, const RenderDataGLES2 *p_render_data, int p_shadow_index) {
    GLES2::LightStorage *light_storage = GLES2::LightStorage::get_singleton();

    ERR_FAIL_COND(p_shadow_index >= GLES2::Config::get_singleton()->max_shadows);

    GLES2::LightInstance *light_instance = light_storage->get_light_instance(p_light_instance);
    ERR_FAIL_NULL(light_instance);

    RID light = light_instance->light;

    GLES2::LightData &light_data = scene_state.omni_lights[p_shadow_index];

    Transform3D light_transform = light_instance->transform;
    float radius = light_storage->light_get_param(light, RS::LIGHT_PARAM_RANGE);

    Rect2i atlas_rect = light_storage->light_instance_get_shadow_atlas_rect(p_light_instance);

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glEnable(GL_SCISSOR_TEST);
    glScissor(atlas_rect.position.x, atlas_rect.position.y, atlas_rect.size.width, atlas_rect.size.height);
    glViewport(atlas_rect.position.x, atlas_rect.position.y, atlas_rect.size.width, atlas_rect.size.height);

    glClearDepth(1.0f);
    glClear(GL_DEPTH_BUFFER_BIT);

    for (int i = 0; i < 6; i++) {
        Projection proj;
        proj.set_perspective(90, 1, 0.01, radius);

        Vector3 cam_target = light_transform.origin;
        Vector3 cam_up = Vector3(0, 1, 0);

        switch (i) {
            case 0:
                cam_target += Vector3(1, 0, 0);
                cam_up = Vector3(0, -1, 0);
                break; // positive X
            case 1:
                cam_target += Vector3(-1, 0, 0);
                cam_up = Vector3(0, -1, 0);
                break; // negative X
            case 2:
                cam_target += Vector3(0, 1, 0);
                cam_up = Vector3(0, 0, 1);
                break; // positive Y
            case 3:
                cam_target += Vector3(0, -1, 0);
                cam_up = Vector3(0, 0, -1);
                break; // negative Y
            case 4:
                cam_target += Vector3(0, 0, 1);
                break; // positive Z
            case 5:
                cam_target += Vector3(0, 0, -1);
                break; // negative Z
        }

        Transform3D cam_transform;
        cam_transform.set_look_at(light_transform.origin, cam_target, cam_up);

        RenderDataGLES2 render_data = *p_render_data;
        render_data.cam_transform = cam_transform;
        render_data.cam_projection = proj;

        RenderListParameters render_list_params(render_list[RENDER_LIST_SECONDARY].elements.ptr(), render_list[RENDER_LIST_SECONDARY].elements.size(), true, 0, true);
        _render_list_template<PASS_MODE_SHADOW>(&render_list_params, &render_data, 0, render_list[RENDER_LIST_SECONDARY].elements.size());
    }

    glDisable(GL_SCISSOR_TEST);
}

void RasterizerSceneGLES2::_render_shadow_spot(RID p_light_instance, const RenderDataGLES2 *p_render_data, int p_shadow_index) {
    GLES2::LightStorage *light_storage = GLES2::LightStorage::get_singleton();

    ERR_FAIL_COND(p_shadow_index >= GLES2::Config::get_singleton()->max_shadows);

    GLES2::LightInstance *light_instance = light_storage->get_light_instance(p_light_instance);
    ERR_FAIL_NULL(light_instance);

    RID light = light_instance->light;

    GLES2::LightData &light_data = scene_state.spot_lights[p_shadow_index];

    Transform3D light_transform = light_instance->transform;
    float radius = light_storage->light_get_param(light, RS::LIGHT_PARAM_RANGE);
    float angle = light_storage->light_get_param(light, RS::LIGHT_PARAM_SPOT_ANGLE);

    Rect2i atlas_rect = light_storage->light_instance_get_shadow_atlas_rect(p_light_instance);

    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glEnable(GL_SCISSOR_TEST);
    glScissor(atlas_rect.position.x, atlas_rect.position.y, atlas_rect.size.width, atlas_rect.size.height);
    glViewport(atlas_rect.position.x, atlas_rect.position.y, atlas_rect.size.width, atlas_rect.size.height);

    glClearDepth(1.0f);
    glClear(GL_DEPTH_BUFFER_BIT);

    Projection proj;
    proj.set_perspective(angle * 2.0, 1, 0.01, radius);

    RenderDataGLES2 render_data = *p_render_data;
    render_data.cam_transform = light_transform;
    render_data.cam_projection = proj;

    RenderListParameters render_list_params(render_list[RENDER_LIST_SECONDARY].elements.ptr(), render_list[RENDER_LIST_SECONDARY].elements.size(), true, 0, true);
    _render_list_template<PASS_MODE_SHADOW>(&render_list_params, &render_data, 0, render_list[RENDER_LIST_SECONDARY].elements.size());

    glDisable(GL_SCISSOR_TEST);
}

Projection RasterizerSceneGLES2::_calculate_directional_shadow_projection(const Projection &p_camera_projection, const Transform3D &p_camera_transform, float p_split_near, float p_split_far, const Vector3 &p_light_direction, const Vector3 &p_light_position) {
    Vector3 camera_forward = -p_camera_transform.basis.get_axis(2);
    Vector3 camera_up = p_camera_transform.basis.get_axis(1);
    Vector3 camera_right = p_camera_transform.basis.get_axis(0);

    Vector3 light_direction = p_light_direction.normalized();

    float camera_size = p_split_far - p_split_near;
    float camera_depth = (p_split_far + p_split_near) * 0.5;

    Vector3 center = p_camera_transform.origin + camera_forward * camera_depth;

    Vector3 min_bounds = Vector3(FLT_MAX, FLT_MAX, FLT_MAX);
    Vector3 max_bounds = Vector3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

    for (int i = 0; i < 8; i++) {
        Vector3 point = center;
        point += camera_up * (((i & 1) ? 0.5 : -0.5) * camera_size);
        point += camera_right * (((i & 2) ? 0.5 : -0.5) * camera_size);
        point += camera_forward * (((i & 4) ? 0.5 : -0.5) * camera_size);

        min_bounds = min_bounds.min(point);
        max_bounds = max_bounds.max(point);
    }

    Vector3 box_center = (min_bounds + max_bounds) * 0.5;
    Vector3 box_size = max_bounds - min_bounds;

    Transform3D light_transform;
    light_transform.set_look_at(box_center - light_direction * box_size.length() * 0.5, box_center, camera_up);

    Projection light_projection;
    light_projection.set_orthogonal(-box_size.x * 0.5, box_size.x * 0.5, -box_size.y * 0.5, box_size.y * 0.5, 0, box_size.z);

    return light_projection * light_transform.inverse();
}

void RasterizerSceneGLES2::environment_set_sky(RID p_env, RID p_sky) {
    Environment *env = environment_owner.get_or_null(p_env);
    ERR_FAIL_NULL(env);
    env->sky = p_sky;
}

void RasterizerSceneGLES2::environment_set_sky_custom_fov(RID p_env, float p_scale) {
    Environment *env = environment_owner.get_or_null(p_env);
    ERR_FAIL_NULL(env);
    env->sky_custom_fov = p_scale;
}

void RasterizerSceneGLES2::environment_set_sky_orientation(RID p_env, const Basis &p_orientation) {
    Environment *env = environment_owner.get_or_null(p_env);
    ERR_FAIL_NULL(env);
    env->sky_orientation = p_orientation;
}

void RasterizerSceneGLES2::environment_set_bg_color(RID p_env, const Color &p_color) {
    Environment *env = environment_owner.get_or_null(p_env);
    ERR_FAIL_NULL(env);
    env->bg_color = p_color;
}

void RasterizerSceneGLES2::environment_set_bg_energy(RID p_env, float p_energy) {
    Environment *env = environment_owner.get_or_null(p_env);
    ERR_FAIL_NULL(env);
    env->bg_energy = p_energy;
}

void RasterizerSceneGLES2::environment_set_canvas_max_layer(RID p_env, int p_max_layer) {
    Environment *env = environment_owner.get_or_null(p_env);
    ERR_FAIL_NULL(env);
    env->canvas_max_layer = p_max_layer;
}

void RasterizerSceneGLES2::environment_set_ambient_light(RID p_env, const Color &p_color, float p_energy, float p_sky_contribution) {
    Environment *env = environment_owner.get_or_null(p_env);
    ERR_FAIL_NULL(env);
    env->ambient_color = p_color;
    env->ambient_energy = p_energy;
    env->ambient_sky_contribution = p_sky_contribution;
}

void RasterizerSceneGLES2::_update_sky(const RenderDataGLES2 *p_render_data) {
    GLES2::MaterialStorage *material_storage = GLES2::MaterialStorage::get_singleton();

    ERR_FAIL_COND(p_render_data->environment.is_null());

    Sky *sky = sky_owner.get_or_null(environment_get_sky(p_render_data->environment));
    ERR_FAIL_NULL(sky);

    RID sky_material = sky->material;

    if (!sky_material.is_valid()) {
        sky_material = sky_globals.default_material;
    }

    GLES2::SkyMaterialData *material_data = static_cast<GLES2::SkyMaterialData *>(material_storage->material_get_data(sky_material, RS::SHADER_SKY));
    ERR_FAIL_NULL(material_data);

    GLES2::SkyShaderData *shader_data = material_data->shader_data;
    ERR_FAIL_NULL(shader_data);

    bool update_single_frame = sky->mode == RS::SKY_MODE_REALTIME || sky->mode == RS::SKY_MODE_QUALITY;

    if (sky->panorama.is_valid()) {
        material_storage->shaders.sky_shader.version_set_uniform(SkyShaderGLES2::TEXTURE_PANORAMA, sky->panorama, shader_data->version, shader_data->uniforms);
    }

    Basis sky_transform = environment_get_sky_orientation(p_render_data->environment);
    sky_transform.invert();
    material_storage->shaders.sky_shader.version_set_uniform(SkyShaderGLES2::ORIENTATION, sky_transform, shader_data->version, shader_data->uniforms);

    material_storage->shaders.sky_shader.version_set_uniform(SkyShaderGLES2::TIME, time, shader_data->version, shader_data->uniforms);

    if (update_single_frame || !sky->radiance) {
        _update_sky_radiance(sky, p_render_data->environment, sky_transform, time);
    }
}

void RasterizerSceneGLES2::_update_sky_radiance(Sky *p_sky, RID p_env, const Basis &p_sky_orientation, float p_time) {
    GLES2::TextureStorage *texture_storage = GLES2::TextureStorage::get_singleton();
    GLES2::Config *config = GLES2::Config::get_singleton();

    if (!p_sky->radiance) {
        p_sky->radiance = texture_storage->texture_create();
        texture_storage->texture_allocate(p_sky->radiance, config->sky_texture_size, config->sky_texture_size, 0, Image::FORMAT_RGBAH, TextureType::TEXTURE_TYPE_CUBE_ARRAY);
    }

    GLES2::Texture *tex = texture_storage->get_texture(p_sky->radiance);
    ERR_FAIL_NULL(tex);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(tex->target, tex->tex_id);

    glTexParameteri(tex->target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(tex->target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(tex->target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(tex->target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    Projection cm;
    cm.set_perspective(90, 1, 0.01, 10.0);
    Projection correction;
    correction.set_depth_correction(true);
    cm = correction * cm;

    Vector3 position(0, 0, 0);

    // Render sky to cubemap faces
    for (int i = 0; i < 6; i++) {
        Basis local_view = Basis::looking_at(CubemapFace::directions[i], CubemapFace::ups[i]);
        local_view = p_sky_orientation * local_view;

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, tex->tex_id, 0);
        glViewport(0, 0, config->sky_texture_size, config->sky_texture_size);

        _render_sky(p_env, cm, local_view.inverse(), position, 1.0, p_time);
    }

    glBindTexture(tex->target, tex->tex_id);
    glGenerateMipmap(tex->target);

    // Update radiance uniform
    GLES2::MaterialStorage *material_storage = GLES2::MaterialStorage::get_singleton();
    RID sky_material = p_sky->material;

    if (!sky_material.is_valid()) {
        sky_material = sky_globals.default_material;
    }

    GLES2::SkyMaterialData *material_data = static_cast<GLES2::SkyMaterialData *>(material_storage->material_get_data(sky_material, RS::SHADER_SKY));
    ERR_FAIL_NULL(material_data);

    GLES2::SkyShaderData *shader_data = material_data->shader_data;
    ERR_FAIL_NULL(shader_data);

    material_storage->shaders.sky_shader.version_set_uniform(SkyShaderGLES2::RADIANCE, 0, shader_data->version, shader_data->uniforms);
}

void RasterizerSceneGLES2::_render_sky(RID p_env, const Projection &p_projection, const Transform3D &p_transform, const Vector3 &p_position, float p_luminance_multiplier, float p_time) {
    GLES2::MaterialStorage *material_storage = GLES2::MaterialStorage::get_singleton();

    ERR_FAIL_COND(p_env.is_null());

    Sky *sky = sky_owner.get_or_null(environment_get_sky(p_env));
    ERR_FAIL_NULL(sky);

    RID sky_material = sky->material;

    if (!sky_material.is_valid()) {
        sky_material = sky_globals.default_material;
    }

    GLES2::SkyMaterialData *material_data = static_cast<GLES2::SkyMaterialData *>(material_storage->material_get_data(sky_material, RS::SHADER_SKY));
    ERR_FAIL_NULL(material_data);

    GLES2::SkyShaderData *shader_data = material_data->shader_data;
    ERR_FAIL_NULL(shader_data);

    material_storage->shaders.sky_shader.version_set_uniform(SkyShaderGLES2::PROJECTION, p_projection, shader_data->version, shader_data->uniforms);
    material_storage->shaders.sky_shader.version_set_uniform(SkyShaderGLES2::POSITION, p_position, shader_data->version, shader_data->uniforms);
    material_storage->shaders.sky_shader.version_set_uniform(SkyShaderGLES2::TIME, p_time, shader_data->version, shader_data->uniforms);
    material_storage->shaders.sky_shader.version_set_uniform(SkyShaderGLES2::LUMINANCE_MULTIPLIER, p_luminance_multiplier, shader_data->version, shader_data->uniforms);

    material_data->bind_uniforms();

    material_storage->shaders.sky_shader.version_bind_shader(shader_data->version, SkyShaderGLES2::MODE_BACKGROUND);

    // Draw sky
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);

    _draw_sky_primitive();

    // Reset state
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
}

void RasterizerSceneGLES2::_draw_sky_primitive() {
    static const Vector3 verts[4] = {
        Vector3(-1, -1, 1),
        Vector3(1, -1, 1),
        Vector3(1, 1, 1),
        Vector3(-1, 1, 1)
    };

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    glEnableVertexAttribArray(RS::ARRAY_VERTEX);
    glVertexAttribPointer(RS::ARRAY_VERTEX, 3, GL_FLOAT, GL_FALSE, sizeof(Vector3), verts);

    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    glDisableVertexAttribArray(RS::ARRAY_VERTEX);
}

void RasterizerSceneGLES2::_fill_render_list(RenderListType p_render_list, const RenderDataGLES2 *p_render_data, PassMode p_pass_mode, bool p_append) {
    GLES2::MeshStorage *mesh_storage = GLES2::MeshStorage::get_singleton();

    if (p_render_list == RENDER_LIST_OPAQUE) {
        scene_state.used_screen_texture = false;
        scene_state.used_normal_texture = false;
        scene_state.used_depth_texture = false;
    }

    Plane near_plane;
    if (p_render_data->cam_orthogonal) {
        near_plane = Plane(-p_render_data->cam_transform.basis.get_column(Vector3::AXIS_Z), p_render_data->cam_transform.origin);
        near_plane.d += p_render_data->cam_projection.get_z_near();
    }
    float z_max = p_render_data->cam_projection.get_z_far() - p_render_data->cam_projection.get_z_near();

    RenderList *rl = &render_list[p_render_list];

    if (!p_append) {
        rl->clear();
        if (p_render_list == RENDER_LIST_OPAQUE) {
            render_list[RENDER_LIST_ALPHA].clear();
        }
    }

    for (int i = 0; i < (int)p_render_data->instances->size(); i++) {
        GeometryInstanceGLES2 *inst = static_cast<GeometryInstanceGLES2 *>((*p_render_data->instances)[i]);

        // Calculate depth
        float depth = 0;
        if (p_render_data->cam_orthogonal) {
            Vector3 center = inst->transform.origin;
            if (inst->use_aabb_center) {
                center = inst->transformed_aabb.get_support(-near_plane.normal);
            }
            depth = near_plane.distance_to(center) - inst->sorting_offset;
        } else {
            Vector3 center = inst->transform.origin;
            if (inst->use_aabb_center) {
                center = inst->transformed_aabb.position + (inst->transformed_aabb.size * 0.5);
            }
            depth = p_render_data->cam_transform.origin.distance_to(center) - inst->sorting_offset;
        }
        uint32_t depth_layer = CLAMP(int(depth * 16 / z_max), 0, 15);

        uint32_t flags = inst->base_flags; //fill flags if appropriate

        if (inst->non_uniform_scale) {
            flags |= INSTANCE_DATA_FLAGS_NON_UNIFORM_SCALE;
        }

        inst->depth = depth;
        inst->depth_layer = depth_layer;

        GeometryInstanceSurface *surf = inst->surface_caches;

        while (surf) {
            // LOD
            if (p_render_data->screen_mesh_lod_threshold > 0.0 && mesh_storage->mesh_surface_has_lod(surf->surface)) {
                // Skip LOD for GLES2 to keep things simpler
                surf->lod_index = 0;
            }

            // Determine which render lists the surface should be added to
            bool opaque_pass = surf->flags & GeometryInstanceSurface::FLAG_PASS_OPAQUE;
            bool alpha_pass = surf->flags & GeometryInstanceSurface::FLAG_PASS_ALPHA;

            if (p_pass_mode == PASS_MODE_COLOR && opaque_pass) {
                rl->add_element(surf);
            }

            if ((p_pass_mode == PASS_MODE_COLOR || p_pass_mode == PASS_MODE_COLOR_TRANSPARENT) && alpha_pass) {
                render_list[RENDER_LIST_ALPHA].add_element(surf);
            }

            if (p_pass_mode == PASS_MODE_SHADOW && (surf->flags & GeometryInstanceSurface::FLAG_PASS_SHADOW)) {
                rl->add_element(surf);
            }

            surf->sort.depth_layer = depth_layer;

            surf = surf->next;
        }
    }

    if (p_render_list == RENDER_LIST_OPAQUE) {
        render_list[RENDER_LIST_ALPHA].sort_by_reverse_depth_and_priority();
    }

    rl->sort_by_key();
}

void RasterizerSceneGLES2::_setup_environment(const RenderDataGLES2 *p_render_data, bool p_no_fog, const Size2i &p_screen_size, bool p_flip_y, const Color &p_default_bg_color) {
    GLES2::MaterialStorage *material_storage = GLES2::MaterialStorage::get_singleton();

    Projection correction;
    correction.set_depth_correction(p_flip_y, true, false);
    Projection projection = correction * p_render_data->cam_projection;

    //store camera into ubo
    GLES3::MaterialStorage::store_camera(projection, scene_state.ubo.projection_matrix);
    GLES3::MaterialStorage::store_camera(projection.inverse(), scene_state.ubo.inv_projection_matrix);
    GLES3::MaterialStorage::store_transform(p_render_data->cam_transform, scene_state.ubo.inv_view_matrix);
    GLES3::MaterialStorage::store_transform(p_render_data->inv_cam_transform, scene_state.ubo.view_matrix);

    scene_state.ubo.viewport_size[0] = p_screen_size.x;
    scene_state.ubo.viewport_size[1] = p_screen_size.y;

    Size2 screen_pixel_size = Vector2(1.0, 1.0) / Size2(p_screen_size);
    scene_state.ubo.screen_pixel_size[0] = screen_pixel_size.x;
    scene_state.ubo.screen_pixel_size[1] = screen_pixel_size.y;

    // Set up environment uniforms
    Color clear_color = p_default_bg_color;
    float energy = 1.0;
    if (p_render_data->environment.is_valid()) {
        RS::EnvironmentBG bg_mode = environment_get_background(p_render_data->environment);
        energy = environment_get_bg_energy(p_render_data->environment);

        if (bg_mode == RS::ENV_BG_COLOR || bg_mode == RS::ENV_BG_SKY) {
            clear_color = environment_get_bg_color(p_render_data->environment);
        }

        // Set up fog
        if (!p_no_fog && environment_get_fog_enabled(p_render_data->environment)) {
            scene_state.ubo.fog_enabled = 1;
            scene_state.ubo.fog_density = environment_get_fog_density(p_render_data->environment);
            scene_state.ubo.fog_height = environment_get_fog_height(p_render_data->environment);
            scene_state.ubo.fog_height_density = environment_get_fog_height_density(p_render_data->environment);
            Color fog_color = environment_get_fog_light_color(p_render_data->environment).to_linear();
            scene_state.ubo.fog_light_color[0] = fog_color.r * energy;
            scene_state.ubo.fog_light_color[1] = fog_color.g * energy;
            scene_state.ubo.fog_light_color[2] = fog_color.b * energy;
        } else {
            scene_state.ubo.fog_enabled = 0;
        }

        // Ambient light
        Color ambient_color = environment_get_ambient_light(p_render_data->environment);
        float ambient_energy = environment_get_ambient_light_energy(p_render_data->environment);
        scene_state.ubo.ambient_light_color_energy[0] = ambient_color.r * ambient_energy;
        scene_state.ubo.ambient_light_color_energy[1] = ambient_color.g * ambient_energy;
        scene_state.ubo.ambient_light_color_energy[2] = ambient_color.b * ambient_energy;
        scene_state.ubo.ambient_light_color_energy[3] = ambient_energy;
    } else {
        scene_state.ubo.fog_enabled = 0;
        scene_state.ubo.ambient_light_color_energy[0] = 0;
        scene_state.ubo.ambient_light_color_energy[1] = 0;
        scene_state.ubo.ambient_light_color_energy[2] = 0;
        scene_state.ubo.ambient_light_color_energy[3] = 1.0;
    }

    clear_color = clear_color.to_linear();
    scene_state.ubo.bg_color[0] = clear_color.r * energy;
    scene_state.ubo.bg_color[1] = clear_color.g * energy;
    scene_state.ubo.bg_color[2] = clear_color.b * energy;
    scene_state.ubo.bg_color[3] = clear_color.a;

    scene_state.ubo.z_far = p_render_data->z_far;
    scene_state.ubo.z_near = p_render_data->z_near;

    // Update UBO
    glBindBufferBase(GL_UNIFORM_BUFFER, SCENE_DATA_UNIFORM_LOCATION, scene_state.ubo_buffer);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(SceneState::UBO), &scene_state.ubo, GL_STREAM_DRAW);
}

void RasterizerSceneGLES2::_setup_lights(const RenderDataGLES2 *p_render_data, uint32_t &r_directional_light_count, uint32_t &r_omni_light_count, uint32_t &r_spot_light_count) {
    GLES2::LightStorage *light_storage = GLES2::LightStorage::get_singleton();
    GLES2::Config *config = GLES2::Config::get_singleton();

    const Transform3D inverse_transform = p_render_data->inv_cam_transform;

    r_directional_light_count = 0;
    r_omni_light_count = 0;
    r_spot_light_count = 0;

    for (int i = 0; i < (int)p_render_data->lights->size(); i++) {
        GLES2::LightInstance *li = GLES2::LightStorage::get_singleton()->get_light_instance((*p_render_data->lights)[i]);
        if (!li) {
            continue;
        }
        RID base = li->light;

        RS::LightType type = light_storage->light_get_type(base);
        switch (type) {
            case RS::LIGHT_DIRECTIONAL: {
                if (r_directional_light_count < config->max_renderable_lights) {
                    DirectionalLightData &light_data = scene_state.directional_lights[r_directional_light_count];

                    Transform3D light_transform = li->transform;
                    Vector3 direction = inverse_transform.basis.xform(light_transform.basis.xform(Vector3(0, 0, 1))).normalized();

                    light_data.direction[0] = direction.x;
                    light_data.direction[1] = direction.y;
                    light_data.direction[2] = direction.z;

                    float sign = light_storage->light_is_negative(base) ? -1 : 1;

                    light_data.energy = sign * light_storage->light_get_param(base, RS::LIGHT_PARAM_ENERGY);

                    Color linear_col = light_storage->light_get_color(base).to_linear();
                    light_data.color[0] = linear_col.r;
                    light_data.color[1] = linear_col.g;
                    light_data.color[2] = linear_col.b;

                    r_directional_light_count++;
                }
            } break;

            case RS::LIGHT_OMNI: {
                if (r_omni_light_count < config->max_renderable_lights) {
                    LightData &light_data = scene_state.omni_lights[r_omni_light_count];

                    Transform3D light_transform = li->transform;
                    Vector3 pos = inverse_transform.xform(light_transform.origin);

                    light_data.position[0] = pos.x;
                    light_data.position[1] = pos.y;
                    light_data.position[2] = pos.z;

                    float radius = light_storage->light_get_param(base, RS::LIGHT_PARAM_RANGE);
                    light_data.inv_radius = 1.0f / radius;

                    float sign = light_storage->light_is_negative(base) ? -1 : 1;

                    light_data.energy = sign * light_storage->light_get_param(base, RS::LIGHT_PARAM_ENERGY);

                    Color linear_col = light_storage->light_get_color(base).to_linear();
                    light_data.color[0] = linear_col.r;
                    light_data.color[1] = linear_col.g;
                    light_data.color[2] = linear_col.b;

                    light_data.attenuation = light_storage->light_get_param(base, RS::LIGHT_PARAM_ATTENUATION);

                    r_omni_light_count++;
                }
            } break;

            case RS::LIGHT_SPOT: {
                if (r_spot_light_count < config->max_renderable_lights) {
                    LightData &light_data = scene_state.spot_lights[r_spot_light_count];

                    Transform3D light_transform = li->transform;
                    Vector3 pos = inverse_transform.xform(light_transform.origin);
                    Vector3 direction = inverse_transform.basis.xform(light_transform.basis.xform(Vector3(0, 0, -1))).normalized();

                    light_data.position[0] = pos.x;
                    light_data.position[1] = pos.y;
                    light_data.position[2] = pos.z;

                    light_data.direction[0] = direction.x;
                    light_data.direction[1] = direction.y;
                    light_data.direction[2] = direction.z;

                    float radius = light_storage->light_get_param(base, RS::LIGHT_PARAM_RANGE);
                    light_data.inv_radius = 1.0f / radius;

                    float sign = light_storage->light_is_negative(base) ? -1 : 1;

                    light_data.energy = sign * light_storage->light_get_param(base, RS::LIGHT_PARAM_ENERGY);

                    Color linear_col = light_storage->light_get_color(base).to_linear();
                    light_data.color[0] = linear_col.r;
                    light_data.color[1] = linear_col.g;
light_data.color[2] = linear_col.b;

                    light_data.attenuation = light_storage->light_get_param(base, RS::LIGHT_PARAM_ATTENUATION);

                    float spot_angle = light_storage->light_get_param(base, RS::LIGHT_PARAM_SPOT_ANGLE);
                    light_data.spot_attenuation = Math::cos(Math::deg_to_rad(spot_angle));
                    light_data.spot_angle = Math::deg_to_rad(spot_angle);

                    r_spot_light_count++;
                }
            } break;
        }
    }

    // Update light UBOs
    glBindBufferBase(GL_UNIFORM_BUFFER, SCENE_DIRECTIONAL_LIGHT_UNIFORM_LOCATION, scene_state.directional_light_buffer);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(DirectionalLightData) * r_directional_light_count, scene_state.directional_lights, GL_STREAM_DRAW);

    glBindBufferBase(GL_UNIFORM_BUFFER, SCENE_OMNILIGHT_UNIFORM_LOCATION, scene_state.omni_light_buffer);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(LightData) * r_omni_light_count, scene_state.omni_lights, GL_STREAM_DRAW);

    glBindBufferBase(GL_UNIFORM_BUFFER, SCENE_SPOTLIGHT_UNIFORM_LOCATION, scene_state.spot_light_buffer);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(LightData) * r_spot_light_count, scene_state.spot_lights, GL_STREAM_DRAW);
}

void RasterizerSceneGLES2::_setup_reflections(const RenderDataGLES2 *p_render_data) {
    GLES2::TextureStorage *texture_storage = GLES2::TextureStorage::get_singleton();
    GLES2::Config *config = GLES2::Config::get_singleton();

    uint32_t reflection_probe_count = 0;

    for (int i = 0; i < (int)p_render_data->reflection_probes->size(); i++) {
        if (reflection_probe_count >= config->max_renderable_reflections) {
            break;
        }

        RID probe = (*p_render_data->reflection_probes)[i];
        GLES2::ReflectionProbeInstance *rpi = GLES2::LightStorage::get_singleton()->get_reflection_probe_instance(probe);

        if (!rpi) {
            continue;
        }

        ReflectionProbeData &reflection_probe = scene_state.reflections[reflection_probe_count];

        GLES2::MaterialStorage::store_transform(rpi->transform, reflection_probe.transform);
        reflection_probe.intensity = GLES2::LightStorage::get_singleton()->reflection_probe_get_intensity(rpi->probe);
        reflection_probe.interior_ambient = GLES2::LightStorage::get_singleton()->reflection_probe_get_interior_ambient(rpi->probe);
        reflection_probe.interior_ambient_energy = GLES2::LightStorage::get_singleton()->reflection_probe_get_interior_ambient_energy(rpi->probe);
        reflection_probe.max_distance = GLES2::LightStorage::get_singleton()->reflection_probe_get_max_distance(rpi->probe);
        reflection_probe.box_projection = GLES2::LightStorage::get_singleton()->reflection_probe_is_box_projection(rpi->probe);
        reflection_probe.useRGBM = true; // We always use RGBM for reflection probes in GLES2

        if (rpi->reflection_atlas_index >= 0) {
            reflection_probe.atlas_index = rpi->reflection_atlas_index;
            texture_storage->reflection_probe_set_reflection_atlas_index(rpi->probe, rpi->reflection_atlas_index);
        } else {
            reflection_probe.atlas_index = -1;
        }

        reflection_probe_count++;
    }

    // Update reflection probe UBO
    glBindBufferBase(GL_UNIFORM_BUFFER, SCENE_REFLECTION_UNIFORM_LOCATION, scene_state.reflection_buffer);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(ReflectionProbeData) * reflection_probe_count, scene_state.reflections, GL_STREAM_DRAW);
}

void RasterizerSceneGLES2::_post_process(const RenderDataGLES2 *p_render_data) {
    GLES2::TextureStorage *texture_storage = GLES2::TextureStorage::get_singleton();
    GLES2::Config *config = GLES2::Config::get_singleton();

    Ref<RenderSceneBuffersGLES2> rb = p_render_data->render_buffers;
    ERR_FAIL_COND(rb.is_null());

    GLES2::RenderTarget *rt = texture_storage->get_render_target(rb->render_target);
    ERR_FAIL_NULL(rt);

    // Apply tonemap and auto exposure
    if (p_render_data->environment.is_valid()) {
        RENDER_TIMESTAMP("Tonemap");

        glBindFramebuffer(GL_FRAMEBUFFER, rt->fbo);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, rt->color);

        // Use tonemap shader
        GLES2::MaterialStorage::get_singleton()->shaders.copy.set_conditional(CopyShaderGLES2::USE_GLOW_FILTER, false);
        GLES2::MaterialStorage::get_singleton()->shaders.copy.set_conditional(CopyShaderGLES2::USE_AUTO_EXPOSURE, false);
        GLES2::MaterialStorage::get_singleton()->shaders.copy.set_conditional(CopyShaderGLES2::USE_DEBANDING, false);
        GLES2::MaterialStorage::get_singleton()->shaders.copy.bind();

        GLES2::MaterialStorage::get_singleton()->shaders.copy.set_uniform(CopyShaderGLES2::SOURCE, 0);

        // Set tonemap uniforms
        RS::EnvironmentToneMapper tonemap_mode = environment_get_tone_mapper(p_render_data->environment);
        GLES2::MaterialStorage::get_singleton()->shaders.copy.set_uniform(CopyShaderGLES2::TONEMAP_MODE, tonemap_mode);
        GLES2::MaterialStorage::get_singleton()->shaders.copy.set_uniform(CopyShaderGLES2::EXPOSURE, environment_get_exposure(p_render_data->environment));
        GLES2::MaterialStorage::get_singleton()->shaders.copy.set_uniform(CopyShaderGLES2::WHITE, environment_get_white(p_render_data->environment));

        _draw_gui_primitive(4, nullptr, nullptr);

        glBindTexture(GL_TEXTURE_2D, 0);
    }

    // Apply FXAA if enabled
    if (p_render_data->environment.is_valid() && environment_get_fxaa(p_render_data->environment)) {
        RENDER_TIMESTAMP("FXAA");

        glBindFramebuffer(GL_FRAMEBUFFER, rt->fbo_blit);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, rt->color);

        GLES2::MaterialStorage::get_singleton()->shaders.fxaa.bind();
        GLES2::MaterialStorage::get_singleton()->shaders.fxaa.set_uniform(FXAAShaderGLES2::EDGE_THRESHOLD, 0.125f);
        GLES2::MaterialStorage::get_singleton()->shaders.fxaa.set_uniform(FXAAShaderGLES2::EDGE_THRESHOLD_MIN, 0.0625f);
        GLES2::MaterialStorage::get_singleton()->shaders.fxaa.set_uniform(FXAAShaderGLES2::SUBPIX_CAP, 0.75f);
        GLES2::MaterialStorage::get_singleton()->shaders.fxaa.set_uniform(FXAAShaderGLES2::SUBPIX_TRIM, 0.0f);

        _draw_gui_primitive(4, nullptr, nullptr);

        glBindTexture(GL_TEXTURE_2D, 0);

        // Copy back the result
        glBindFramebuffer(GL_READ_FRAMEBUFFER, rt->fbo_blit);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, rt->fbo);
        glBlitFramebuffer(0, 0, rt->width, rt->height, 0, 0, rt->width, rt->height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    }
}

void RasterizerSceneGLES2::render_scene(const Ref<RenderSceneBuffers> &p_render_buffers, const CameraData *p_camera_data, const CameraData *p_prev_camera_data, const PagedArray<RenderGeometryInstance *> &p_instances, const PagedArray<RID> &p_lights, const PagedArray<RID> &p_reflection_probes, const PagedArray<RID> &p_voxel_gi_instances, const PagedArray<RID> &p_decals, const PagedArray<RID> &p_lightmaps, const PagedArray<RID> &p_fog_volumes, RID p_environment, RID p_camera_attributes, RID p_compositor, RID p_shadow_atlas, RID p_occluder_debug_tex, RID p_reflection_atlas, RID p_reflection_probe, int p_reflection_probe_pass, float p_screen_mesh_lod_threshold, const RenderShadowData *p_render_shadows, int p_render_shadow_count, const RenderSDFGIData *p_render_sdfgi_regions, int p_render_sdfgi_region_count, const RenderSDFGIUpdateData *p_sdfgi_update_data, RenderingMethod::RenderInfo *r_render_info) {
    GLES2::TextureStorage *texture_storage = GLES2::TextureStorage::get_singleton();
    GLES2::Config *config = GLES2::Config::get_singleton();

    RENDER_TIMESTAMP("Setup 3D Scene");

    Ref<RenderSceneBuffersGLES2> rb = p_render_buffers;
    ERR_FAIL_COND(rb.is_null());

    GLES2::RenderTarget *rt = texture_storage->get_render_target(rb->render_target);
    ERR_FAIL_NULL(rt);

    // Setup render data
    RenderDataGLES2 render_data;
    render_data.render_buffers = rb;
    render_data.transparent_bg = rt->flags[GLES2::RenderTarget::TRANSPARENT];
    render_data.cam_transform = p_camera_data->main_transform;
    render_data.cam_projection = p_camera_data->main_projection;
    render_data.cam_orthogonal = p_camera_data->is_orthogonal;
    render_data.inv_cam_transform = render_data.cam_transform.affine_inverse();
    render_data.z_near = p_camera_data->main_projection.get_z_near();
    render_data.z_far = p_camera_data->main_projection.get_z_far();
    render_data.instances = &p_instances;
    render_data.lights = &p_lights;
    render_data.reflection_probes = &p_reflection_probes;
    render_data.environment = p_environment;
    render_data.shadow_atlas = p_shadow_atlas;
    render_data.camera_attributes = p_camera_attributes;
    render_data.reflection_probe = p_reflection_probe;
    render_data.reflection_probe_pass = p_reflection_probe_pass;

    PagedArray<RID> empty_array;
    render_data.voxel_gi_instances = &empty_array;
    render_data.fog_volumes = &empty_array;

    render_data.directional_light_count = 0;
    render_data.spot_light_count = 0;
    render_data.omni_light_count = 0;

    render_data.render_info = r_render_info;

    // Setup environment
    _setup_environment(&render_data, false, rb->internal_size, render_data.cam_transform.basis.determinant() < 0, texture_storage->get_default_clear_color());

    // Setup lights
    _setup_lights(&render_data, render_data.directional_light_count, render_data.omni_light_count, render_data.spot_light_count);

    // Setup reflections
    _setup_reflections(&render_data);

    // Render shadows
    _render_shadows(&render_data, render_data.directional_light_count);

    RENDER_TIMESTAMP("Render 3D Scene");

    // Actual rendering
    _fill_render_list(RENDER_LIST_OPAQUE, &render_data, PASS_MODE_COLOR);
    render_list[RENDER_LIST_OPAQUE].sort_by_key();
    render_list[RENDER_LIST_ALPHA].sort_by_reverse_depth_and_priority();

    _render_list_template<PASS_MODE_COLOR>(&render_list[RENDER_LIST_OPAQUE], &render_data, 0, render_list[RENDER_LIST_OPAQUE].elements.size());
    _render_list_template<PASS_MODE_COLOR_TRANSPARENT>(&render_list[RENDER_LIST_ALPHA], &render_data, 0, render_list[RENDER_LIST_ALPHA].elements.size());

    // Apply post-processing effects
    _post_process(&render_data);
}

// ... (more functions can be added as needed)

#endif // GLES2_ENABLED