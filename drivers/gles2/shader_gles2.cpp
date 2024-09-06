/**************************************************************************/
/*  shader_gles2.cpp                                                      */
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

#include "shader_gles2.h"

#ifdef GLES2_ENABLED

#include "core/io/compression.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"

#include "drivers/gles2/rasterizer_gles2.h"
#include "drivers/gles2/storage/config.h"

static String _mkid(const String &p_id) {
    String id = "m_" + p_id.replace("__", "_dus_");
    return id.replace("__", "_dus_"); //doubleunderscore is reserved in glsl
}

void ShaderGLES2::_add_stage(const char *p_code, StageType p_stage_type) {
    Vector<String> lines = String(p_code).split("\n");

    String text;

    for (int i = 0; i < lines.size(); i++) {
        const String &l = lines[i];
        bool push_chunk = false;

        StageTemplate::Chunk chunk;

        if (l.begins_with("#GLOBALS")) {
            switch (p_stage_type) {
                case STAGE_TYPE_VERTEX:
                    chunk.type = StageTemplate::Chunk::TYPE_VERTEX_GLOBALS;
                    break;
                case STAGE_TYPE_FRAGMENT:
                    chunk.type = StageTemplate::Chunk::TYPE_FRAGMENT_GLOBALS;
                    break;
                default: {
                }
            }

            push_chunk = true;
        } else if (l.begins_with("#MATERIAL_UNIFORMS")) {
            chunk.type = StageTemplate::Chunk::TYPE_MATERIAL_UNIFORMS;
            push_chunk = true;
        } else if (l.begins_with("#CODE")) {
            chunk.type = StageTemplate::Chunk::TYPE_CODE;
            push_chunk = true;
            chunk.code = l.replace_first("#CODE", String()).replace(":", "").strip_edges().to_upper();
        } else {
            text += l + "\n";
        }

        if (push_chunk) {
            if (text != String()) {
                StageTemplate::Chunk text_chunk;
                text_chunk.type = StageTemplate::Chunk::TYPE_TEXT;
                text_chunk.text = text.utf8();
                stage_templates[p_stage_type].chunks.push_back(text_chunk);
                text = String();
            }
            stage_templates[p_stage_type].chunks.push_back(chunk);
        }

        if (text != String()) {
            StageTemplate::Chunk text_chunk;
            text_chunk.type = StageTemplate::Chunk::TYPE_TEXT;
            text_chunk.text = text.utf8();
            stage_templates[p_stage_type].chunks.push_back(text_chunk);
            text = String();
        }
    }
}

void ShaderGLES2::_setup(
	const char *p_vertex_code,
	const char *p_fragment_code,
	const char *p_name,
	int p_uniform_count,
	const char **p_uniform_names,
	int p_ubo_count,
	const UBOPair *p_ubos,
	int p_feedback_count,
	const Feedback *p_feedback,
	int p_texture_count,
	const TexUnitPair *p_tex_units,
	int p_variant_count,
	const char **p_variants
) {
    name = p_name;

    if (p_vertex_code) {
        _add_stage(p_vertex_code, STAGE_TYPE_VERTEX);
    }
    if (p_fragment_code) {
        _add_stage(p_fragment_code, STAGE_TYPE_FRAGMENT);
    }

    uniform_names = p_uniform_names;
    uniform_count = p_uniform_count;
    ubo_pairs = p_ubos;
    ubo_count = p_ubo_count;
    texunit_pairs = p_tex_units;
    texunit_pair_count = p_texture_count;
    variant_defines = p_variants;
    variant_count = p_variant_count;
    feedbacks = p_feedback;
    feedback_count = p_feedback_count;
	vertex_code = p_vertex_code;

	{
		String globals_tag = "\nVERTEX_SHADER_GLOBALS";
		String code_tag = "\nVERTEX_SHADER_CODE";
		String code = vertex_code;
		int cpos = code.find(globals_tag);
		if (cpos == -1) {
			vertex_code0 = code.ascii();
		} else {
			vertex_code0 = code.substr(0, cpos).ascii();
			code = code.substr(cpos + globals_tag.length(), code.length());

			cpos = code.find(code_tag);

			if (cpos == -1) {
				vertex_code1 = code.ascii();
			} else {
				vertex_code1 = code.substr(0, cpos).ascii();
				vertex_code2 = code.substr(cpos + code_tag.length(), code.length()).ascii();
			}
		}
	}

	{
		String globals_tag = "\nFRAGMENT_SHADER_GLOBALS";
		String code_tag = "\nFRAGMENT_SHADER_CODE";
		String light_code_tag = "\nLIGHT_SHADER_CODE";
		String code = fragment_code;
		int cpos = code.find(globals_tag);
		if (cpos == -1) {
			fragment_code0 = code.ascii();
		} else {
			fragment_code0 = code.substr(0, cpos).ascii();
			code = code.substr(cpos + globals_tag.length(), code.length());

			cpos = code.find(light_code_tag);

			String code2;

			if (cpos != -1) {
				fragment_code1 = code.substr(0, cpos).ascii();
				code2 = code.substr(cpos + light_code_tag.length(), code.length());
			} else {
				code2 = code;
			}

			cpos = code2.find(code_tag);
			if (cpos == -1) {
				fragment_code2 = code2.ascii();
			} else {
				fragment_code2 = code2.substr(0, cpos).ascii();
				fragment_code3 = code2.substr(cpos + code_tag.length(), code2.length()).ascii();
			}
		}
	}

	// The upper limit must match the version used in storage.
	// max_image_units = RasterizerStorageGLES2::safe_gl_get_integer(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, RasterizerStorageGLES2::Config::max_desired_texture_image_units);
}

RID ShaderGLES2::version_create() {
    Version version;
    return version_owner.make_rid(version);
}

void ShaderGLES2::_build_variant_code(StringBuilder &builder, uint32_t p_variant, const Version *p_version, StageType p_stage_type) {
    builder.append("#version 100\n"); // GLES2 uses GLSL ES 1.00

    // Add precision qualifiers for GLES2
    builder.append("precision mediump float;\n");
    builder.append("precision mediump int;\n");

    builder.append("\n"); //make sure defines begin at newline
    builder.append(general_defines.get_data());
    builder.append(variant_defines[p_variant]);
    builder.append("\n");
    for (int j = 0; j < p_version->custom_defines.size(); j++) {
        builder.append(p_version->custom_defines[j].get_data());
    }
    builder.append("\n"); //make sure defines begin at newline

    const StageTemplate &stage_template = stage_templates[p_stage_type];
    for (uint32_t i = 0; i < stage_template.chunks.size(); i++) {
        const StageTemplate::Chunk &chunk = stage_template.chunks[i];
        switch (chunk.type) {
            case StageTemplate::Chunk::TYPE_MATERIAL_UNIFORMS: {
                builder.append(p_version->uniforms.get_data()); //uniforms (same for vertex and fragment)
            } break;
            case StageTemplate::Chunk::TYPE_VERTEX_GLOBALS: {
                builder.append(p_version->vertex_globals.get_data()); // vertex globals
            } break;
            case StageTemplate::Chunk::TYPE_FRAGMENT_GLOBALS: {
                builder.append(p_version->fragment_globals.get_data()); // fragment globals
            } break;
            case StageTemplate::Chunk::TYPE_CODE: {
                if (p_version->code_sections.has(chunk.code)) {
                    builder.append(p_version->code_sections[chunk.code].get_data());
                }
            } break;
            case StageTemplate::Chunk::TYPE_TEXT: {
                builder.append(chunk.text.get_data());
            } break;
        }
    }
}

static void _display_error_with_code(const String &p_error, const String &p_code) {
    int line = 1;
    Vector<String> lines = p_code.split("\n");

    for (int j = 0; j < lines.size(); j++) {
        print_line(itos(line) + ": " + lines[j]);
        line++;
    }

    ERR_PRINT(p_error);
}

void ShaderGLES2::_get_uniform_locations(Version::Variant &variant, Version *p_version) {
    glUseProgram(variant.id);

    variant.uniform_location.resize(uniform_count);
    for (int i = 0; i < uniform_count; i++) {
        variant.uniform_location[i] = glGetUniformLocation(variant.id, uniform_names[i]);
    }

    for (int i = 0; i < texunit_pair_count; i++) {
        GLint loc = glGetUniformLocation(variant.id, texunit_pairs[i].name);
        if (loc >= 0) {
            if (texunit_pairs[i].index < 0) {
                glUniform1i(loc, texunit_pairs[i].index);
            } else {
                glUniform1i(loc, texunit_pairs[i].index);
            }
        }
    }

    // textures
    int texture_index = 0;
    for (uint32_t i = 0; i < p_version->texture_uniforms.size(); i++) {
        String native_uniform_name = _mkid(p_version->texture_uniforms[i].name);
        GLint location = glGetUniformLocation(variant.id, (native_uniform_name).ascii().get_data());
        Vector<int32_t> texture_uniform_bindings;
        int texture_count = p_version->texture_uniforms[i].array_size;
        for (int j = 0; j < texture_count; j++) {
            texture_uniform_bindings.append(texture_index + base_texture_index);
            texture_index++;
        }
        glUniform1iv(location, texture_uniform_bindings.size(), texture_uniform_bindings.ptr());
    }

    glUseProgram(0);
}

void ShaderGLES2::_compile_variant(Version::Variant &variant, uint32_t p_variant, Version *p_version) {
    variant.id = glCreateProgram();
    variant.ok = false;
    GLint status;

    //vertex stage
    {
        StringBuilder builder;
        _build_variant_code(builder, p_variant, p_version, STAGE_TYPE_VERTEX);

        variant.vert_id = glCreateShader(GL_VERTEX_SHADER);
        String builder_string = builder.as_string();
        CharString cs = builder_string.utf8();
        const char *cstr = cs.ptr();
        glShaderSource(variant.vert_id, 1, &cstr, nullptr);
        glCompileShader(variant.vert_id);

        glGetShaderiv(variant.vert_id, GL_COMPILE_STATUS, &status);
        if (status == GL_FALSE) {
            GLsizei iloglen;
            glGetShaderiv(variant.vert_id, GL_INFO_LOG_LENGTH, &iloglen);

            if (iloglen < 0) {
                glDeleteShader(variant.vert_id);
                glDeleteProgram(variant.id);
                variant.id = 0;

                ERR_PRINT("No OpenGL vertex shader compiler log. What the frick?");
            } else {
                if (iloglen == 0) {
                    iloglen = 4096; // buggy driver (Adreno 220+)
                }

                char *ilogmem = (char *)Memory::alloc_static(iloglen + 1);
                ilogmem[iloglen] = '\0';
                glGetShaderInfoLog(variant.vert_id, iloglen, &iloglen, ilogmem);

                String err_string = name + ": Vertex shader compilation failed:\n";

                err_string += ilogmem;

                _display_error_with_code(err_string, builder_string);

                Memory::free_static(ilogmem);
                glDeleteShader(variant.vert_id);
                glDeleteProgram(variant.id);
                variant.id = 0;
            }

            ERR_FAIL();
        }
    }

    //fragment stage
    {
        StringBuilder builder;
        _build_variant_code(builder, p_variant, p_version, STAGE_TYPE_FRAGMENT);

        variant.frag_id = glCreateShader(GL_FRAGMENT_SHADER);
        String builder_string = builder.as_string();
        CharString cs = builder_string.utf8();
        const char *cstr = cs.ptr();
        glShaderSource(variant.frag_id, 1, &cstr, nullptr);
        glCompileShader(variant.frag_id);

        glGetShaderiv(variant.frag_id, GL_COMPILE_STATUS, &status);
        if (status == GL_FALSE) {
            GLsizei iloglen;
            glGetShaderiv(variant.frag_id, GL_INFO_LOG_LENGTH, &iloglen);

            if (iloglen < 0) {
                glDeleteShader(variant.frag_id);
                glDeleteProgram(variant.id);
                variant.id = 0;

                ERR_PRINT("No OpenGL fragment shader compiler log. What the frick?");
            } else {
                if (iloglen == 0) {
                    iloglen = 4096; // buggy driver (Adreno 220+)
                }

                char *ilogmem = (char *)Memory::alloc_static(iloglen + 1);
                ilogmem[iloglen] = '\0';
                glGetShaderInfoLog(variant.frag_id, iloglen, &iloglen, ilogmem);

                String err_string = name + ": Fragment shader compilation failed:\n";

                err_string += ilogmem;

                _display_error_with_code(err_string, builder_string);

                Memory::free_static(ilogmem);
                glDeleteShader(variant.frag_id);
                glDeleteProgram(variant.id);
                variant.id = 0;
            }

            ERR_FAIL();
        }
    }

    glAttachShader(variant.id, variant.frag_id);
    glAttachShader(variant.id, variant.vert_id);

    // If feedback exists, set it up.
    if (feedback_count) {
        Vector<const char *> feedback;
        for (int i = 0; i < feedback_count; i++) {
            feedback.push_back(feedbacks[i].name);
        }

        if (feedback.size()) {
            // TODO GLES2: In GLES2, we use a workaround for transform feedback
            // by rendering to a texture and reading it back
            // This part would need to be implemented separately
            WARN_PRINT("Transform feedback not supported in GLES2. Using fallback method.");
        }
    }

    glLinkProgram(variant.id);

    glGetProgramiv(variant.id, GL_LINK_STATUS, &status);
    if (status == GL_FALSE) {
        GLsizei iloglen;
        glGetProgramiv(variant.id, GL_INFO_LOG_LENGTH, &iloglen);

        if (iloglen < 0) {
            glDeleteShader(variant.frag_id);
            glDeleteShader(variant.vert_id);
            glDeleteProgram(variant.id);
            variant.id = 0;

            ERR_PRINT("No OpenGL program link log. What the frick?");
            ERR_FAIL();
        }

        if (iloglen == 0) {
            iloglen = 4096; // buggy driver (Adreno 220+)
        }

        char *ilogmem = (char *)Memory::alloc_static(iloglen + 1);
        ilogmem[iloglen] = '\0';
        glGetProgramInfoLog(variant.id, iloglen, &iloglen, ilogmem);

        String err_string = name + ": Program linking failed:\n";

        err_string += ilogmem;

        _display_error_with_code(err_string, String());

        Memory::free_static(ilogmem);
        glDeleteShader(variant.frag_id);
        glDeleteShader(variant.vert_id);
        glDeleteProgram(variant.id);
        variant.id = 0;

        ERR_FAIL();
    }

    _get_uniform_locations(variant, p_version);

    variant.ok = true;
}

void ShaderGLES2::_clear_version(Version *p_version) {
    // Variants not compiled yet, just return
    if (p_version->variants.size() == 0) {
        return;
    }

    for (int i = 0; i < variant_count; i++) {
        Version::Variant &variant = p_version->variants[i];
        if (variant.id != 0) {
            glDeleteShader(variant.vert_id);
            glDeleteShader(variant.frag_id);
            glDeleteProgram(variant.id);
        }
    }

    p_version->variants.clear();
}

void ShaderGLES2::_initialize_version(Version *p_version) {
    ERR_FAIL_COND(p_version->variants.size() > 0);
    bool use_cache = shader_cache_dir_valid && !(feedback_count > 0 && GLES2::Config::get_singleton()->disable_shader_cache);
    if (use_cache && _load_from_cache(p_version)) {
        return;
    }
    p_version->variants.resize(variant_count);
    for (int i = 0; i < variant_count; i++) {
        Version::Variant &variant = p_version->variants[i];
        _compile_variant(variant, i, p_version);
    }
    if (use_cache) {
        _save_to_cache(p_version);
    }
}

void ShaderGLES2::version_set_code(RID p_version, const HashMap<String, String> &p_code, const String &p_uniforms, const String &p_vertex_globals, const String &p_fragment_globals, const Vector<String> &p_custom_defines, const LocalVector<ShaderGLES2::TextureUniformData> &p_texture_uniforms, bool p_initialize) {
    Version *version = version_owner.get_or_null(p_version);
    ERR_FAIL_NULL(version);

    _clear_version(version); //clear if existing

    version->vertex_globals = p_vertex_globals.utf8();
    version->fragment_globals = p_fragment_globals.utf8();
    version->uniforms = p_uniforms.utf8();
    version->code_sections.clear();
    version->texture_uniforms = p_texture_uniforms;
    for (const KeyValue<String, String> &E : p_code) {
        version->code_sections[StringName(E.key.to_upper())] = E.value.utf8();
    }

    version->custom_defines.clear();
    for (int i = 0; i < p_custom_defines.size(); i++) {
        version->custom_defines.push_back(p_custom_defines[i].utf8());
    }

    if (p_initialize) {
        _initialize_version(version);
    }
}

bool ShaderGLES2::version_is_valid(RID p_version) {
    Version *version = version_owner.get_or_null(p_version);
    return version != nullptr;
}

bool ShaderGLES2::version_free(RID p_version) {
    if (version_owner.owns(p_version)) {
        Version *version = version_owner.get_or_null(p_version);
        _clear_version(version);
        version_owner.free(p_version);
    } else {
        return false;
    }

    return true;
}

void ShaderGLES2::initialize(const String &p_general_defines, int p_base_texture_index) {
    general_defines = p_general_defines.utf8();
    base_texture_index = p_base_texture_index;

    _init();

    if (shader_cache_dir != String()) {
        StringBuilder hash_build;

        hash_build.append("[base_hash]");
        hash_build.append(base_sha256);
        hash_build.append("[general_defines]");
        hash_build.append(general_defines.get_data());
        for (int i = 0; i < variant_count; i++) {
            hash_build.append("[variant_defines:" + itos(i) + "]");
            hash_build.append(variant_defines[i]);
        }

        base_sha256 = hash_build.as_string().sha256_text();

        Ref<DirAccess> d = DirAccess::open(shader_cache_dir);
        ERR_FAIL_COND(d.is_null());
        if (d->change_dir(name) != OK) {
            Error err = d->make_dir(name);
            ERR_FAIL_COND(err != OK);
            d->change_dir(name);
        }

        //erase other versions?
        if (shader_cache_cleanup_on_start) {
            // TODO: Implement cleanup logic if needed
        }

        if (d->change_dir(base_sha256) != OK) {
            Error err = d->make_dir(base_sha256);
            ERR_FAIL_COND(err != OK);
        }
        shader_cache_dir_valid = true;

        print_verbose("Shader '" + name + "' SHA256: " + base_sha256);
    }
}

ShaderGLES2::ShaderGLES2() {
}

ShaderGLES2::~ShaderGLES2() {
    List<RID> remaining;
    version_owner.get_owned_list(&remaining);
    if (remaining.size()) {
        ERR_PRINT(itos(remaining.size()) + " shaders of type " + name + " were never freed");
        while (remaining.size()) {
            version_free(remaining.front()->get());
            remaining.pop_front();
        }
    }
}

#endif // GLES2_ENABLED
