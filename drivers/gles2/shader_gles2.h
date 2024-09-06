/**************************************************************************/
/*  shader_gles2.h                                                        */
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

#ifndef SHADER_GLES2_H
#define SHADER_GLES2_H

#include "core/math/projection.h"
#include "core/os/mutex.h"
#include "core/string/string_builder.h"
#include "core/templates/hash_map.h"
#include "core/templates/local_vector.h"
#include "core/templates/rb_map.h"
#include "core/templates/rid_owner.h"
#include "core/variant/variant.h"
#include "servers/rendering_server.h"

#ifdef GLES2_ENABLED

#include "platform_gl.h"

#include <stdio.h>

class ShaderGLES2 {
public:
    struct TextureUniformData {
        StringName name;
        int array_size;
    };

protected:
    struct TexUnitPair {
        const char *name;
        int index;
    };

    struct UBOPair {
        const char *name;
        int index;
    };

    // Remove Specialization struct as it's not needed in GLES2

    struct Feedback {
        const char *name;
        // Remove specialization field
    };

private:
    CharString general_defines;

    // Modify the Version struct to remove GLES3-specific features
    struct Version {
        LocalVector<TextureUniformData> texture_uniforms;
        CharString uniforms;
        CharString vertex_globals;
        CharString fragment_globals;
        HashMap<StringName, CharString> code_sections;
        Vector<CharString> custom_defines;

        struct Variant {
            GLuint id;
            GLuint vert_id;
            GLuint frag_id;
            LocalVector<GLint> uniform_location;
            LocalVector<GLint> texture_uniform_locations;
            bool ok = false;
            Variant() {
                id = 0;
                vert_id = 0;
                frag_id = 0;
            }
        };

        LocalVector<Variant> variants;
    };

    Mutex variant_set_mutex;

    void _get_uniform_locations(Version::Variant &variant, Version *p_version);
    void _compile_variant(Version::Variant &variant, uint32_t p_variant, Version *p_version);

    void _clear_version(Version *p_version);
    void _initialize_version(Version *p_version);

    RID_Owner<Version, true> version_owner;

    struct StageTemplate {
        struct Chunk {
            enum Type {
                TYPE_MATERIAL_UNIFORMS,
                TYPE_VERTEX_GLOBALS,
                TYPE_FRAGMENT_GLOBALS,
                TYPE_CODE,
                TYPE_TEXT
            };

            Type type;
            StringName code;
            CharString text;
        };
        LocalVector<Chunk> chunks;
    };

    String name;

    String base_sha256;

    static String shader_cache_dir;
    static bool shader_cache_cleanup_on_start;
    static bool shader_cache_save_compressed;
    static bool shader_cache_save_compressed_zstd;
    static bool shader_cache_save_debug;
    bool shader_cache_dir_valid = false;

    // Remove max_image_units as it's not relevant in GLES2

    enum StageType {
        STAGE_TYPE_VERTEX,
        STAGE_TYPE_FRAGMENT,
        STAGE_TYPE_MAX,
    };

    StageTemplate stage_templates[STAGE_TYPE_MAX];

    void _build_variant_code(StringBuilder &p_builder, uint32_t p_variant, const Version *p_version, StageType p_stage_type);

    void _add_stage(const char *p_code, StageType p_stage_type);

    String _version_get_sha1(Version *p_version) const;
    bool _load_from_cache(Version *p_version);
    void _save_to_cache(Version *p_version);

    const char **uniform_names = nullptr;
    int uniform_count = 0;
    const UBOPair *ubo_pairs = nullptr;
    int ubo_count = 0;
    const Feedback *feedbacks;
    int feedback_count = 0;
    const TexUnitPair *texunit_pairs = nullptr;
    int texunit_pair_count = 0;
    // Remove specialization-related variables
    const char **variant_defines = nullptr;
    int variant_count = 0;

    int base_texture_index = 0;
    Version::Variant *current_shader = nullptr;

protected:
    ShaderGLES2();
    void _setup(
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
	);

    _FORCE_INLINE_ bool _version_bind_shader(RID p_version, int p_variant) {
        ERR_FAIL_INDEX_V(p_variant, variant_count, false);

        Version *version = version_owner.get_or_null(p_version);
        ERR_FAIL_NULL_V(version, false);

        if (version->variants.size() == 0) {
            _initialize_version(version); //may lack initialization
        }

        Version::Variant *variant = &version->variants[p_variant];

        if (!variant->ok) {
            WARN_PRINT_ONCE("shader failed to compile, unable to bind shader.");
            return false;
        }

        glUseProgram(variant->id);
        current_shader = variant;
        return true;
    }

    _FORCE_INLINE_ int _version_get_uniform(int p_which, RID p_version, int p_variant) {
        ERR_FAIL_INDEX_V(p_which, uniform_count, -1);
        Version *version = version_owner.get_or_null(p_version);
        ERR_FAIL_NULL_V(version, -1);
        ERR_FAIL_INDEX_V(p_variant, int(version->variants.size()), -1);
        Version::Variant *variant = &version->variants[p_variant];
        ERR_FAIL_INDEX_V(p_which, int(variant->uniform_location.size()), -1);
        return variant->uniform_location[p_which];
    }

    virtual void _init() = 0;

public:
    RID version_create();

    void version_set_code(RID p_version, const HashMap<String, String> &p_code, const String &p_uniforms, const String &p_vertex_globals, const String &p_fragment_globals, const Vector<String> &p_custom_defines, const LocalVector<ShaderGLES2::TextureUniformData> &p_texture_uniforms, bool p_initialize = false);

    bool version_is_valid(RID p_version);

    bool version_free(RID p_version);

    static void set_shader_cache_dir(const String &p_dir);
    static void set_shader_cache_save_compressed(bool p_enable);
    static void set_shader_cache_save_compressed_zstd(bool p_enable);
    static void set_shader_cache_save_debug(bool p_enable);

    RS::ShaderNativeSourceCode version_get_native_source_code(RID p_version);

    void initialize(const String &p_general_defines = "", int p_base_texture_index = 0);
    virtual ~ShaderGLES2();
};

#endif // GLES2_ENABLED

#endif // SHADER_GLES2_H
