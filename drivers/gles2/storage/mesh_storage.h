/**************************************************************************/
/*  mesh_storage.h                                                        */
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

#ifndef MESH_STORAGE_GLES2_H
#define MESH_STORAGE_GLES2_H

#ifdef GLES2_ENABLED

#include "core/templates/local_vector.h"
#include "core/templates/rid_owner.h"
#include "core/templates/self_list.h"
#include "servers/rendering/storage/mesh_storage.h"
#include "servers/rendering/storage/utilities.h"

#include "platform_gl.h"

namespace GLES2 {

struct MeshInstance;

class MeshStorage : public RendererMeshStorage {
private:
    static MeshStorage *singleton;

    /* MESH */

	struct Mesh {
		struct Surface {
			struct Attrib {
				bool enabled;
				bool integer;
				GLint size;
				GLenum type;
				GLboolean normalized;
				GLsizei stride;
				uint32_t offset;
			};
			RS::PrimitiveType primitive = RS::PRIMITIVE_POINTS;
			uint32_t format = 0;

			GLuint vertex_buffer = 0;
			GLuint index_buffer = 0;
			GLuint uniform_buffer = 0;

			uint32_t vertex_count = 0;
			uint32_t index_count = 0;

			AABB aabb;

			Vector<AABB> bone_aabbs;

			Vector4 uv_scale;

			RID material;
		};

		uint32_t blend_shape_count = 0;
		RS::BlendShapeMode blend_shape_mode = RS::BLEND_SHAPE_MODE_NORMALIZED;

		Surface **surfaces = nullptr;
		uint32_t surface_count = 0;

		AABB aabb;
		AABB custom_aabb;

		Vector<RID> material_cache;

		RID shadow_mesh;
		HashSet<Mesh *> shadow_owners;

		String path;

		Dependency dependency;
	};

	/* MESH INSTANCE */

    struct MeshInstance {
        Mesh *mesh = nullptr;
        RID skeleton;
        struct Surface {
            GLuint vertex_array = 0;
            GLuint vertex_buffer = 0;
            GLuint index_buffer = 0;
        };
        LocalVector<Surface> surfaces;

        Transform2D canvas_item_transform_2d;
        MeshInstance() {}
    };

public:
    static MeshStorage *get_singleton();

    MeshStorage();
    virtual ~MeshStorage();

    /* MESH API */

    virtual RID mesh_allocate() override;
    virtual void mesh_initialize(RID p_rid) override;
    virtual void mesh_free(RID p_rid) override;

    virtual void mesh_set_blend_shape_count(RID p_mesh, int p_blend_shape_count) override {}
    virtual bool mesh_needs_instance(RID p_mesh, bool p_has_skeleton) override;

    virtual void mesh_add_surface(RID p_mesh, const RS::SurfaceData &p_surface) override;

    virtual int mesh_get_blend_shape_count(RID p_mesh) const override { return 0; }

    virtual void mesh_set_blend_shape_mode(RID p_mesh, RS::BlendShapeMode p_mode) override {}
    virtual RS::BlendShapeMode mesh_get_blend_shape_mode(RID p_mesh) const override { return RS::BLEND_SHAPE_MODE_NORMALIZED; }

    virtual void mesh_surface_update_vertex_region(RID p_mesh, int p_surface, int p_offset, const Vector<uint8_t> &p_data) override;
    virtual void mesh_surface_update_attribute_region(RID p_mesh, int p_surface, int p_offset, const Vector<uint8_t> &p_data) override;
    virtual void mesh_surface_update_skin_region(RID p_mesh, int p_surface, int p_offset, const Vector<uint8_t> &p_data) override {}

    virtual void mesh_surface_set_material(RID p_mesh, int p_surface, RID p_material) override;
    virtual RID mesh_surface_get_material(RID p_mesh, int p_surface) const override;

    virtual RS::SurfaceData mesh_get_surface(RID p_mesh, int p_surface) const override;
    virtual int mesh_get_surface_count(RID p_mesh) const override;

    virtual void mesh_set_custom_aabb(RID p_mesh, const AABB &p_aabb) override;
    virtual AABB mesh_get_custom_aabb(RID p_mesh) const override;

    virtual AABB mesh_get_aabb(RID p_mesh, RID p_skeleton = RID()) override;

    virtual void mesh_set_shadow_mesh(RID p_mesh, RID p_shadow_mesh) override;

    virtual void mesh_clear(RID p_mesh) override;

virtual void mesh_set_path(RID p_mesh, const String &p_path) override;
    virtual String mesh_get_path(RID p_mesh) const override;

    /* MESH INSTANCE API */

    virtual RID mesh_instance_create(RID p_base) override;
    virtual void mesh_instance_free(RID p_rid) override;

    virtual void mesh_instance_set_skeleton(RID p_mesh_instance, RID p_skeleton) override;
    virtual void mesh_instance_set_transform(RID p_mesh_instance, const Transform3D &p_transform) override;

    /* MULTIMESH API */

    virtual RID multimesh_allocate() override;
    virtual void multimesh_initialize(RID p_rid) override;
    virtual void multimesh_free(RID p_rid) override;

    virtual void multimesh_allocate_data(RID p_multimesh, int p_instances, RS::MultimeshTransformFormat p_transform_format, bool p_use_colors = false, bool p_use_custom_data = false) override;
    virtual int multimesh_get_instance_count(RID p_multimesh) const override;

    virtual void multimesh_set_mesh(RID p_multimesh, RID p_mesh) override;
    virtual void multimesh_instance_set_transform(RID p_multimesh, int p_index, const Transform3D &p_transform) override;
    virtual void multimesh_instance_set_transform_2d(RID p_multimesh, int p_index, const Transform2D &p_transform) override;
    virtual void multimesh_instance_set_color(RID p_multimesh, int p_index, const Color &p_color) override;
    virtual void multimesh_instance_set_custom_data(RID p_multimesh, int p_index, const Color &p_custom_data) override;

    virtual RID multimesh_get_mesh(RID p_multimesh) const override;
    virtual AABB multimesh_get_aabb(RID p_multimesh) const override;

    virtual Transform3D multimesh_instance_get_transform(RID p_multimesh, int p_index) const override;
    virtual Transform2D multimesh_instance_get_transform_2d(RID p_multimesh, int p_index) const override;
    virtual Color multimesh_instance_get_color(RID p_multimesh, int p_index) const override;
    virtual Color multimesh_instance_get_custom_data(RID p_multimesh, int p_index) const override;

    virtual void multimesh_set_buffer(RID p_multimesh, const Vector<float> &p_buffer) override;
    virtual Vector<float> multimesh_get_buffer(RID p_multimesh) const override;

    virtual void multimesh_set_visible_instances(RID p_multimesh, int p_visible) override;
    virtual int multimesh_get_visible_instances(RID p_multimesh) const override;

    /* Misc */

    virtual void mesh_render_blend_shapes(RID p_mesh, const RID *p_surfaces, uint32_t p_surface_count) override {}

    virtual void update_mesh_instances() override {}

    _FORCE_INLINE_ RS::PrimitiveType mesh_surface_get_primitive(void *p_surface) {
        const Mesh::Surface *s = (const Mesh::Surface *)p_surface;
        return s->primitive;
    }

    _FORCE_INLINE_ void mesh_surface_get_vertex_arrays_and_format(void *p_surface, uint32_t p_input_mask, GLuint &r_vertex_array_gl) {
        Mesh::Surface *s = (Mesh::Surface *)p_surface;

        // TODO: Implement attribute setup for GLES2
    }

    _FORCE_INLINE_ void mesh_instance_surface_get_vertex_arrays_and_format(RID p_mesh_instance, uint32_t p_surface_index, uint32_t p_input_mask, GLuint &r_vertex_array_gl) {
        MeshInstance *mi = mesh_instance_owner.get_or_null(p_mesh_instance);
        ERR_FAIL_NULL(mi);
        Mesh *mesh = mi->mesh;
        ERR_FAIL_UNSIGNED_INDEX(p_surface_index, mesh->surface_count);

        // TODO: Implement attribute setup for GLES2
    }

    _FORCE_INLINE_ uint32_t mesh_surface_get_render_index_offset(void *p_surface) {
        const Mesh::Surface *s = (const Mesh::Surface *)p_surface;
        return s->index_buffer ? 0 : s->vertex_count;
    }
};

} // namespace GLES2

#endif // GLES2_ENABLED

#endif // MESH_STORAGE_GLES2_H
