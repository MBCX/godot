/**************************************************************************/
/*  material_storage.h                                                    */
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

#ifndef MATERIAL_STORAGE_GLES2_H
#define MATERIAL_STORAGE_GLES2_H

#if defined(GLES2_ENABLED)

#include "drivers/gles_common/storage/config.h"
#include "drivers/gles_common/storage/material_storage.h"

namespace GLES {

struct CanvasMaterialDataGLES2 : public GLES::CanvasMaterialData {
protected:
	HashMap<StringName, Variant> uniform_values;

	virtual void update_uniform_values(const HashMap<StringName, Variant> &p_parameters, bool p_uniform_dirty, const HashMap<StringName, ShaderLanguage::ShaderNode::Uniform> &p_uniforms, const uint32_t *p_uniform_offsets, uint32_t p_ubo_size);
	virtual void bind_uniforms();
};
GLES::MaterialData *_create_canvas_material_func(GLES::ShaderData *p_shader);

struct SkyMaterialDataGLES2 : public GLES::SkyMaterialData {
protected:
	HashMap<StringName, Variant> uniform_values;

	virtual void update_uniform_values(const HashMap<StringName, Variant> &p_parameters, bool p_uniform_dirty, const HashMap<StringName, ShaderLanguage::ShaderNode::Uniform> &p_uniforms, const uint32_t *p_uniform_offsets, uint32_t p_ubo_size);
	virtual void bind_uniforms();
};
GLES::MaterialData *_create_sky_material_func(GLES::ShaderData *p_shader);

struct SceneMaterialDataGLES2 : public GLES::SceneMaterialData {
protected:
	HashMap<StringName, Variant> uniform_values;

	virtual void update_uniform_values(const HashMap<StringName, Variant> &p_parameters, bool p_uniform_dirty, const HashMap<StringName, ShaderLanguage::ShaderNode::Uniform> &p_uniforms, const uint32_t *p_uniform_offsets, uint32_t p_ubo_size);
	virtual void bind_uniforms();
};
GLES::MaterialData *_create_scene_material_func(GLES::ShaderData *p_shader);

struct ParticleProcessMaterialDataGLES2 : public GLES::ParticleProcessMaterialData {
protected:
	HashMap<StringName, Variant> uniform_values;

	virtual void update_uniform_values(const HashMap<StringName, Variant> &p_parameters, bool p_uniform_dirty, const HashMap<StringName, ShaderLanguage::ShaderNode::Uniform> &p_uniforms, const uint32_t *p_uniform_offsets, uint32_t p_ubo_size);
	virtual void bind_uniforms();
};
GLES::MaterialData *_create_particles_material_func(GLES::ShaderData *p_shader);

class MaterialStorageGLES2 : public GLES::MaterialStorage {
private:
	//no need to setup a global ubo when we don't have ubos...
	virtual void _gen_global_uniforms() {}

public:
	MaterialStorageGLES2();
	~MaterialStorageGLES2();

	virtual void _update_global_shader_uniforms();
};

} //namespace GLES

#endif // GLES2_ENABLED

#endif // MATERIAL_STORAGE_GLES2_H
