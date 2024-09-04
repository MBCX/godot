/**************************************************************************/
/*  mesh_storage.cpp                                                      */
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

#include "mesh_storage.h"

#ifdef GLES2_ENABLED

#include "config.h"
#include "material_storage.h"
#include "texture_storage.h"
#include "utilities.h"

using namespace GLES2;

MeshStorage *MeshStorage::singleton = nullptr;

MeshStorage *MeshStorage::get_singleton() {
    return singleton;
}

MeshStorage::MeshStorage() {
    singleton = this;
}

MeshStorage::~MeshStorage() {
    singleton = nullptr;
}

/* MESH API */

RID MeshStorage::mesh_allocate() {
    return mesh_owner.allocate_rid();
}

void MeshStorage::mesh_initialize(RID p_rid) {
    mesh_owner.initialize_rid(p_rid, Mesh());
}

void MeshStorage::mesh_free(RID p_rid) {
    mesh_clear(p_rid);
    mesh_set_shadow_mesh(p_rid, RID());
    Mesh *mesh = mesh_owner.get_or_null(p_rid);
    ERR_FAIL_NULL(mesh);

    mesh->dependency.deleted_notify(p_rid);
    if (mesh->instances.size()) {
        ERR_PRINT("deleting mesh with active instances");
    }
    mesh_owner.free(p_rid);
}

void MeshStorage::mesh_set_blend_shape_count(RID p_mesh, int p_blend_shape_count) {
}

bool MeshStorage::mesh_needs_instance(RID p_mesh, bool p_has_skeleton) {
    return false;
}

void MeshStorage::mesh_add_surface(RID p_mesh, const RS::SurfaceData &p_surface) {
    Mesh *mesh = mesh_owner.get_or_null(p_mesh);
    ERR_FAIL_NULL(mesh);

    ERR_FAIL_COND(mesh->surface_count == RS::MAX_MESH_SURFACES);

#ifdef DEBUG_ENABLED
    // Perform validation checks
    {
        uint32_t stride = 0;
        for (int i = 0; i < RS::ARRAY_MAX; i++) {
            if ((p_surface.format & (1ULL << i))) {
                switch (i) {
                    case RS::ARRAY_VERTEX: {
                        stride += sizeof(float) * 3;
                    } break;
                    case RS::ARRAY_NORMAL: {
                        stride += sizeof(float) * 3;
                    } break;
                    case RS::ARRAY_TANGENT: {
                        stride += sizeof(float) * 4;
                    } break;
                    case RS::ARRAY_COLOR: {
                        stride += sizeof(float) * 4;
                    } break;
                    case RS::ARRAY_TEX_UV: {
                        stride += sizeof(float) * 2;
                    } break;
                    case RS::ARRAY_TEX_UV2: {
                        stride += sizeof(float) * 2;
                    } break;
                    case RS::ARRAY_INDEX: {
                        // Indices are not part of the vertex stride
                    } break;
                }
            }
        }

        int expected_size = stride * p_surface.vertex_count;
        ERR_FAIL_COND_MSG(expected_size != p_surface.vertex_data.size(), "Size of vertex data provided (" + itos(p_surface.vertex_data.size()) + ") does not match expected (" + itos(expected_size) + ")");

        if ((p_surface.format & RS::ARRAY_FORMAT_INDEX)) {
            int expected_index_size = p_surface.index_count * (p_surface.vertex_count <= 65536 ? 2 : 4);
            ERR_FAIL_COND_MSG(expected_index_size != p_surface.index_data.size(), "Size of index data provided (" + itos(p_surface.index_data.size()) + ") does not match expected (" + itos(expected_index_size) + ")");
        }
    }
#endif

    Mesh::Surface *s = memnew(Mesh::Surface);

    s->format = p_surface.format;
    s->primitive = p_surface.primitive;

    if (p_surface.vertex_data.size()) {
        glGenBuffers(1, &s->vertex_buffer);
        glBindBuffer(GL_ARRAY_BUFFER, s->vertex_buffer);
        glBufferData(GL_ARRAY_BUFFER, p_surface.vertex_data.size(), p_surface.vertex_data.ptr(), GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    if (p_surface.index_data.size()) {
        glGenBuffers(1, &s->index_buffer);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s->index_buffer);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, p_surface.index_data.size(), p_surface.index_data.ptr(), GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }

    s->vertex_count = p_surface.vertex_count;
    s->index_count = p_surface.index_count;

    s->aabb = p_surface.aabb;
    s->bone_aabbs = p_surface.bone_aabbs; // Copying for consistency, but not used in GLES2

    if (mesh->surface_count == 0) {
        mesh->aabb = s->aabb;
    } else {
        mesh->aabb.merge_with(s->aabb);
    }

    s->material = p_surface.material;

    mesh->surfaces = (Mesh::Surface **)memrealloc(mesh->surfaces, sizeof(Mesh::Surface *) * (mesh->surface_count + 1));
    mesh->surfaces[mesh->surface_count] = s;
    mesh->surface_count++;

    mesh->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_MESH);
}

void MeshStorage::mesh_surface_update_vertex_region(RID p_mesh, int p_surface, int p_offset, const Vector<uint8_t> &p_data) {
    Mesh *mesh = mesh_owner.get_or_null(p_mesh);
    ERR_FAIL_NULL(mesh);
    ERR_FAIL_UNSIGNED_INDEX((uint32_t)p_surface, mesh->surface_count);
    ERR_FAIL_COND(p_data.size() == 0);

    uint64_t data_size = p_data.size();
    const uint8_t *r = p_data.ptr();

    glBindBuffer(GL_ARRAY_BUFFER, mesh->surfaces[p_surface]->vertex_buffer);
    glBufferSubData(GL_ARRAY_BUFFER, p_offset, data_size, r);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void MeshStorage::mesh_surface_update_attribute_region(RID p_mesh, int p_surface, int p_offset, const Vector<uint8_t> &p_data) {
    // GLES2 doesn't have separate attribute buffers, so this is the same as updating the vertex region
    mesh_surface_update_vertex_region(p_mesh, p_surface, p_offset, p_data);
}

void MeshStorage::mesh_surface_set_material(RID p_mesh, int p_surface, RID p_material) {
    Mesh *mesh = mesh_owner.get_or_null(p_mesh);
    ERR_FAIL_NULL(mesh);
    ERR_FAIL_UNSIGNED_INDEX((uint32_t)p_surface, mesh->surface_count);
    mesh->surfaces[p_surface]->material = p_material;

    mesh->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_MATERIAL);
}

RID MeshStorage::mesh_surface_get_material(RID p_mesh, int p_surface) const {
    Mesh *mesh = mesh_owner.get_or_null(p_mesh);
    ERR_FAIL_NULL_V(mesh, RID());
    ERR_FAIL_UNSIGNED_INDEX_V((uint32_t)p_surface, mesh->surface_count, RID());

    return mesh->surfaces[p_surface]->material;
}

RS::SurfaceData MeshStorage::mesh_get_surface(RID p_mesh, int p_surface) const {
    Mesh *mesh = mesh_owner.get_or_null(p_mesh);
    ERR_FAIL_NULL_V(mesh, RS::SurfaceData());
    ERR_FAIL_UNSIGNED_INDEX_V((uint32_t)p_surface, mesh->surface_count, RS::SurfaceData());

    Mesh::Surface &s = *mesh->surfaces[p_surface];

    RS::SurfaceData sd;
    sd.format = s.format;
    sd.vertex_count = s.vertex_count;
    sd.index_count = s.index_count;
    sd.primitive = s.primitive;

    if (s.vertex_buffer != 0) {
        sd.vertex_data = Utilities::buffer_get_data(GL_ARRAY_BUFFER, s.vertex_buffer, s.vertex_count * sizeof(float) * 3);
    }

    if (s.index_buffer != 0) {
        sd.index_data = Utilities::buffer_get_data(GL_ELEMENT_ARRAY_BUFFER, s.index_buffer, s.index_count * (s.vertex_count <= 65536 ? 2 : 4));
    }

    sd.aabb = s.aabb;

    return sd;
}

void MeshStorage::mesh_set_custom_aabb(RID p_mesh, const AABB &p_aabb) {
    Mesh *mesh = mesh_owner.get_or_null(p_mesh);
    ERR_FAIL_NULL(mesh);
    mesh->custom_aabb = p_aabb;

    mesh->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_AABB);
}

AABB MeshStorage::mesh_get_custom_aabb(RID p_mesh) const {
    Mesh *mesh = mesh_owner.get_or_null(p_mesh);
    ERR_FAIL_NULL_V(mesh, AABB());
    return mesh->custom_aabb;
}

AABB MeshStorage::mesh_get_aabb(RID p_mesh, RID p_skeleton) {
    Mesh *mesh = mesh_owner.get_or_null(p_mesh);
    ERR_FAIL_NULL_V(mesh, AABB());

    if (mesh->custom_aabb != AABB()) {
        return mesh->custom_aabb;
    }

    return mesh->aabb;
}

void MeshStorage::mesh_clear(RID p_mesh) {
    Mesh *mesh = mesh_owner.get_or_null(p_mesh);
    ERR_FAIL_NULL(mesh);

    for (uint32_t i = 0; i < mesh->surface_count; i++) {
        Mesh::Surface *s = mesh->surfaces[i];

        if (s->vertex_buffer) {
            glDeleteBuffers(1, &s->vertex_buffer);
        }
        if (s->index_buffer) {
            glDeleteBuffers(1, &s->index_buffer);
        }

        memdelete(mesh->surfaces[i]);
    }
    if (mesh->surfaces) {
        memfree(mesh->surfaces);
    }

    mesh->surfaces = nullptr;
    mesh->surface_count = 0;
    mesh->aabb = AABB();
    mesh->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_MESH);
}

/* MESH INSTANCE API */

RID MeshStorage::mesh_instance_create(RID p_base) {
    Mesh *mesh = mesh_owner.get_or_null(p_base);
    ERR_FAIL_NULL_V(mesh, RID());

    RID rid = mesh_instance_owner.make_rid();
    MeshInstance *mi = mesh_instance_owner.get_or_null(rid);
    ERR_FAIL_NULL_V(mi, RID());

    mi->mesh = mesh;

    for (uint32_t i = 0; i < mesh->surface_count; i++) {
        mi->surfaces.push_back(MeshInstance::Surface());
        MeshInstance::Surface &s = mi->surfaces.write[i];
        s.vertex_buffer = 0;
        s.index_buffer = 0;
    }

    mi->I = mesh->instances.push_back(mi);

    return rid;
}

void MeshStorage::mesh_instance_free(RID p_rid) {
    MeshInstance *mi = mesh_instance_owner.get_or_null(p_rid);
    ERR_FAIL_NULL(mi);

    for (uint32_t i = 0; i < mi->surfaces.size(); i++) {
        if (mi->surfaces[i].vertex_buffer) {
            glDeleteBuffers(1, &mi->surfaces[i].vertex_buffer);
        }
        if (mi->surfaces[i].index_buffer) {
            glDeleteBuffers(1, &mi->surfaces[i].index_buffer);
        }
    }

    mi->mesh->instances.erase(mi->I);
    mesh_instance_owner.free(p_rid);
}

void MeshStorage::mesh_instance_set_skeleton(RID p_mesh_instance, RID p_skeleton) {
    MeshInstance *mi = mesh_instance_owner.get_or_null(p_mesh_instance);
    ERR_FAIL_NULL(mi);

    mi->skeleton = p_skeleton;
}

void MeshStorage::mesh_instance_set_transform(RID p_mesh_instance, const Transform3D &p_transform) {
    MeshInstance *mi = mesh_instance_owner.get_or_null(p_mesh_instance);
    ERR_FAIL_NULL(mi);

    mi->transform = p_transform;
}

/* MULTIMESH API */

RID MeshStorage::multimesh_allocate() {
    return multimesh_owner.allocate_rid();
}

void MeshStorage::multimesh_initialize(RID p_rid) {
    multimesh_owner.initialize_rid(p_rid);
    MultiMesh *multimesh = multimesh_owner.get_or_null(p_rid);
    ERR_FAIL_NULL(multimesh);

    multimesh->transform_format = RS::MULTIMESH_TRANSFORM_2D;
    multimesh->use_colors = false;
    multimesh->use_custom_data = false;
    multimesh->visible_instances = -1;
    multimesh->aabb = AABB();
    multimesh->buffer = 0;
    multimesh->instance_count = 0;
}

void MeshStorage::multimesh_free(RID p_rid) {
    MultiMesh *multimesh = multimesh_owner.get_or_null(p_rid);
    ERR_FAIL_NULL(multimesh);

    if (multimesh->buffer) {
        glDeleteBuffers(1, &multimesh->buffer);
    }

    multimesh_owner.free(p_rid);
}

void MeshStorage::multimesh_allocate_data(RID p_multimesh, int p_instances, RS::MultimeshTransformFormat p_transform_format, bool p_use_colors, bool p_use_custom_data) {
    MultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
    ERR_FAIL_NULL(multimesh);

    if (multimesh->buffer) {
        glDeleteBuffers(1, &multimesh->buffer);
        multimesh->buffer = 0;
    }

    multimesh->instance_count = p_instances;
    multimesh->transform_format = p_transform_format;
    multimesh->use_colors = p_use_colors;
    multimesh->use_custom_data = p_use_custom_data;

    multimesh->stride_transform = p_transform_format == RS::MULTIMESH_TRANSFORM_2D ? 8 : 12;
    multimesh->stride_color = p_use_colors ? 4 : 0;
    multimesh->stride_custom = p_use_custom_data ? 4 : 0;

    int total_stride = multimesh->stride_transform + multimesh->stride_color + multimesh->stride_custom;

    glGenBuffers(1, &multimesh->buffer);
    glBindBuffer(GL_ARRAY_BUFFER, multimesh->buffer);
    glBufferData(GL_ARRAY_BUFFER, p_instances * total_stride * sizeof(float), NULL, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

int MeshStorage::multimesh_get_instance_count(RID p_multimesh) const {
    MultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
    ERR_FAIL_NULL_V(multimesh, 0);

    return multimesh->instance_count;
}

void MeshStorage::multimesh_set_mesh(RID p_multimesh, RID p_mesh) {
    MultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
    ERR_FAIL_NULL(multimesh);

    multimesh->mesh = p_mesh;
}

void MeshStorage::multimesh_instance_set_transform(RID p_multimesh, int p_index, const Transform3D &p_transform) {
    MultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
    ERR_FAIL_NULL(multimesh);
    ERR_FAIL_INDEX(p_index, multimesh->instance_count);

    int stride = multimesh->stride_transform + multimesh->stride_color + multimesh->stride_custom;
    float *data = (float *)malloc(stride * sizeof(float));

    if (multimesh->transform_format == RS::MULTIMESH_TRANSFORM_3D) {
        data[0] = p_transform.basis.elements[0][0];
        data[1] = p_transform.basis.elements[1][0];
        data[2] = p_transform.basis.elements[2][0];
        data[3] = p_transform.origin.x;
        data[4] = p_transform.basis.elements[0][1];
        data[5] = p_transform.basis.elements[1][1];
        data[6] = p_transform.basis.elements[2][1];
        data[7] = p_transform.origin.y;
        data[8] = p_transform.basis.elements[0][2];
        data[9] = p_transform.basis.elements[1][2];
        data[10] = p_transform.basis.elements[2][2];
        data[11] = p_transform.origin.z;
    } else {
        data[0] = p_transform.basis.elements[0][0];
        data[1] = p_transform.basis.elements[0][1];
        data[2] = p_transform.origin.x;
        data[3] = p_transform.basis.elements[1][0];
        data[4] = p_transform.basis.elements[1][1];
        data[5] = p_transform.origin.y;
        data[6] = 0;
        data[7] = 0;
    }

    glBindBuffer(GL_ARRAY_BUFFER, multimesh->buffer);
    glBufferSubData(GL_ARRAY_BUFFER, p_index * stride * sizeof(float), multimesh->stride_transform * sizeof(float), data);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    free(data);
}

void MeshStorage::multimesh_instance_set_transform_2d(RID p_multimesh, int p_index, const Transform2D &p_transform) {
    MultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
    ERR_FAIL_NULL(multimesh);
    ERR_FAIL_INDEX(p_index, multimesh->instance_count);
    ERR_FAIL_COND(multimesh->transform_format != RS::MULTIMESH_TRANSFORM_2D);

    int stride = multimesh->stride_transform + multimesh->stride_color + multimesh->stride_custom;
    float *data = (float *)malloc(stride * sizeof(float));

    data[0] = p_transform.elements[0][0];
    data[1] = p_transform.elements[1][0];
    data[2] = 0;
    data[3] = p_transform.elements[2][0];
    data[4] = p_transform.elements[0][1];
    data[5] = p_transform.elements[1][1];
    data[6] = 0;
    data[7] = p_transform.elements[2][1];

    glBindBuffer(GL_ARRAY_BUFFER, multimesh->buffer);
    glBufferSubData(GL_ARRAY_BUFFER, p_index * stride * sizeof(float), multimesh->stride_transform * sizeof(float), data);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    free(data);
}

void MeshStorage::multimesh_instance_set_color(RID p_multimesh, int p_index, const Color &p_color) {
    MultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
    ERR_FAIL_NULL(multimesh);
    ERR_FAIL_INDEX(p_index, multimesh->instance_count);
    ERR_FAIL_COND(!multimesh->use_colors);

    int stride = multimesh->stride_transform + multimesh->stride_color + multimesh->stride_custom;
    float *data = (float *)malloc(multimesh->stride_color * sizeof(float));

    data[0] = p_color.r;
    data[1] = p_color.g;
    data[2] = p_color.b;
    data[3] = p_color.a;

    glBindBuffer(GL_ARRAY_BUFFER, multimesh->buffer);
    glBufferSubData(GL_ARRAY_BUFFER, p_index * stride * sizeof(float) + multimesh->stride_transform * sizeof(float), multimesh->stride_color * sizeof(float), data);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    free(data);
}

void MeshStorage::multimesh_instance_set_custom_data(RID p_multimesh, int p_index, const Color &p_custom_data) {
    MultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
    ERR_FAIL_NULL(multimesh);
    ERR_FAIL_INDEX(p_index, multimesh->instance_count);
    ERR_FAIL_COND(!multimesh->use_custom_data);

    int stride = multimesh->stride_transform + multimesh->stride_color + multimesh->stride_custom;
    float *data = (float *)malloc(multimesh->stride_custom * sizeof(float));

    data[0] = p_custom_data.r;
    data[1] = p_custom_data.g;
    data[2] = p_custom_data.b;
    data[3] = p_custom_data.a;

    glBindBuffer(GL_ARRAY_BUFFER, multimesh->buffer);
    glBufferSubData(GL_ARRAY_BUFFER, p_index * stride * sizeof(float) + (multimesh->stride_transform + multimesh->stride_color) * sizeof(float), multimesh->stride_custom * sizeof(float), data);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    free(data);
}

RID MeshStorage::multimesh_get_mesh(RID p_multimesh) const {
    MultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
    ERR_FAIL_NULL_V(multimesh, RID());

    return multimesh->mesh;
}

AABB MeshStorage::multimesh_get_aabb(RID p_multimesh) const {
    MultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
    ERR_FAIL_NULL_V(multimesh, AABB());

    if (multimesh->aabb_dirty) {
        const_cast<MeshStorage *>(this)->_update_multimesh_aabb(multimesh);
    }

    return multimesh->aabb;
}

void MeshStorage::_update_multimesh_aabb(MultiMesh *multimesh) {
    if (multimesh->instance_count == 0 || multimesh->mesh.is_null()) {
        multimesh->aabb = AABB();
        return;
    }

    AABB mesh_aabb = mesh_get_aabb(multimesh->mesh);
    AABB aabb;

    Vector3 position;
    Vector3 scale;
    float *buffer = (float *)malloc(multimesh->instance_count * (multimesh->stride_transform + multimesh->stride_color + multimesh->stride_custom) * sizeof(float));

    glBindBuffer(GL_ARRAY_BUFFER, multimesh->buffer);
    glGetBufferSubData(GL_ARRAY_BUFFER, 0, multimesh->instance_count * (multimesh->stride_transform + multimesh->stride_color + multimesh->stride_custom) * sizeof(float), buffer);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    for (int i = 0; i < multimesh->instance_count; i++) {
        int offset = i * (multimesh->stride_transform + multimesh->stride_color + multimesh->stride_custom);

        if (multimesh->transform_format == RS::MULTIMESH_TRANSFORM_3D) {
            position = Vector3(buffer[offset + 3], buffer[offset + 7], buffer[offset + 11]);
            scale = Vector3(
                Vector3(buffer[offset + 0], buffer[offset + 1], buffer[offset + 2]).length(),
                Vector3(buffer[offset + 4], buffer[offset + 5], buffer[offset + 6]).length(),
                Vector3(buffer[offset + 8], buffer[offset + 9], buffer[offset + 10]).length());
        } else {
            position = Vector3(buffer[offset + 3], buffer[offset + 7], 0);
            scale = Vector3(
                Vector2(buffer[offset + 0], buffer[offset + 1]).length(),
                Vector2(buffer[offset + 4], buffer[offset + 5]).length(),
                1);
        }

        AABB instance_aabb = mesh_aabb;
        instance_aabb.position *= scale;
        instance_aabb.size *= scale;
        instance_aabb.position += position;

        if (i == 0) {
            aabb = instance_aabb;
        } else {
            aabb.merge_with(instance_aabb);
        }
    }

    free(buffer);

    multimesh->aabb = aabb;
    multimesh->aabb_dirty = false;
}

void MeshStorage::multimesh_set_visible_instances(RID p_multimesh, int p_visible) {
    MultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
    ERR_FAIL_NULL(multimesh);

    multimesh->visible_instances = p_visible;
}

int MeshStorage::multimesh_get_visible_instances(RID p_multimesh) const {
    MultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
    ERR_FAIL_NULL_V(multimesh, 0);

    return multimesh->visible_instances;
}

#endif // GLES2_ENABLED
