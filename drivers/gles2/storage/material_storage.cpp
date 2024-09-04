/**************************************************************************/
/*  material_storage.cpp                                                  */
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

#include "material_storage.h"

#ifdef GLES2_ENABLED

#include "core/config/project_settings.h"
#include "drivers/gles2/rasterizer_canvas_gles2.h"
#include "drivers/gles2/rasterizer_gles2.h"
#include "servers/rendering/storage/variant_converters.h"

using namespace GLES2;

MaterialStorage *MaterialStorage::singleton = nullptr;

MaterialStorage *MaterialStorage::get_singleton() {
    return singleton;
}

MaterialStorage::MaterialStorage() {
    singleton = this;

    shader_data_request_func[RS::SHADER_SPATIAL] = _create_scene_shader_func;
    shader_data_request_func[RS::SHADER_CANVAS_ITEM] = _create_canvas_shader_func;
    shader_data_request_func[RS::SHADER_PARTICLES] = _create_particles_shader_func;
    shader_data_request_func[RS::SHADER_SKY] = _create_sky_shader_func;
    shader_data_request_func[RS::SHADER_FOG] = nullptr;

    material_data_request_func[RS::SHADER_SPATIAL] = _create_scene_material_func;
    material_data_request_func[RS::SHADER_CANVAS_ITEM] = _create_canvas_material_func;
    material_data_request_func[RS::SHADER_PARTICLES] = _create_particles_material_func;
    material_data_request_func[RS::SHADER_SKY] = _create_sky_material_func;
    material_data_request_func[RS::SHADER_FOG] = nullptr;

    static_assert(sizeof(GlobalShaderUniforms::Value) == 16);

    global_shader_uniforms.buffer_size = MAX(16, (int)GLOBAL_GET("rendering/limits/global_shader_variables/buffer_size"));
    if (global_shader_uniforms.buffer_size > 128) {
        // Limit to 128 due to GLES2 limitations
        global_shader_uniforms.buffer_size = 128;
    }

    global_shader_uniforms.buffer_values = memnew_arr(GlobalShaderUniforms::Value, global_shader_uniforms.buffer_size);
    memset(global_shader_uniforms.buffer_values, 0, sizeof(GlobalShaderUniforms::Value) * global_shader_uniforms.buffer_size);
    global_shader_uniforms.buffer_usage = memnew_arr(GlobalShaderUniforms::ValueUsage, global_shader_uniforms.buffer_size);
    global_shader_uniforms.buffer_dirty_regions = memnew_arr(bool, 1 + (global_shader_uniforms.buffer_size / GlobalShaderUniforms::BUFFER_DIRTY_REGION_SIZE));
    memset(global_shader_uniforms.buffer_dirty_regions, 0, sizeof(bool) * (1 + (global_shader_uniforms.buffer_size / GlobalShaderUniforms::BUFFER_DIRTY_REGION_SIZE)));
    glGenBuffers(1, &global_shader_uniforms.buffer);
    glBindBuffer(GL_UNIFORM_BUFFER, global_shader_uniforms.buffer);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(GlobalShaderUniforms::Value) * global_shader_uniforms.buffer_size, nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    {
		// Setup CanvasItem compiler
        ShaderCompiler::DefaultIdentifierActions actions;

        actions.renames["VERTEX"] = "vertex";
        actions.renames["UV"] = "uv";
        actions.renames["COLOR"] = "color";

        actions.renames["POINT_SIZE"] = "point_size";

        actions.renames["WORLD_MATRIX"] = "world_matrix";
        actions.renames["PROJECTION_MATRIX"] = "projection_matrix";
        actions.renames["EXTRA_MATRIX"] = "extra_matrix";
        actions.renames["TIME"] = "time";
        actions.renames["AT_LIGHT_PASS"] = "at_light_pass";
        actions.renames["INSTANCE_CUSTOM"] = "instance_custom";

        actions.renames["TEXTURE"] = "color_texture";
        actions.renames["NORMAL_TEXTURE"] = "normal_texture";
        actions.renames["SCREEN_UV"] = "screen_uv";
        actions.renames["SCREEN_TEXTURE"] = "screen_texture";
        actions.renames["FRAGCOORD"] = "gl_FragCoord";
        actions.renames["POINT_COORD"] = "gl_PointCoord";

        actions.usage_defines["COLOR"] = "#define COLOR_USED\n";
        actions.usage_defines["SCREEN_TEXTURE"] = "#define SCREEN_TEXTURE_USED\n";
        actions.usage_defines["SCREEN_UV"] = "#define SCREEN_UV_USED\n";
        actions.usage_defines["NORMAL"] = "#define NORMAL_USED\n";
        actions.usage_defines["NORMAL_TEXTURE"] = "#define NORMAL_TEXTURE_USED\n";

        actions.render_mode_defines["blend_mix"] = "#define BLEND_MIX\n";
        actions.render_mode_defines["blend_add"] = "#define BLEND_ADD\n";
        actions.render_mode_defines["blend_sub"] = "#define BLEND_SUB\n";
        actions.render_mode_defines["blend_mul"] = "#define BLEND_MUL\n";
        actions.render_mode_defines["blend_premul_alpha"] = "#define BLEND_PREMUL_ALPHA\n";
        actions.render_mode_defines["blend_disabled"] = "#define BLEND_DISABLED\n";
        actions.render_mode_defines["unshaded"] = "#define UNSHADED\n";

        actions.global_buffer_array_variable = "global_shader_uniforms";

        shaders.compiler_canvas.initialize(actions);
    }

	{
		// Setup Scene compiler

		//shader compiler
		ShaderCompiler::DefaultIdentifierActions actions;

		actions.renames["MODEL_MATRIX"] = "model_matrix";
		actions.renames["MODEL_NORMAL_MATRIX"] = "model_normal_matrix";
		actions.renames["VIEW_MATRIX"] = "scene_data.view_matrix";
		actions.renames["INV_VIEW_MATRIX"] = "scene_data.inv_view_matrix";
		actions.renames["PROJECTION_MATRIX"] = "projection_matrix";
		actions.renames["INV_PROJECTION_MATRIX"] = "inv_projection_matrix";
		actions.renames["MODELVIEW_MATRIX"] = "modelview";
		actions.renames["MODELVIEW_NORMAL_MATRIX"] = "modelview_normal";
		actions.renames["MAIN_CAM_INV_VIEW_MATRIX"] = "scene_data.main_cam_inv_view_matrix";

		actions.renames["VERTEX"] = "vertex";
		actions.renames["NORMAL"] = "normal";
		actions.renames["TANGENT"] = "tangent";
		actions.renames["BINORMAL"] = "binormal";
		actions.renames["POSITION"] = "position";
		actions.renames["UV"] = "uv_interp";
		actions.renames["UV2"] = "uv2_interp";
		actions.renames["COLOR"] = "color_interp";
		actions.renames["POINT_SIZE"] = "point_size";
		actions.renames["INSTANCE_ID"] = "gl_InstanceID";
		actions.renames["VERTEX_ID"] = "gl_VertexID";

		actions.renames["ALPHA_SCISSOR_THRESHOLD"] = "alpha_scissor_threshold";
		actions.renames["ALPHA_HASH_SCALE"] = "alpha_hash_scale";
		actions.renames["ALPHA_ANTIALIASING_EDGE"] = "alpha_antialiasing_edge";
		actions.renames["ALPHA_TEXTURE_COORDINATE"] = "alpha_texture_coordinate";

		//builtins

		actions.renames["TIME"] = "scene_data.time";
		actions.renames["EXPOSURE"] = "(1.0 / scene_data.emissive_exposure_normalization)";
		actions.renames["PI"] = _MKSTR(Math_PI);
		actions.renames["TAU"] = _MKSTR(Math_TAU);
		actions.renames["E"] = _MKSTR(Math_E);
		actions.renames["VIEWPORT_SIZE"] = "scene_data.viewport_size";

		actions.renames["FRAGCOORD"] = "gl_FragCoord";
		actions.renames["FRONT_FACING"] = "gl_FrontFacing";
		actions.renames["NORMAL_MAP"] = "normal_map";
		actions.renames["NORMAL_MAP_DEPTH"] = "normal_map_depth";
		actions.renames["ALBEDO"] = "albedo";
		actions.renames["ALPHA"] = "alpha";
		actions.renames["PREMUL_ALPHA_FACTOR"] = "premul_alpha";
		actions.renames["METALLIC"] = "metallic";
		actions.renames["SPECULAR"] = "specular";
		actions.renames["ROUGHNESS"] = "roughness";
		actions.renames["RIM"] = "rim";
		actions.renames["RIM_TINT"] = "rim_tint";
		actions.renames["CLEARCOAT"] = "clearcoat";
		actions.renames["CLEARCOAT_ROUGHNESS"] = "clearcoat_roughness";
		actions.renames["ANISOTROPY"] = "anisotropy";
		actions.renames["ANISOTROPY_FLOW"] = "anisotropy_flow";
		actions.renames["SSS_STRENGTH"] = "sss_strength";
		actions.renames["SSS_TRANSMITTANCE_COLOR"] = "transmittance_color";
		actions.renames["SSS_TRANSMITTANCE_DEPTH"] = "transmittance_depth";
		actions.renames["SSS_TRANSMITTANCE_BOOST"] = "transmittance_boost";
		actions.renames["BACKLIGHT"] = "backlight";
		actions.renames["AO"] = "ao";
		actions.renames["AO_LIGHT_AFFECT"] = "ao_light_affect";
		actions.renames["EMISSION"] = "emission";
		actions.renames["POINT_COORD"] = "gl_PointCoord";
		actions.renames["INSTANCE_CUSTOM"] = "instance_custom";
		actions.renames["SCREEN_UV"] = "screen_uv";
		actions.renames["DEPTH"] = "gl_FragDepth";
		actions.renames["FOG"] = "fog";
		actions.renames["RADIANCE"] = "custom_radiance";
		actions.renames["IRRADIANCE"] = "custom_irradiance";
		actions.renames["BONE_INDICES"] = "bone_attrib";
		actions.renames["BONE_WEIGHTS"] = "weight_attrib";
		actions.renames["CUSTOM0"] = "custom0_attrib";
		actions.renames["CUSTOM1"] = "custom1_attrib";
		actions.renames["CUSTOM2"] = "custom2_attrib";
		actions.renames["CUSTOM3"] = "custom3_attrib";
		actions.renames["OUTPUT_IS_SRGB"] = "SHADER_IS_SRGB";
		actions.renames["CLIP_SPACE_FAR"] = "SHADER_SPACE_FAR";
		actions.renames["LIGHT_VERTEX"] = "light_vertex";

		actions.renames["NODE_POSITION_WORLD"] = "model_matrix[3].xyz";
		actions.renames["CAMERA_POSITION_WORLD"] = "scene_data.inv_view_matrix[3].xyz";
		actions.renames["CAMERA_DIRECTION_WORLD"] = "scene_data.inv_view_matrix[2].xyz";
		actions.renames["CAMERA_VISIBLE_LAYERS"] = "scene_data.camera_visible_layers";
		actions.renames["NODE_POSITION_VIEW"] = "(scene_data.view_matrix * model_matrix)[3].xyz";

		actions.renames["VIEW_INDEX"] = "ViewIndex";
		actions.renames["VIEW_MONO_LEFT"] = "uint(0)";
		actions.renames["VIEW_RIGHT"] = "uint(1)";
		actions.renames["EYE_OFFSET"] = "eye_offset";

		//for light
		actions.renames["VIEW"] = "view";
		actions.renames["SPECULAR_AMOUNT"] = "specular_amount";
		actions.renames["LIGHT_COLOR"] = "light_color";
		actions.renames["LIGHT_IS_DIRECTIONAL"] = "is_directional";
		actions.renames["LIGHT"] = "light";
		actions.renames["ATTENUATION"] = "attenuation";
		actions.renames["DIFFUSE_LIGHT"] = "diffuse_light";
		actions.renames["SPECULAR_LIGHT"] = "specular_light";

		actions.usage_defines["NORMAL"] = "#define NORMAL_USED\n";
		actions.usage_defines["TANGENT"] = "#define TANGENT_USED\n";
		actions.usage_defines["BINORMAL"] = "@TANGENT";
		actions.usage_defines["RIM"] = "#define LIGHT_RIM_USED\n";
		actions.usage_defines["RIM_TINT"] = "@RIM";
		actions.usage_defines["CLEARCOAT"] = "#define LIGHT_CLEARCOAT_USED\n";
		actions.usage_defines["CLEARCOAT_ROUGHNESS"] = "@CLEARCOAT";
		actions.usage_defines["ANISOTROPY"] = "#define LIGHT_ANISOTROPY_USED\n";
		actions.usage_defines["ANISOTROPY_FLOW"] = "@ANISOTROPY";
		actions.usage_defines["AO"] = "#define AO_USED\n";
		actions.usage_defines["AO_LIGHT_AFFECT"] = "#define AO_USED\n";
		actions.usage_defines["UV"] = "#define UV_USED\n";
		actions.usage_defines["UV2"] = "#define UV2_USED\n";
		actions.usage_defines["BONE_INDICES"] = "#define BONES_USED\n";
		actions.usage_defines["BONE_WEIGHTS"] = "#define WEIGHTS_USED\n";
		actions.usage_defines["CUSTOM0"] = "#define CUSTOM0_USED\n";
		actions.usage_defines["CUSTOM1"] = "#define CUSTOM1_USED\n";
		actions.usage_defines["CUSTOM2"] = "#define CUSTOM2_USED\n";
		actions.usage_defines["CUSTOM3"] = "#define CUSTOM3_USED\n";
		actions.usage_defines["NORMAL_MAP"] = "#define NORMAL_MAP_USED\n";
		actions.usage_defines["NORMAL_MAP_DEPTH"] = "@NORMAL_MAP";
		actions.usage_defines["COLOR"] = "#define COLOR_USED\n";
		actions.usage_defines["INSTANCE_CUSTOM"] = "#define ENABLE_INSTANCE_CUSTOM\n";
		actions.usage_defines["POSITION"] = "#define OVERRIDE_POSITION\n";
		actions.usage_defines["LIGHT_VERTEX"] = "#define LIGHT_VERTEX_USED\n";

		actions.usage_defines["ALPHA_SCISSOR_THRESHOLD"] = "#define ALPHA_SCISSOR_USED\n";
		actions.usage_defines["ALPHA_HASH_SCALE"] = "#define ALPHA_HASH_USED\n";
		actions.usage_defines["ALPHA_ANTIALIASING_EDGE"] = "#define ALPHA_ANTIALIASING_EDGE_USED\n";
		actions.usage_defines["ALPHA_TEXTURE_COORDINATE"] = "@ALPHA_ANTIALIASING_EDGE";
		actions.usage_defines["PREMULT_ALPHA_FACTOR"] = "#define PREMULT_ALPHA_USED";

		actions.usage_defines["SSS_STRENGTH"] = "#define ENABLE_SSS\n";
		actions.usage_defines["SSS_TRANSMITTANCE_DEPTH"] = "#define ENABLE_TRANSMITTANCE\n";
		actions.usage_defines["BACKLIGHT"] = "#define LIGHT_BACKLIGHT_USED\n";
		actions.usage_defines["SCREEN_UV"] = "#define SCREEN_UV_USED\n";

		actions.usage_defines["FOG"] = "#define CUSTOM_FOG_USED\n";
		actions.usage_defines["RADIANCE"] = "#define CUSTOM_RADIANCE_USED\n";
		actions.usage_defines["IRRADIANCE"] = "#define CUSTOM_IRRADIANCE_USED\n";

		actions.render_mode_defines["skip_vertex_transform"] = "#define SKIP_TRANSFORM_USED\n";
		actions.render_mode_defines["world_vertex_coords"] = "#define VERTEX_WORLD_COORDS_USED\n";
		actions.render_mode_defines["ensure_correct_normals"] = "#define ENSURE_CORRECT_NORMALS\n";
		actions.render_mode_defines["cull_front"] = "#define DO_SIDE_CHECK\n";
		actions.render_mode_defines["cull_disabled"] = "#define DO_SIDE_CHECK\n";
		actions.render_mode_defines["particle_trails"] = "#define USE_PARTICLE_TRAILS\n";
		actions.render_mode_defines["depth_prepass_alpha"] = "#define USE_OPAQUE_PREPASS\n";

		bool force_lambert = GLOBAL_GET("rendering/shading/overrides/force_lambert_over_burley");

		if (!force_lambert) {
			actions.render_mode_defines["diffuse_burley"] = "#define DIFFUSE_BURLEY\n";
		}

		actions.render_mode_defines["diffuse_lambert_wrap"] = "#define DIFFUSE_LAMBERT_WRAP\n";
		actions.render_mode_defines["diffuse_toon"] = "#define DIFFUSE_TOON\n";

		actions.render_mode_defines["sss_mode_skin"] = "#define SSS_MODE_SKIN\n";

		actions.render_mode_defines["specular_schlick_ggx"] = "#define SPECULAR_SCHLICK_GGX\n";
		actions.render_mode_defines["specular_toon"] = "#define SPECULAR_TOON\n";
		actions.render_mode_defines["specular_disabled"] = "#define SPECULAR_DISABLED\n";
		actions.render_mode_defines["shadows_disabled"] = "#define SHADOWS_DISABLED\n";
		actions.render_mode_defines["ambient_light_disabled"] = "#define AMBIENT_LIGHT_DISABLED\n";
		actions.render_mode_defines["shadow_to_opacity"] = "#define USE_SHADOW_TO_OPACITY\n";
		actions.render_mode_defines["unshaded"] = "#define MODE_UNSHADED\n";
		actions.render_mode_defines["fog_disabled"] = "#define FOG_DISABLED\n";

		actions.default_filter = ShaderLanguage::FILTER_LINEAR_MIPMAP;
		actions.default_repeat = ShaderLanguage::REPEAT_ENABLE;

		actions.check_multiview_samplers = RasterizerGLES3::get_singleton()->is_xr_enabled();
		actions.global_buffer_array_variable = "global_shader_uniforms";

		shaders.compiler_scene.initialize(actions);
	}

	{
		// Setup Particles compiler

		ShaderCompiler::DefaultIdentifierActions actions;

		actions.renames["COLOR"] = "out_color";
		actions.renames["VELOCITY"] = "out_velocity_flags.xyz";
		//actions.renames["MASS"] = "mass"; ?
		actions.renames["ACTIVE"] = "particle_active";
		actions.renames["RESTART"] = "restart";
		actions.renames["CUSTOM"] = "out_custom";
		for (int i = 0; i < PARTICLES_MAX_USERDATAS; i++) {
			String udname = "USERDATA" + itos(i + 1);
			actions.renames[udname] = "out_userdata" + itos(i + 1);
			actions.usage_defines[udname] = "#define USERDATA" + itos(i + 1) + "_USED\n";
		}
		actions.renames["TRANSFORM"] = "xform";
		actions.renames["TIME"] = "time";
		actions.renames["PI"] = _MKSTR(Math_PI);
		actions.renames["TAU"] = _MKSTR(Math_TAU);
		actions.renames["E"] = _MKSTR(Math_E);
		actions.renames["LIFETIME"] = "lifetime";
		actions.renames["DELTA"] = "local_delta";
		actions.renames["NUMBER"] = "particle_number";
		actions.renames["INDEX"] = "index";
		actions.renames["AMOUNT_RATIO"] = "amount_ratio";
		//actions.renames["GRAVITY"] = "current_gravity";
		actions.renames["EMISSION_TRANSFORM"] = "emission_transform";
		actions.renames["RANDOM_SEED"] = "random_seed";
		actions.renames["RESTART_POSITION"] = "restart_position";
		actions.renames["RESTART_ROT_SCALE"] = "restart_rotation_scale";
		actions.renames["RESTART_VELOCITY"] = "restart_velocity";
		actions.renames["RESTART_COLOR"] = "restart_color";
		actions.renames["RESTART_CUSTOM"] = "restart_custom";
		actions.renames["COLLIDED"] = "collided";
		actions.renames["COLLISION_NORMAL"] = "collision_normal";
		actions.renames["COLLISION_DEPTH"] = "collision_depth";
		actions.renames["ATTRACTOR_FORCE"] = "attractor_force";
		actions.renames["EMITTER_VELOCITY"] = "emitter_velocity";
		actions.renames["INTERPOLATE_TO_END"] = "interp_to_end";

		// These are unsupported, but may be used by users. To avoid compile time overhead, we add the stub only when used.
		actions.renames["FLAG_EMIT_POSITION"] = "uint(1)";
		actions.renames["FLAG_EMIT_ROT_SCALE"] = "uint(2)";
		actions.renames["FLAG_EMIT_VELOCITY"] = "uint(4)";
		actions.renames["FLAG_EMIT_COLOR"] = "uint(8)";
		actions.renames["FLAG_EMIT_CUSTOM"] = "uint(16)";
		actions.renames["emit_subparticle"] = "emit_subparticle";
		actions.usage_defines["emit_subparticle"] = "\nbool emit_subparticle(mat4 p_xform, vec3 p_velocity, vec4 p_color, vec4 p_custom, uint p_flags) {\n\treturn false;\n}\n";

		actions.render_mode_defines["disable_force"] = "#define DISABLE_FORCE\n";
		actions.render_mode_defines["disable_velocity"] = "#define DISABLE_VELOCITY\n";
		actions.render_mode_defines["keep_data"] = "#define ENABLE_KEEP_DATA\n";
		actions.render_mode_defines["collision_use_scale"] = "#define USE_COLLISION_SCALE\n";

		actions.default_filter = ShaderLanguage::FILTER_LINEAR_MIPMAP;
		actions.default_repeat = ShaderLanguage::REPEAT_ENABLE;

		actions.global_buffer_array_variable = "global_shader_uniforms";

		shaders.compiler_particles.initialize(actions);
	}

	{
		// Setup Sky compiler
		ShaderCompiler::DefaultIdentifierActions actions;

		actions.renames["COLOR"] = "color";
		actions.renames["ALPHA"] = "alpha";
		actions.renames["EYEDIR"] = "cube_normal";
		actions.renames["POSITION"] = "position";
		actions.renames["SKY_COORDS"] = "panorama_coords";
		actions.renames["SCREEN_UV"] = "uv";
		actions.renames["TIME"] = "time";
		actions.renames["FRAGCOORD"] = "gl_FragCoord";
		actions.renames["PI"] = _MKSTR(Math_PI);
		actions.renames["TAU"] = _MKSTR(Math_TAU);
		actions.renames["E"] = _MKSTR(Math_E);
		actions.renames["HALF_RES_COLOR"] = "half_res_color";
		actions.renames["QUARTER_RES_COLOR"] = "quarter_res_color";
		actions.renames["RADIANCE"] = "radiance";
		actions.renames["FOG"] = "custom_fog";
		actions.renames["LIGHT0_ENABLED"] = "directional_lights.data[0].enabled";
		actions.renames["LIGHT0_DIRECTION"] = "directional_lights.data[0].direction_energy.xyz";
		actions.renames["LIGHT0_ENERGY"] = "directional_lights.data[0].direction_energy.w";
		actions.renames["LIGHT0_COLOR"] = "directional_lights.data[0].color_size.xyz";
		actions.renames["LIGHT0_SIZE"] = "directional_lights.data[0].color_size.w";
		actions.renames["LIGHT1_ENABLED"] = "directional_lights.data[1].enabled";
		actions.renames["LIGHT1_DIRECTION"] = "directional_lights.data[1].direction_energy.xyz";
		actions.renames["LIGHT1_ENERGY"] = "directional_lights.data[1].direction_energy.w";
		actions.renames["LIGHT1_COLOR"] = "directional_lights.data[1].color_size.xyz";
		actions.renames["LIGHT1_SIZE"] = "directional_lights.data[1].color_size.w";
		actions.renames["LIGHT2_ENABLED"] = "directional_lights.data[2].enabled";
		actions.renames["LIGHT2_DIRECTION"] = "directional_lights.data[2].direction_energy.xyz";
		actions.renames["LIGHT2_ENERGY"] = "directional_lights.data[2].direction_energy.w";
		actions.renames["LIGHT2_COLOR"] = "directional_lights.data[2].color_size.xyz";
		actions.renames["LIGHT2_SIZE"] = "directional_lights.data[2].color_size.w";
		actions.renames["LIGHT3_ENABLED"] = "directional_lights.data[3].enabled";
		actions.renames["LIGHT3_DIRECTION"] = "directional_lights.data[3].direction_energy.xyz";
		actions.renames["LIGHT3_ENERGY"] = "directional_lights.data[3].direction_energy.w";
		actions.renames["LIGHT3_COLOR"] = "directional_lights.data[3].color_size.xyz";
		actions.renames["LIGHT3_SIZE"] = "directional_lights.data[3].color_size.w";
		actions.renames["AT_CUBEMAP_PASS"] = "AT_CUBEMAP_PASS";
		actions.renames["AT_HALF_RES_PASS"] = "AT_HALF_RES_PASS";
		actions.renames["AT_QUARTER_RES_PASS"] = "AT_QUARTER_RES_PASS";
		actions.usage_defines["HALF_RES_COLOR"] = "\n#define USES_HALF_RES_COLOR\n";
		actions.usage_defines["QUARTER_RES_COLOR"] = "\n#define USES_QUARTER_RES_COLOR\n";
		actions.render_mode_defines["disable_fog"] = "#define DISABLE_FOG\n";
		actions.render_mode_defines["use_debanding"] = "#define USE_DEBANDING\n";

		actions.default_filter = ShaderLanguage::FILTER_LINEAR_MIPMAP;
		actions.default_repeat = ShaderLanguage::REPEAT_ENABLE;

		actions.global_buffer_array_variable = "global_shader_uniforms";

		shaders.compiler_sky.initialize(actions);
	}
}

// Global Shader Uniform functions

void MaterialStorage::_global_shader_uniform_store_in_buffer(int32_t p_index, RS::GlobalShaderParameterType p_type, const Variant &p_value) {
    uint8_t *buffer = (uint8_t *)global_shader_uniforms.buffer_values;

    switch (p_type) {
        case RS::GLOBAL_VAR_TYPE_BOOL: {
            bool value = p_value;
            encode_uint32(value ? 1 : 0, &buffer[p_index * 16]);
        } break;
        case RS::GLOBAL_VAR_TYPE_BVEC2: {
            uint32_t value = p_value;
            encode_uint32(value, &buffer[p_index * 16]);
        } break;
        case RS::GLOBAL_VAR_TYPE_BVEC3: {
            uint32_t value = p_value;
            encode_uint32(value, &buffer[p_index * 16]);
        } break;
        case RS::GLOBAL_VAR_TYPE_BVEC4: {
            uint32_t value = p_value;
            encode_uint32(value, &buffer[p_index * 16]);
        } break;
        case RS::GLOBAL_VAR_TYPE_INT: {
            int32_t value = p_value;
            encode_uint32(value, &buffer[p_index * 16]);
        } break;
        case RS::GLOBAL_VAR_TYPE_IVEC2: {
            Vector2i value = p_value;
            encode_uint32(value.x, &buffer[p_index * 16 + 0]);
            encode_uint32(value.y, &buffer[p_index * 16 + 4]);
        } break;
        case RS::GLOBAL_VAR_TYPE_IVEC3: {
            Vector3i value = p_value;
            encode_uint32(value.x, &buffer[p_index * 16 + 0]);
            encode_uint32(value.y, &buffer[p_index * 16 + 4]);
            encode_uint32(value.z, &buffer[p_index * 16 + 8]);
        } break;
        case RS::GLOBAL_VAR_TYPE_IVEC4: {
            Vector4i value = p_value;
            encode_uint32(value.x, &buffer[p_index * 16 + 0]);
            encode_uint32(value.y, &buffer[p_index * 16 + 4]);
            encode_uint32(value.z, &buffer[p_index * 16 + 8]);
            encode_uint32(value.w, &buffer[p_index * 16 + 12]);
        } break;
        case RS::GLOBAL_VAR_TYPE_RECT2I: {
            Rect2i value = p_value;
            encode_uint32(value.position.x, &buffer[p_index * 16 + 0]);
            encode_uint32(value.position.y, &buffer[p_index * 16 + 4]);
            encode_uint32(value.size.x, &buffer[p_index * 16 + 8]);
            encode_uint32(value.size.y, &buffer[p_index * 16 + 12]);
        } break;
        case RS::GLOBAL_VAR_TYPE_FLOAT: {
            float value = p_value;
            encode_float(value, &buffer[p_index * 16]);
        } break;
        case RS::GLOBAL_VAR_TYPE_VEC2: {
            Vector2 value = p_value;
            encode_float(value.x, &buffer[p_index * 16 + 0]);
            encode_float(value.y, &buffer[p_index * 16 + 4]);
        } break;
        case RS::GLOBAL_VAR_TYPE_VEC3: {
            Vector3 value = p_value;
            encode_float(value.x, &buffer[p_index * 16 + 0]);
            encode_float(value.y, &buffer[p_index * 16 + 4]);
            encode_float(value.z, &buffer[p_index * 16 + 8]);
        } break;
        case RS::GLOBAL_VAR_TYPE_VEC4: {
            Vector4 value = p_value;
            encode_float(value.x, &buffer[p_index * 16 + 0]);
            encode_float(value.y, &buffer[p_index * 16 + 4]);
            encode_float(value.z, &buffer[p_index * 16 + 8]);
            encode_float(value.w, &buffer[p_index * 16 + 12]);
        } break;
        case RS::GLOBAL_VAR_TYPE_COLOR: {
            Color value = p_value;
            encode_float(value.r, &buffer[p_index * 16 + 0]);
            encode_float(value.g, &buffer[p_index * 16 + 4]);
            encode_float(value.b, &buffer[p_index * 16 + 8]);
            encode_float(value.a, &buffer[p_index * 16 + 12]);
        } break;
        case RS::GLOBAL_VAR_TYPE_RECT2: {
            Rect2 value = p_value;
            encode_float(value.position.x, &buffer[p_index * 16 + 0]);
            encode_float(value.position.y, &buffer[p_index * 16 + 4]);
            encode_float(value.size.x, &buffer[p_index * 16 + 8]);
            encode_float(value.size.y, &buffer[p_index * 16 + 12]);
        } break;
        case RS::GLOBAL_VAR_TYPE_MAT2: {
            Vector<float> value = p_value;
            ERR_FAIL_COND(value.size() != 4);
            for (int i = 0; i < 4; i++) {
                encode_float(value[i], &buffer[p_index * 16 + i * 4]);
            }
        } break;
        case RS::GLOBAL_VAR_TYPE_MAT3: {
            Basis value = p_value;
            encode_float(value.rows[0][0], &buffer[p_index * 16 + 0]);
            encode_float(value.rows[0][1], &buffer[p_index * 16 + 4]);
            encode_float(value.rows[0][2], &buffer[p_index * 16 + 8]);
            encode_float(value.rows[1][0], &buffer[p_index * 16 + 12]);
            encode_float(value.rows[1][1], &buffer[p_index * 16 + 16]);
            encode_float(value.rows[1][2], &buffer[p_index * 16 + 20]);
            encode_float(value.rows[2][0], &buffer[p_index * 16 + 24]);
            encode_float(value.rows[2][1], &buffer[p_index * 16 + 28]);
            encode_float(value.rows[2][2], &buffer[p_index * 16 + 32]);
        } break;
        case RS::GLOBAL_VAR_TYPE_MAT4: {
            Transform3D value = p_value;
            for (int i = 0; i < 4; i++) {
                for (int j = 0; j < 3; j++) {
                    encode_float(value.basis.rows[i][j], &buffer[p_index * 16 + i * 16 + j * 4]);
                }
                encode_float(value.origin[i], &buffer[p_index * 16 + i * 16 + 12]);
            }
        } break;
        case RS::GLOBAL_VAR_TYPE_TRANSFORM_2D: {
            Transform2D value = p_value;
            for (int i = 0; i < 3; i++) {
                for (int j = 0; j < 2; j++) {
                    encode_float(value[i][j], &buffer[p_index * 16 + i * 8 + j * 4]);
                }
            }
        } break;
        default: {
            ERR_FAIL();
        }
    }
}

void MaterialStorage::_global_shader_uniform_mark_buffer_dirty(int32_t p_index, int32_t p_elements) {
    int32_t prev_chunk = -1;

    for (int32_t i = 0; i < p_elements; i++) {
        int32_t chunk = (p_index + i) / GlobalShaderUniforms::BUFFER_DIRTY_REGION_SIZE;
        if (chunk != prev_chunk) {
            if (!global_shader_uniforms.buffer_dirty_regions[chunk]) {
                global_shader_uniforms.buffer_dirty_regions[chunk] = true;
                global_shader_uniforms.buffer_dirty_region_count++;
            }
        }

        prev_chunk = chunk;
    }
}

void MaterialStorage::global_shader_parameter_add(const StringName &p_name, RS::GlobalShaderParameterType p_type, const Variant &p_value) {
    ERR_FAIL_COND(global_shader_uniforms.variables.has(p_name));
    GlobalShaderUniforms::Variable gv;
    gv.type = p_type;
    gv.value = p_value;
    gv.buffer_index = -1;

    if (p_type >= RS::GLOBAL_VAR_TYPE_SAMPLER2D) {
        //is texture
        global_shader_uniforms.must_update_texture_materials = true;
    } else {
        gv.buffer_elements = 1;
        if (p_type == RS::GLOBAL_VAR_TYPE_COLOR || p_type == RS::GLOBAL_VAR_TYPE_MAT2) {
            //color needs to elements to store srgb and linear
            gv.buffer_elements = 2;
        }
        if (p_type == RS::GLOBAL_VAR_TYPE_MAT3 || p_type == RS::GLOBAL_VAR_TYPE_TRANSFORM_2D) {
            //mat3 and transform2d need 3 elements
            gv.buffer_elements = 3;
        }
        if (p_type == RS::GLOBAL_VAR_TYPE_MAT4) {
            //mat4 needs 4 elements
            gv.buffer_elements = 4;
        }

        gv.buffer_index = _global_shader_uniform_allocate(gv.buffer_elements);
        ERR_FAIL_COND_MSG(gv.buffer_index < 0, "Too many global shader uniforms!");
        global_shader_uniforms.buffer_usage[gv.buffer_index].elements = gv.buffer_elements;
        _global_shader_uniform_store_in_buffer(gv.buffer_index, gv.type, gv.value);
        _global_shader_uniform_mark_buffer_dirty(gv.buffer_index, gv.buffer_elements);

        global_shader_uniforms.must_update_buffer_materials = true;
    }

    global_shader_uniforms.variables[p_name] = gv;
}

void MaterialStorage::global_shader_parameter_remove(const StringName &p_name) {
    if (!global_shader_uniforms.variables.has(p_name)) {
        return;
    }
    GlobalShaderUniforms::Variable &gv = global_shader_uniforms.variables[p_name];

    if (gv.buffer_index >= 0) {
        global_shader_uniforms.buffer_usage[gv.buffer_index].elements = 0;
        global_shader_uniforms.must_update_buffer_materials = true;
    } else {
        global_shader_uniforms.must_update_texture_materials = true;
    }

    global_shader_uniforms.variables.erase(p_name);
}

Vector<StringName> MaterialStorage::global_shader_parameter_get_list() const {
    if (!Engine::get_singleton()->is_editor_hint()) {
        ERR_FAIL_V_MSG(Vector<StringName>(), "This function should never be used outside the editor, it can severely damage performance.");
    }

    Vector<StringName> names;
    for (const KeyValue<StringName, GlobalShaderUniforms::Variable> &E : global_shader_uniforms.variables) {
        names.push_back(E.key);
    }
    names.sort_custom<StringName::AlphCompare>();
    return names;
}

void MaterialStorage::global_shader_parameter_set(const StringName &p_name, const Variant &p_value) {
    ERR_FAIL_COND(!global_shader_uniforms.variables.has(p_name));
    GlobalShaderUniforms::Variable &gv = global_shader_uniforms.variables[p_name];
    gv.value = p_value;
    if (gv.override.get_type() == Variant::NIL) {
        if (gv.buffer_index >= 0) {
            //buffer
            _global_shader_uniform_store_in_buffer(gv.buffer_index, gv.type, gv.value);
            _global_shader_uniform_mark_buffer_dirty(gv.buffer_index, gv.buffer_elements);
        } else {
            //texture
            for (const RID &E : gv.texture_materials) {
                Material *material = get_material(E);
                ERR_CONTINUE(!material);
                _material_queue_update(material, false, true);
            }
        }
    }
}

void MaterialStorage::global_shader_parameter_set_override(const StringName &p_name, const Variant &p_value) {
    if (!global_shader_uniforms.variables.has(p_name)) {
        return; //variable may not exist
    }

    ERR_FAIL_COND(p_value.get_type() == Variant::OBJECT);

    GlobalShaderUniforms::Variable &gv = global_shader_uniforms.variables[p_name];

    gv.override = p_value;

    if (gv.buffer_index >= 0) {
        //buffer
        if (gv.override.get_type() == Variant::NIL) {
            _global_shader_uniform_store_in_buffer(gv.buffer_index, gv.type, gv.value);
        } else {
            _global_shader_uniform_store_in_buffer(gv.buffer_index, gv.type, gv.override);
        }

        _global_shader_uniform_mark_buffer_dirty(gv.buffer_index, gv.buffer_elements);
    } else {
        //texture
        for (const RID &E : gv.texture_materials) {
            Material *material = get_material(E);
            ERR_CONTINUE(!material);
            _material_queue_update(material, false, true);
        }
    }
}

Variant MaterialStorage::global_shader_parameter_get(const StringName &p_name) const {
    if (!Engine::get_singleton()->is_editor_hint()) {
        ERR_FAIL_V_MSG(Variant(), "This function should never be used outside the editor, it can severely damage performance.");
    }

    if (!global_shader_uniforms.variables.has(p_name)) {
        return Variant();
    }

    return global_shader_uniforms.variables[p_name].value;
}

RS::GlobalShaderParameterType MaterialStorage::global_shader_parameter_get_type_internal(const StringName &p_name) const {
    if (!global_shader_uniforms.variables.has(p_name)) {
        return RS::GLOBAL_VAR_TYPE_MAX;
    }

    return global_shader_uniforms.variables[p_name].type;
}

RS::GlobalShaderParameterType MaterialStorage::global_shader_parameter_get_type(const StringName &p_name) const {
    if (!Engine::get_singleton()->is_editor_hint()) {
        ERR_FAIL_V_MSG(RS::GLOBAL_VAR_TYPE_MAX, "This function should never be used outside the editor, it can severely damage performance.");
    }

    return global_shader_parameter_get_type_internal(p_name);
}

void MaterialStorage::global_shader_parameters_load_settings(bool p_load_textures) {
    List<PropertyInfo> settings;
    ProjectSettings::get_singleton()->get_property_list(&settings);

    for (const PropertyInfo &E : settings) {
        if (E.name.begins_with("shader_globals/")) {
            StringName name = E.name.get_slice("/", 1);
            Dictionary d = GLOBAL_GET(E.name);

            ERR_CONTINUE(!d.has("type"));
            ERR_CONTINUE(!d.has("value"));

            String type = d["type"];

            static const char *global_var_type_names[RS::GLOBAL_VAR_TYPE_MAX] = {
                "bool",
                "bvec2",
                "bvec3",
                "bvec4",
                "int",
                "ivec2",
                "ivec3",
                "ivec4",
                "rect2i",
                "float",
                "vec2",
                "vec3",
                "vec4",
                "color",
                "rect2",
                "mat2",
                "mat3",
                "mat4",
                "transform_2d",
                "sampler2D",
                "sampler2DArray",
                "sampler3D",
                "samplerCube",
            };

            RS::GlobalShaderParameterType gvtype = RS::GLOBAL_VAR_TYPE_MAX;

            for (int i = 0; i < RS::GLOBAL_VAR_TYPE_MAX; i++) {
                if (global_var_type_names[i] == type) {
                    gvtype = RS::GlobalShaderParameterType(i);
                    break;
                }
            }

            ERR_CONTINUE(gvtype == RS::GLOBAL_VAR_TYPE_MAX); //type invalid

            Variant value = d["value"];

            if (gvtype >= RS::GLOBAL_VAR_TYPE_SAMPLER2D) {
                // GLES2 doesn't support uniform buffer objects, so we can't store textures in the buffer
                // We'll need to handle textures differently in the shader
                continue;
            }

            if (global_shader_uniforms.variables.has(name)) {
                //has it, update it
                global_shader_parameter_set(name, value);
            } else {
                global_shader_parameter_add(name, gvtype, value);
            }
        }
    }
}

void MaterialStorage::global_shader_parameters_clear() {
    global_shader_uniforms.variables.clear();
}

GLuint MaterialStorage::global_shader_parameters_get_uniform_buffer() const {
    return global_shader_uniforms.buffer;
}

int32_t MaterialStorage::global_shader_parameters_instance_allocate(RID p_instance) {
    ERR_FAIL_COND_V(global_shader_uniforms.instance_buffer_pos.has(p_instance), -1);
    int32_t pos = _global_shader_uniform_allocate(1); // Changed to allocate only 1 element for GLES2
    global_shader_uniforms.instance_buffer_pos[p_instance] = pos;
    ERR_FAIL_COND_V_MSG(pos < 0, -1, "Too many instances using shader instance variables!");
    global_shader_uniforms.buffer_usage[pos].elements = 1; // Changed to 1 element for GLES2
    return pos;
}

void MaterialStorage::global_shader_parameters_instance_free(RID p_instance) {
    ERR_FAIL_COND(!global_shader_uniforms.instance_buffer_pos.has(p_instance));
    int32_t pos = global_shader_uniforms.instance_buffer_pos[p_instance];
    if (pos >= 0) {
        global_shader_uniforms.buffer_usage[pos].elements = 0;
    }
    global_shader_uniforms.instance_buffer_pos.erase(p_instance);
}

void MaterialStorage::global_shader_parameters_instance_update(RID p_instance, int p_index, const Variant &p_value, int p_flags_count) {
    if (!global_shader_uniforms.instance_buffer_pos.has(p_instance)) {
        return; //just not allocated, ignore
    }
    int32_t pos = global_shader_uniforms.instance_buffer_pos[p_instance];

    if (pos < 0) {
        return; //again, not allocated, ignore
    }

    Variant::Type value_type = p_value.get_type();
    ERR_FAIL_COND_MSG(p_value.get_type() > Variant::COLOR, "Unsupported variant type for instance parameter: " + Variant::get_type_name(value_type)); //anything greater not supported

    // For GLES2, we'll need to handle this differently since we can't use UBOs
    // We might need to use individual uniforms for each instance parameter
    // This is a placeholder and needs to be implemented based on how you want to handle instance parameters in GLES2
    ERR_PRINT("Instance parameters are not fully supported in GLES2 yet.");
}

void MaterialStorage::_update_global_shader_uniforms() {
    MaterialStorage *material_storage = MaterialStorage::get_singleton();
    if (global_shader_uniforms.buffer_dirty_region_count > 0) {
        uint32_t total_regions = 1 + (global_shader_uniforms.buffer_size / GlobalShaderUniforms::BUFFER_DIRTY_REGION_SIZE);
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
    }

    if (global_shader_uniforms.must_update_buffer_materials) {
        // Update materials using the global shader uniforms buffer
        for (const RID &E : global_shader_uniforms.materials_using_buffer) {
            Material *material = material_storage->get_material(E);
            ERR_CONTINUE(!material);
            _material_queue_update(material, true, false);
        }

        global_shader_uniforms.must_update_buffer_materials = false;
    }

    if (global_shader_uniforms.must_update_texture_materials) {
        // Update materials using global shader uniforms textures
        for (const RID &E : global_shader_uniforms.materials_using_texture) {
            Material *material = material_storage->get_material(E);
            ERR_CONTINUE(!material);
            _material_queue_update(material, false, true);
        }

        global_shader_uniforms.must_update_texture_materials = false;
    }
}

/* SHADER API */

RID MaterialStorage::shader_allocate() {
    return shader_owner.allocate_rid();
}

void MaterialStorage::shader_initialize(RID p_rid) {
    Shader shader;
    shader.data = nullptr;
    shader.mode = RS::SHADER_MAX;

    shader_owner.initialize_rid(p_rid, shader);
}

void MaterialStorage::shader_free(RID p_rid) {
    GLES2::Shader *shader = shader_owner.get_or_null(p_rid);
    ERR_FAIL_NULL(shader);

    //make material unreference this
    while (shader->owners.size()) {
        material_set_shader((*shader->owners.begin())->self, RID());
    }

    //clear data if exists
    if (shader->data) {
        memdelete(shader->data);
    }
    shader_owner.free(p_rid);
}

void MaterialStorage::shader_set_code(RID p_shader, const String &p_code) {
    GLES2::Shader *shader = shader_owner.get_or_null(p_shader);
    ERR_FAIL_NULL(shader);

    shader->code = p_code;

    String mode_string = ShaderLanguage::get_shader_type(p_code);

    RS::ShaderMode new_mode;
    if (mode_string == "canvas_item") {
        new_mode = RS::SHADER_CANVAS_ITEM;
    } else if (mode_string == "particles") {
        new_mode = RS::SHADER_PARTICLES;
    } else if (mode_string == "spatial") {
        new_mode = RS::SHADER_SPATIAL;
    } else if (mode_string == "sky") {
        new_mode = RS::SHADER_SKY;
    } else {
        new_mode = RS::SHADER_MAX;
        ERR_PRINT("shader type " + mode_string + " not supported in OpenGL ES 2.0 renderer");
    }

    if (new_mode != shader->mode) {
        if (shader->data) {
            memdelete(shader->data);
            shader->data = nullptr;
        }

        for (Material *E : shader->owners) {
            Material *material = E;
            material->shader_mode = new_mode;
            if (material->data) {
                memdelete(material->data);
                material->data = nullptr;
            }
        }

        shader->mode = new_mode;

        if (new_mode < RS::SHADER_MAX && shader_data_request_func[new_mode]) {
            shader->data = shader_data_request_func[new_mode]();
        } else {
            shader->mode = RS::SHADER_MAX;
        }

        for (Material *E : shader->owners) {
            Material *material = E;
            if (shader->data) {
                material->data = material_data_request_func[new_mode](shader->data);
                material->data->self = material->self;
                material->data->set_next_pass(material->next_pass);
                material->data->set_render_priority(material->priority);
            }
            material->shader_mode = new_mode;
        }

        if (shader->data) {
            for (const KeyValue<StringName, HashMap<int, RID>> &E : shader->default_texture_parameter) {
                for (const KeyValue<int, RID> &E2 : E.value) {
                    shader->data->set_default_texture_parameter(E.key, E2.value, E2.key);
                }
            }
        }
    }

    if (shader->data) {
        shader->data->set_code(p_code);
    }

    for (Material *E : shader->owners) {
        Material *material = E;
        material->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_MATERIAL);
        _material_queue_update(material, true, true);
    }
}

void MaterialStorage::shader_set_path_hint(RID p_shader, const String &p_path) {
    GLES2::Shader *shader = shader_owner.get_or_null(p_shader);
    ERR_FAIL_NULL(shader);

    shader->path_hint = p_path;
    if (shader->data) {
        shader->data->set_path_hint(p_path);
    }
}

String MaterialStorage::shader_get_code(RID p_shader) const {
    const GLES2::Shader *shader = shader_owner.get_or_null(p_shader);
    ERR_FAIL_NULL_V(shader, String());
    return shader->code;
}

void MaterialStorage::get_shader_parameter_list(RID p_shader, List<PropertyInfo> *p_param_list) const {
    GLES2::Shader *shader = shader_owner.get_or_null(p_shader);
    ERR_FAIL_NULL(shader);
    if (shader->data) {
        return shader->data->get_shader_uniform_list(p_param_list);
    }
}

void MaterialStorage::shader_set_default_texture_parameter(RID p_shader, const StringName &p_name, RID p_texture, int p_index) {
    GLES2::Shader *shader = shader_owner.get_or_null(p_shader);
    ERR_FAIL_NULL(shader);

    if (p_texture.is_valid() && TextureStorage::get_singleton()->owns_texture(p_texture)) {
        if (!shader->default_texture_parameter.has(p_name)) {
            shader->default_texture_parameter[p_name] = HashMap<int, RID>();
        }
        shader->default_texture_parameter[p_name][p_index] = p_texture;
    } else {
        if (shader->default_texture_parameter.has(p_name) && shader->default_texture_parameter[p_name].has(p_index)) {
            shader->default_texture_parameter[p_name].erase(p_index);

            if (shader->default_texture_parameter[p_name].is_empty()) {
				shader->default_texture_parameter.erase(p_name);
            }
        }
    }

	if (shader->data) {
        shader->data->set_default_texture_parameter(p_name, p_texture, p_index);
    }
    for (Material *E : shader->owners) {
        Material *material = E;
        _material_queue_update(material, false, true);
    }
}

RID MaterialStorage::shader_get_default_texture_parameter(RID p_shader, const StringName &p_name, int p_index) const {
    const GLES2::Shader *shader = shader_owner.get_or_null(p_shader);
    ERR_FAIL_NULL_V(shader, RID());
    if (shader->default_texture_parameter.has(p_name) && shader->default_texture_parameter[p_name].has(p_index)) {
        return shader->default_texture_parameter[p_name][p_index];
    }

    return RID();
}

Variant MaterialStorage::shader_get_parameter_default(RID p_shader, const StringName &p_param) const {
    Shader *shader = shader_owner.get_or_null(p_shader);
    ERR_FAIL_NULL_V(shader, Variant());
    if (shader->data) {
        return shader->data->get_default_parameter(p_param);
    }
    return Variant();
}

RS::ShaderNativeSourceCode MaterialStorage::shader_get_native_source_code(RID p_shader) const {
    Shader *shader = shader_owner.get_or_null(p_shader);
    ERR_FAIL_NULL_V(shader, RS::ShaderNativeSourceCode());
    if (shader->data) {
        return shader->data->get_native_source_code();
    }
    return RS::ShaderNativeSourceCode();
}

/* MATERIAL API */

void MaterialStorage::_material_queue_update(GLES2::Material *material, bool p_uniform, bool p_texture) {
    material->uniform_dirty = material->uniform_dirty || p_uniform;
    material->texture_dirty = material->texture_dirty || p_texture;

    if (material->update_element.in_list()) {
        return;
    }

    material_update_list.add(&material->update_element);
}

void MaterialStorage::_update_queued_materials() {
    while (material_update_list.first()) {
        Material *material = material_update_list.first()->self();

        if (material->data) {
            material->data->update_parameters(material->params, material->uniform_dirty, material->texture_dirty);
        }
        material->texture_dirty = false;
        material->uniform_dirty = false;

        material_update_list.remove(&material->update_element);
    }
}

RID MaterialStorage::material_allocate() {
    return material_owner.allocate_rid();
}

void MaterialStorage::material_initialize(RID p_rid) {
    material_owner.initialize_rid(p_rid);
    Material *material = material_owner.get_or_null(p_rid);
    material->self = p_rid;
}

void MaterialStorage::material_free(RID p_rid) {
    Material *material = material_owner.get_or_null(p_rid);
    ERR_FAIL_NULL(material);

    // Need to clear texture arrays to prevent spin locking of their RID's.
    // This happens when the app is being closed.
    for (KeyValue<StringName, Variant> &E : material->params) {
        if (E.value.get_type() == Variant::ARRAY) {
            Array(E.value).clear();
        }
    }

    material_set_shader(p_rid, RID()); //clean up shader
    material->dependency.deleted_notify(p_rid);

    material_owner.free(p_rid);
}

void MaterialStorage::material_set_shader(RID p_material, RID p_shader) {
    GLES2::Material *material = material_owner.get_or_null(p_material);
    ERR_FAIL_NULL(material);

    if (material->data) {
        memdelete(material->data);
        material->data = nullptr;
    }

    if (material->shader) {
        material->shader->owners.erase(material);
        material->shader = nullptr;
        material->shader_mode = RS::SHADER_MAX;
    }

    if (p_shader.is_null()) {
        material->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_MATERIAL);
        material->shader_id = 0;
        return;
    }

    Shader *shader = get_shader(p_shader);
    ERR_FAIL_NULL(shader);
    material->shader = shader;
    material->shader_mode = shader->mode;
    material->shader_id = p_shader.get_local_index();
    shader->owners.insert(material);

    if (shader->mode == RS::SHADER_MAX) {
        return;
    }

    ERR_FAIL_NULL(shader->data);

    material->data = material_data_request_func[shader->mode](shader->data);
    material->data->self = p_material;
    material->data->set_next_pass(material->next_pass);
    material->data->set_render_priority(material->priority);
    //updating happens later
    material->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_MATERIAL);
    _material_queue_update(material, true, true);
}

void MaterialStorage::material_set_param(RID p_material, const StringName &p_param, const Variant &p_value) {
    GLES2::Material *material = material_owner.get_or_null(p_material);
    ERR_FAIL_NULL(material);

    if (p_value.get_type() == Variant::NIL) {
        material->params.erase(p_param);
    } else {
        ERR_FAIL_COND(p_value.get_type() == Variant::OBJECT); //object not allowed
        material->params[p_param] = p_value;
    }

    if (material->shader && material->shader->data) { //shader is valid
        bool is_texture = material->shader->data->is_parameter_texture(p_param);
        _material_queue_update(material, !is_texture, is_texture);
    } else {
        _material_queue_update(material, true, true);
    }
}

Variant MaterialStorage::material_get_param(RID p_material, const StringName &p_param) const {
    const GLES2::Material *material = material_owner.get_or_null(p_material);
    ERR_FAIL_NULL_V(material, Variant());
    if (material->params.has(p_param)) {
        return material->params[p_param];
    } else {
        return Variant();
    }
}

void MaterialStorage::material_set_next_pass(RID p_material, RID p_next_material) {
    GLES2::Material *material = material_owner.get_or_null(p_material);
    ERR_FAIL_NULL(material);

    if (material->next_pass == p_next_material) {
        return;
    }

    material->next_pass = p_next_material;
    if (material->data) {
        material->data->set_next_pass(p_next_material);
    }

    material->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_MATERIAL);
}

void MaterialStorage::material_set_render_priority(RID p_material, int priority) {
    ERR_FAIL_COND(priority < RS::MATERIAL_RENDER_PRIORITY_MIN);
    ERR_FAIL_COND(priority > RS::MATERIAL_RENDER_PRIORITY_MAX);

    GLES2::Material *material = material_owner.get_or_null(p_material);
    ERR_FAIL_NULL(material);
    material->priority = priority;
    if (material->data) {
        material->data->set_render_priority(priority);
    }
    material->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_MATERIAL);
}

bool MaterialStorage::material_is_animated(RID p_material) {
    GLES2::Material *material = material_owner.get_or_null(p_material);
    ERR_FAIL_NULL_V(material, false);
    if (material->shader && material->shader->data) {
        if (material->shader->data->is_animated()) {
            return true;
        } else if (material->next_pass.is_valid()) {
            return material_is_animated(material->next_pass);
        }
    }
    return false; //by default nothing is animated
}

bool MaterialStorage::material_casts_shadows(RID p_material) {
    GLES2::Material *material = material_owner.get_or_null(p_material);
    ERR_FAIL_NULL_V(material, true);
    if (material->shader && material->shader->data) {
        if (material->shader->data->casts_shadows()) {
            return true;
        } else if (material->next_pass.is_valid()) {
            return material_casts_shadows(material->next_pass);
        }
    }
    return true; //by default everything casts shadows
}

void MaterialStorage::material_get_instance_shader_parameters(RID p_material, List<InstanceShaderParam> *r_parameters) {
    GLES2::Material *material = material_owner.get_or_null(p_material);
    ERR_FAIL_NULL(material);
    if (material->shader && material->shader->data) {
        material->shader->data->get_instance_param_list(r_parameters);

        if (material->next_pass.is_valid()) {
            material_get_instance_shader_parameters(material->next_pass, r_parameters);
        }
    }
}

void MaterialStorage::material_update_dependency(RID p_material, DependencyTracker *p_instance) {
    Material *material = material_owner.get_or_null(p_material);
    ERR_FAIL_NULL(material);
    p_instance->update_dependency(&material->dependency);
    if (material->next_pass.is_valid()) {
        material_update_dependency(material->next_pass, p_instance);
    }
}

MaterialStorage::~MaterialStorage() {

	memdelete_arr(global_shader_uniforms.buffer_values);
	memdelete_arr(global_shader_uniforms.buffer_usage);
	memdelete_arr(global_shader_uniforms.buffer_dirty_regions);
	glDeleteBuffers(1, &global_shader_uniforms.buffer);

    singleton = nullptr;
}

#endif // GLES2_ENABLED
