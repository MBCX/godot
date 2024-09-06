/**************************************************************************/
/*  particles_storage.h                                                   */
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

#ifndef PARTICLES_STORAGE_GLES2_H
#define PARTICLES_STORAGE_GLES2_H

#ifdef GLES2_ENABLED

#include "core/templates/local_vector.h"
#include "core/templates/rid_owner.h"
#include "core/templates/self_list.h"
#include "servers/rendering/storage/particles_storage.h"
#include "servers/rendering/storage/utilities.h"

#include "platform_gl.h"

namespace GLES2 {

class ParticlesStorage : public RendererParticlesStorage {
private:
    static ParticlesStorage *singleton;

	struct ParticleInstanceData2D {
		float xform[8];
		float color[4];
		float custom[4];
	};

	struct ParticleInstanceData3D {
		float xform[12];
		float color[4];
		float custom[4];
	};

	struct Particles {
		RS::ParticlesMode mode = RS::PARTICLES_MODE_3D;
		bool inactive = true;
		double inactive_time = 0.0;
		bool emitting = false;
		bool one_shot = false;
		int amount = 0;
		double lifetime = 1.0;
		double pre_process_time = 0.0;
		real_t explosiveness = 0.0;
		real_t randomness = 0.0;
		bool restart_request = false;
		AABB custom_aabb = AABB(Vector3(-4, -4, -4), Vector3(8, 8, 8));
		bool use_local_coords = false;

		RID process_material;
		uint32_t frame_counter = 0;
		RS::ParticlesTransformAlign transform_align = RS::PARTICLES_TRANSFORM_ALIGN_DISABLED;

		RS::ParticlesDrawOrder draw_order = RS::PARTICLES_DRAW_ORDER_INDEX;

		Vector<RID> draw_passes;

		GLuint vertex_buffer = 0;
		GLuint color_buffer = 0;
		GLuint custom_buffer = 0;
		uint32_t vertex_buffer_size = 0;
		uint32_t color_buffer_size = 0;
		uint32_t custom_buffer_size = 0;

		SelfList<Particles> update_list;

		double phase = 0.0;
		double prev_phase = 0.0;
		uint64_t prev_ticks = 0;
		uint32_t random_seed = 0;

		uint32_t cycle_number = 0;

		double speed_scale = 1.0;

		int fixed_fps = 0;
		bool fractional_delta = false;
		double frame_remainder = 0;

		bool clear = true;

		Transform3D emission_transform;

		Dependency dependency;

		Particles() : update_list(this) {}
	};

	struct ParticlesShader {
		RID default_shader;
		RID default_material;
		RID default_shader_version;
	};
	
	mutable RID_Owner<Particles, true> particles_owner;

public:
    static ParticlesStorage *get_singleton();

    ParticlesStorage();
    virtual ~ParticlesStorage();

    bool free(RID p_rid);

public:
    // Particles

    virtual RID particles_allocate() override;
    virtual void particles_initialize(RID p_rid) override;
    virtual void particles_free(RID p_rid) override;

    virtual void particles_set_mode(RID p_particles, RS::ParticlesMode p_mode) override;
    virtual void particles_emit(RID p_particles, const Transform3D &p_transform, const Vector3 &p_velocity, const Color &p_color, const Color &p_custom, uint32_t p_emit_flags) override;
    virtual void particles_set_emitting(RID p_particles, bool p_emitting) override;
    virtual void particles_set_amount(RID p_particles, int p_amount) override;
    virtual void particles_set_lifetime(RID p_particles, double p_lifetime) override;
    virtual void particles_set_one_shot(RID p_particles, bool p_one_shot) override;
    virtual void particles_set_pre_process_time(RID p_particles, double p_time) override;
    virtual void particles_set_explosiveness_ratio(RID p_particles, real_t p_ratio) override;
    virtual void particles_set_randomness_ratio(RID p_particles, real_t p_ratio) override;
    virtual void particles_set_custom_aabb(RID p_particles, const AABB &p_aabb) override;
    virtual void particles_set_speed_scale(RID p_particles, double p_scale) override;
    virtual void particles_set_use_local_coordinates(RID p_particles, bool p_enable) override;
    virtual void particles_set_process_material(RID p_particles, RID p_material) override;
    virtual void particles_set_fixed_fps(RID p_particles, int p_fps) override;
    virtual void particles_set_fractional_delta(RID p_particles, bool p_enable) override;
    virtual void particles_set_view_axis(RID p_particles, const Vector3 &p_axis, const Vector3 &p_up_axis) override;
    virtual void particles_set_collision_base_size(RID p_particles, real_t p_size) override;

    virtual void particles_set_transform_align(RID p_particles, RS::ParticlesTransformAlign p_transform_align) override;

    virtual void particles_set_draw_order(RID p_particles, RS::ParticlesDrawOrder p_order) override;

    virtual void particles_set_draw_passes(RID p_particles, int p_passes) override;
    virtual void particles_set_draw_pass_mesh(RID p_particles, int p_pass, RID p_mesh) override;

    virtual void particles_restart(RID p_particles) override;

    virtual void particles_request_process(RID p_particles) override;
    virtual AABB particles_get_current_aabb(RID p_particles) override;
    virtual AABB particles_get_aabb(RID p_particles) const override;

    virtual void particles_set_emission_transform(RID p_particles, const Transform3D &p_transform) override;

    virtual bool particles_get_emitting(RID p_particles) override;
    virtual int particles_get_draw_passes(RID p_particles) const override;
    virtual RID particles_get_draw_pass_mesh(RID p_particles, int p_pass) const override;

    virtual void particles_add_collision(RID p_particles, RID p_instance) override; // Not implemented in GLES2
    virtual void particles_remove_collision(RID p_particles, RID p_instance) override; // Not implemented in GLES2

    virtual void particles_set_canvas_sdf_collision(RID p_particles, bool p_enable, const Transform2D &p_xform, const Rect2 &p_to_screen, GLuint p_texture);

    virtual void update_particles() override;

    virtual bool particles_is_inactive(RID p_particles) const override;

	_FORCE_INLINE_ RS::ParticlesMode particles_get_mode(RID p_particles) {
		Particles *particles = particles_owner.get_or_null(p_particles);
		ERR_FAIL_NULL_V(particles, RS::PARTICLES_MODE_2D);
		return particles->mode;
	}
	
    _FORCE_INLINE_ uint32_t particles_get_amount(RID p_particles) const {
        const Particles *particles = particles_owner.get_or_null(p_particles);
        ERR_FAIL_NULL_V(particles, 0);
        return particles->amount;
    }

    _FORCE_INLINE_ bool particles_has_collision(RID p_particles) const {
        // Always return false in GLES2 as collision is not implemented
        return false;
    }

    Dependency *particles_get_dependency(RID p_particles) const;

};

} // namespace GLES2

#endif // GLES2_ENABLED

#endif // PARTICLES_STORAGE_GLES2_H
