/**************************************************************************/
/*  particles_storage.cpp                                                 */
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

#include "particles_storage.h"
#include "config.h"
#include "material_storage.h"
#include "mesh_storage.h"
#include "texture_storage.h"
#include "utilities.h"

#include "servers/rendering/rendering_server_default.h"

using namespace GLES2;

ParticlesStorage *ParticlesStorage::singleton = nullptr;

ParticlesStorage *ParticlesStorage::get_singleton() {
    return singleton;
}

ParticlesStorage::ParticlesStorage() {
    singleton = this;

    // Initialize default particle shader
    {
        particles_shader.default_shader = MaterialStorage::get_singleton()->shader_allocate();
        MaterialStorage::get_singleton()->shader_initialize(particles_shader.default_shader);
        MaterialStorage::get_singleton()->shader_set_code(particles_shader.default_shader, R"(
// Default particles shader.

shader_type particles;

void vertex() {
    MODELVIEW_MATRIX = INV_CAMERA_MATRIX * mat4(CAMERA_MATRIX[0], CAMERA_MATRIX[1], CAMERA_MATRIX[2], WORLD_MATRIX[3]);
    MODELVIEW_MATRIX = MODELVIEW_MATRIX * mat4(vec4(length(WORLD_MATRIX[0].xyz), 0.0, 0.0, 0.0), vec4(0.0, length(WORLD_MATRIX[1].xyz), 0.0, 0.0), vec4(0.0, 0.0, length(WORLD_MATRIX[2].xyz), 0.0), vec4(0.0, 0.0, 0.0, 1.0));
    VERTEX = VERTEX;
}

void fragment() {
    ALBEDO = COLOR.rgb;
    ALPHA = COLOR.a;
}
)");
        particles_shader.default_material = MaterialStorage::get_singleton()->material_allocate();
        MaterialStorage::get_singleton()->material_initialize(particles_shader.default_material);
        MaterialStorage::get_singleton()->material_set_shader(particles_shader.default_material, particles_shader.default_shader);
    }
}

ParticlesStorage::~ParticlesStorage() {
    singleton = nullptr;
    MaterialStorage::get_singleton()->material_free(particles_shader.default_material);
    MaterialStorage::get_singleton()->shader_free(particles_shader.default_shader);
}

bool ParticlesStorage::free(RID p_rid) {
    if (particles_owner.owns(p_rid)) {
        particles_free(p_rid);
        return true;
    }
    return false;
}

RID ParticlesStorage::particles_allocate() {
    return particles_owner.allocate_rid();
}

void ParticlesStorage::particles_initialize(RID p_rid) {
    particles_owner.initialize_rid(p_rid, Particles());
}

void ParticlesStorage::particles_free(RID p_rid) {
    Particles *particles = particles_owner.get_or_null(p_rid);
    ERR_FAIL_NULL(particles);

    particles->dependency.deleted_notify(p_rid);
    particles->update_list.remove_from_list();

    if (particles->vertex_buffer != 0) {
        glDeleteBuffers(1, &particles->vertex_buffer);
    }
    if (particles->color_buffer != 0) {
        glDeleteBuffers(1, &particles->color_buffer);
    }
    if (particles->custom_buffer != 0) {
        glDeleteBuffers(1, &particles->custom_buffer);
    }

    particles_owner.free(p_rid);
}

void ParticlesStorage::particles_set_mode(RID p_particles, RS::ParticlesMode p_mode) {
    Particles *particles = particles_owner.get_or_null(p_particles);
    ERR_FAIL_NULL(particles);
    particles->mode = p_mode;
}

void ParticlesStorage::particles_set_emitting(RID p_particles, bool p_emitting) {
    Particles *particles = particles_owner.get_or_null(p_particles);
    ERR_FAIL_NULL(particles);

    particles->emitting = p_emitting;
}

bool ParticlesStorage::particles_get_emitting(RID p_particles) {
    Particles *particles = particles_owner.get_or_null(p_particles);
    ERR_FAIL_NULL_V(particles, false);

    return particles->emitting;
}

#endif // GLES2_ENABLED
