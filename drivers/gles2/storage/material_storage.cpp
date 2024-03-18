/**************************************************************************/
/*  config.cpp                                                            */
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

#if defined(GLES2_ENABLED)

#include "material_storage.h"
#include "drivers/gles2/shaders/canvas.glsl.gen.h"
#include "drivers/gles_common/storage/texture_storage.h"

using namespace GLES;

void CanvasMaterialDataGLES2::update_uniform_values(const HashMap<StringName, Variant> &p_parameters, bool p_uniform_dirty, const HashMap<StringName, ShaderLanguage::ShaderNode::Uniform> &p_uniforms, const uint32_t *p_uniform_offsets, uint32_t p_ubo_size) {
	/* We don't have ubos
	if ((uint32_t)ubo_data.size() != p_ubo_size) {
		p_uniform_dirty = true;
		if (!uniform_buffer) {
			glGenBuffers(1, &uniform_buffer);
		}

		ubo_data.resize(p_ubo_size);
		if (ubo_data.size()) {
			memset(ubo_data.ptrw(), 0, ubo_data.size()); //clear
		}
	}

	//check whether buffer changed
	if (p_uniform_dirty && ubo_data.size()) {
		update_uniform_buffer(p_uniforms, p_uniform_offsets, p_parameters, ubo_data.ptrw(), ubo_data.size());
		glBindBuffer(GL_UNIFORM_BUFFER, uniform_buffer);
		glBufferData(GL_UNIFORM_BUFFER, ubo_data.size(), ubo_data.ptrw(), GL_DYNAMIC_DRAW);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}
	*/
	uniform_values = p_parameters;
};

void CanvasMaterialDataGLES2::bind_uniforms() {
	//TODO: Bind Material Uniforms

	bind_uniforms_generic(texture_cache, shader_data->texture_uniforms, 1, filter_from_uniform_canvas, repeat_from_uniform_canvas); // Start at GL_TEXTURE1 because texture slot 0 is used by the base texture
}

GLES::MaterialData *GLES::_create_canvas_material_func(GLES::ShaderData *p_shader) {
	CanvasMaterialDataGLES2 *material_data = memnew(CanvasMaterialDataGLES2);
	material_data->shader_data = static_cast<GLES::CanvasShaderData *>(p_shader);
	//update will happen later anyway so do nothing.
	return material_data;
}

////////////////////////////////////////////////////////////////////////////////
// SKY SHADER

void SkyMaterialDataGLES2::update_uniform_values(const HashMap<StringName, Variant> &p_parameters, bool p_uniform_dirty, const HashMap<StringName, ShaderLanguage::ShaderNode::Uniform> &p_uniforms, const uint32_t *p_uniform_offsets, uint32_t p_ubo_size) {
	/* We don't have ubos
	if ((uint32_t)ubo_data.size() != p_ubo_size) {
		p_uniform_dirty = true;
		if (!uniform_buffer) {
			glGenBuffers(1, &uniform_buffer);
		}

		ubo_data.resize(p_ubo_size);
		if (ubo_data.size()) {
			memset(ubo_data.ptrw(), 0, ubo_data.size()); //clear
		}
	}

	//check whether buffer changed
	if (p_uniform_dirty && ubo_data.size()) {
		update_uniform_buffer(p_uniforms, p_uniform_offsets, p_parameters, ubo_data.ptrw(), ubo_data.size());
		glBindBuffer(GL_UNIFORM_BUFFER, uniform_buffer);
		glBufferData(GL_UNIFORM_BUFFER, ubo_data.size(), ubo_data.ptrw(), GL_DYNAMIC_DRAW);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}
	*/
	uniform_values = p_parameters;
};

void SkyMaterialDataGLES2::bind_uniforms() {
	// Bind Material Uniforms
	// TODO: set uniforms based on uniform_values

	bind_uniforms_generic(texture_cache, shader_data->texture_uniforms);
}

GLES::MaterialData *GLES::_create_sky_material_func(GLES::ShaderData *p_shader) {
	GLES::SkyMaterialData *material_data = memnew(SkyMaterialDataGLES2);
	material_data->shader_data = static_cast<GLES::SkyShaderData *>(p_shader);
	//update will happen later anyway so do nothing.
	return material_data;
}

////////////////////////////////////////////////////////////////////////////////
// SCENE SHADER

void SceneMaterialDataGLES2::update_uniform_values(const HashMap<StringName, Variant> &p_parameters, bool p_uniform_dirty, const HashMap<StringName, ShaderLanguage::ShaderNode::Uniform> &p_uniforms, const uint32_t *p_uniform_offsets, uint32_t p_ubo_size) {
	/* We don't have ubos
	if ((uint32_t)ubo_data.size() != p_ubo_size) {
		p_uniform_dirty = true;
		if (!uniform_buffer) {
			glGenBuffers(1, &uniform_buffer);
		}

		ubo_data.resize(p_ubo_size);
		if (ubo_data.size()) {
			memset(ubo_data.ptrw(), 0, ubo_data.size()); //clear
		}
	}

	//check whether buffer changed
	if (p_uniform_dirty && ubo_data.size()) {
		update_uniform_buffer(p_uniforms, p_uniform_offsets, p_parameters, ubo_data.ptrw(), ubo_data.size());
		glBindBuffer(GL_UNIFORM_BUFFER, uniform_buffer);
		glBufferData(GL_UNIFORM_BUFFER, ubo_data.size(), ubo_data.ptrw(), GL_DYNAMIC_DRAW);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}
	*/
	uniform_values = p_parameters;
};

void SceneMaterialDataGLES2::bind_uniforms() {
	// Bind Material Uniforms
	// TODO: set uniforms based on uniform_values

	bind_uniforms_generic(texture_cache, shader_data->texture_uniforms);
}

GLES::MaterialData *GLES::_create_scene_material_func(GLES::ShaderData *p_shader) {
	GLES::SceneMaterialData *material_data = memnew(SceneMaterialDataGLES2);
	material_data->shader_data = static_cast<GLES::SceneShaderData *>(p_shader);
	//update will happen later anyway so do nothing.
	return material_data;
}

////////////////////////////////////////////////////////////////////////////////
// PARTICLE SHADER

void ParticleProcessMaterialDataGLES2::update_uniform_values(const HashMap<StringName, Variant> &p_parameters, bool p_uniform_dirty, const HashMap<StringName, ShaderLanguage::ShaderNode::Uniform> &p_uniforms, const uint32_t *p_uniform_offsets, uint32_t p_ubo_size) {
	/* We don't have ubos
	if ((uint32_t)ubo_data.size() != p_ubo_size) {
		p_uniform_dirty = true;
		if (!uniform_buffer) {
			glGenBuffers(1, &uniform_buffer);
		}

		ubo_data.resize(p_ubo_size);
		if (ubo_data.size()) {
			memset(ubo_data.ptrw(), 0, ubo_data.size()); //clear
		}
	}

	//check whether buffer changed
	if (p_uniform_dirty && ubo_data.size()) {
		update_uniform_buffer(p_uniforms, p_uniform_offsets, p_parameters, ubo_data.ptrw(), ubo_data.size());
		glBindBuffer(GL_UNIFORM_BUFFER, uniform_buffer);
		glBufferData(GL_UNIFORM_BUFFER, ubo_data.size(), ubo_data.ptrw(), GL_DYNAMIC_DRAW);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}
	*/
	uniform_values = p_parameters;
};

void ParticleProcessMaterialDataGLES2::bind_uniforms() {
	// Bind Material Uniforms
	// TODO: set uniforms based on uniform_values

	bind_uniforms_generic(texture_cache, shader_data->texture_uniforms);
}

GLES::MaterialData *GLES::_create_particles_material_func(GLES::ShaderData *p_shader) {
	GLES::ParticleProcessMaterialData *material_data = memnew(ParticleProcessMaterialDataGLES2);
	material_data->shader_data = static_cast<GLES::ParticlesShaderData *>(p_shader);
	//update will happen later anyway so do nothing.
	return material_data;
}

MaterialStorageGLES2::MaterialStorageGLES2() {
	material_data_request_func[RS::SHADER_SPATIAL] = _create_scene_material_func;
	material_data_request_func[RS::SHADER_CANVAS_ITEM] = _create_canvas_material_func;
	material_data_request_func[RS::SHADER_PARTICLES] = _create_particles_material_func;
	material_data_request_func[RS::SHADER_SKY] = _create_sky_material_func;
	material_data_request_func[RS::SHADER_FOG] = nullptr;

	shaders.canvas_shader = memnew(CanvasShaderGLES2);
	//ShaderGLES *sky_shader = nullptr;
	//ShaderGLES *scene_shader = nullptr;
	//ShaderGLES *cubemap_filter_shader = nullptr;
	//ShaderGLES *particles_process_shader = nullptr;
}

MaterialStorageGLES2::~MaterialStorageGLES2() {
	memdelete(shaders.canvas_shader);
	//memdelete(shaders.sky_shader);
	//memdelete(shaders.scene_shader);
	//memdelete(shaders.cubemap_filter_shader);
	//memdelete(shaders.particles_process_shader);
}

void MaterialStorageGLES2::_update_global_shader_uniforms() {
	//TODO: this will need more work.

	MaterialStorageGLES2 *material_storage = static_cast<MaterialStorageGLES2 *>(MaterialStorageGLES2::get_singleton());
	if (global_shader_uniforms.buffer_dirty_region_count > 0) {
		uint32_t total_regions = global_shader_uniforms.buffer_size / GLES::GlobalShaderUniforms::BUFFER_DIRTY_REGION_SIZE;
		memset(global_shader_uniforms.buffer_dirty_regions, 0, sizeof(bool) * total_regions);
		global_shader_uniforms.buffer_dirty_region_count = 0;

		//TODO: figure out a good way to update global uniforms

		/*
			uint32_t total_regions = global_shader_uniforms.buffer_size / GlobalShaderUniforms::BUFFER_DIRTY_REGION_SIZE;
			if (total_regions / global_shader_uniforms.buffer_dirty_region_count <= 4) {
				// 25% of regions dirty, just update all buffer
				glBindBuffer(GL_UNIFORM_BUFFER, global_shader_uniforms.buffer);
				glBufferData(GL_UNIFORM_BUFFER, sizeof(GlobalShaderUniforms::Value) * global_shader_uniforms.buffer_size, global_shader_uniforms.buffer_values, GL_DYNAMIC_DRAW);
				glBindBuffer(GL_UNIFORM_BUFFER, 0);
				memset(global_shader_uniforms.buffer_dirty_regions, 0, sizeof(bool) * total_regions);
			} else {
				uint32_t region_byte_size = sizeof(GlobalShaderUniforms::Value) * GlobalShaderUniforms::BUFFER_DIRTY_REGION_SIZE;
				glBindBuffer(GL_UNIFORM_BUFFER, global_shader_uniforms.buffer);
				for (uint32_t i = 0; i < total_regions; i++) {
					if (global_shader_uniforms.buffer_dirty_regions[i]) {
						glBufferSubData(GL_UNIFORM_BUFFER, i * region_byte_size, region_byte_size, &global_shader_uniforms.buffer_values[i * GlobalShaderUniforms::BUFFER_DIRTY_REGION_SIZE]);
						global_shader_uniforms.buffer_dirty_regions[i] = false;
					}
				}
				glBindBuffer(GL_UNIFORM_BUFFER, 0);
			}



			global_shader_uniforms.buffer_dirty_region_count = 0;

		*/
	}

	if (global_shader_uniforms.must_update_buffer_materials) {
		// only happens in the case of a buffer variable added or removed,
		// so not often.
		for (const RID &E : global_shader_uniforms.materials_using_buffer) {
			GLES::Material *material = material_storage->get_material(E);
			ERR_CONTINUE(!material); //wtf

			material_storage->_material_queue_update(material, true, false);
		}

		global_shader_uniforms.must_update_buffer_materials = false;
	}

	if (global_shader_uniforms.must_update_texture_materials) {
		// only happens in the case of a buffer variable added or removed,
		// so not often.
		for (const RID &E : global_shader_uniforms.materials_using_texture) {
			GLES::Material *material = material_storage->get_material(E);
			ERR_CONTINUE(!material); //wtf

			material_storage->_material_queue_update(material, false, true);
		}

		global_shader_uniforms.must_update_texture_materials = false;
	}
}

#endif // GLES2_ENABLED
