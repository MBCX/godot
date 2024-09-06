"""Functions used to generate source files during build time

All such functions are invoked in a subprocess on Windows to prevent build flakiness.

"""
import os.path
from typing import Optional

from methods import print_error, to_raw_cstring


class LegacyGLHeaderStruct:
    def __init__(self):
        self.vertex_lines = []
        self.fragment_lines = []
        self.uniforms = []
        self.attributes = []
        self.feedbacks = []
        self.fbos = []
        self.conditionals = []
        self.enums = {}
        self.texunits = []
        self.texunit_names = []
        self.ubos = []
        self.ubo_names = []

        self.vertex_included_files = []
        self.fragment_included_files = []

        self.reading = ""
        self.line_offset = 0
        self.vertex_offset = 0
        self.fragment_offset = 0


def include_file_in_legacygl_header(filename, header_data, depth):
    fs = open(filename, "r")
    line = fs.readline()

    while line:
        if line.find("#[vertex]") != -1:
            header_data.reading = "vertex"
            line = fs.readline()
            header_data.line_offset += 1
            header_data.vertex_offset = header_data.line_offset
            continue

        if line.find("#[fragment]") != -1:
            header_data.reading = "fragment"
            line = fs.readline()
            header_data.line_offset += 1
            header_data.fragment_offset = header_data.line_offset
            continue

        while line.find("#include ") != -1:
            includeline = line.replace("#include ", "").strip()[1:-1]
            included_file = os.path.relpath(os.path.dirname(filename) + "/" + includeline)
            if not included_file in header_data.vertex_included_files and header_data.reading == "vertex":
                header_data.vertex_included_files += [included_file]
                if include_file_in_legacygl_header(included_file, header_data, depth + 1) is None:
                    print("Error in file '" + filename + "': #include " + includeline + "could not be found!")
            elif not included_file in header_data.fragment_included_files and header_data.reading == "fragment":
                header_data.fragment_included_files += [included_file]
                if include_file_in_legacygl_header(included_file, header_data, depth + 1) is None:
                    print("Error in file '" + filename + "': #include " + includeline + "could not be found!")
            line = fs.readline()

        if line.find("uniform") != -1 and line.lower().find("texunit:") != -1:
            texunitstr = line[line.find(":") + 1 :].strip()
            if texunitstr == "auto":
                texunit = "-1"
            else:
                texunit = str(int(texunitstr))
            uline = line[: line.lower().find("//")]
            uline = uline.replace("uniform", "").replace("highp", "").replace(";", "")
            lines = uline.split(",")
            for x in lines:
                x = x.strip()
                x = x[x.rfind(" ") + 1 :]
                if x.find("[") != -1:
                    x = x[: x.find("[")]
                if not x in header_data.texunit_names:
                    header_data.texunits += [(x, texunit)]
                    header_data.texunit_names += [x]

        elif line.find("uniform") != -1 and line.find("{") == -1 and line.find(";") != -1:
            uline = line.replace("uniform", "").replace(";", "")
            lines = uline.split(",")
            for x in lines:
                x = x.strip()
                x = x[x.rfind(" ") + 1 :]
                if x.find("[") != -1:
                    x = x[: x.find("[")]
                if not x in header_data.uniforms:
                    header_data.uniforms += [x]

        if line.strip().find("attribute ") == 0 and line.find("attrib:") != -1:
            uline = line.replace("in ", "").replace("attribute ", "").replace("highp ", "").replace(";", "")
            uline = uline[uline.find(" ") :].strip()

            if uline.find("//") != -1:
                name, bind = uline.split("//")
                if bind.find("attrib:") != -1:
                    name = name.strip()
                    bind = bind.replace("attrib:", "").strip()
                    header_data.attributes += [(name, bind)]

        line = line.replace("\r", "").replace("\n", "")

        if header_data.reading == "vertex":
            header_data.vertex_lines += [line]
        if header_data.reading == "fragment":
            header_data.fragment_lines += [line]

        line = fs.readline()
        header_data.line_offset += 1

    fs.close()

    return header_data

def build_legacygl_header(filename, include, class_suffix, output_attribs, gles2=False):
    header_data = LegacyGLHeaderStruct()
    include_file_in_legacygl_header(filename, header_data, 0)

    out_file = filename + ".gen.h"
    fd = open(out_file, "w")

    enum_constants = []

    fd.write("/* WARNING, THIS FILE WAS GENERATED, DO NOT EDIT */\n")

    out_file_base = os.path.basename(out_file)
    out_file_ifdef = out_file_base.replace(".", "_").upper()
    fd.write("#ifndef " + out_file_ifdef + class_suffix + "_120\n")
    fd.write("#define " + out_file_ifdef + class_suffix + "_120\n")

    out_file_class = (
        out_file_base.replace(".glsl.gen.h", "").title().replace("_", "").replace(".", "") + "Shader" + class_suffix
    )
    fd.write("\n\n")
    fd.write('#include "' + include + '"\n\n\n')
    fd.write("class " + out_file_class + " : public ShaderGLES2 {\n\n")
    fd.write('public:\n\n')

    fd.write("\tenum Uniforms {\n")
    for x in header_data.uniforms:
        fd.write("\t\t" + x.upper() + ",\n")
    fd.write("\t};\n\n")

    fd.write("\tenum ShaderVariant {\n")
    for x in header_data.conditionals:
        fd.write("\t\t" + x.upper() + ",\n")
    fd.write("\t};\n\n")

    fd.write("\t_FORCE_INLINE_ bool version_bind_shader(RID p_version, ShaderVariant p_variant) { return _version_bind_shader(p_version, (int)p_variant); }\n\n")
    fd.write("\t_FORCE_INLINE_ int version_get_uniform(Uniforms p_uniform, RID p_version, ShaderVariant p_variant) { return _version_get_uniform((int)p_uniform, p_version, (int)p_variant); }\n\n")

    fd.write("\t#define _FU if (version_get_uniform(p_uniform, p_version, p_variant) < 0) return; \n\n")

    fd.write("\t_FORCE_INLINE_ void version_set_uniform(Uniforms p_uniform, float p_value, RID p_version, ShaderVariant p_variant) { _FU glUniform1f(version_get_uniform(p_uniform, p_version, p_variant), p_value); }\n\n")
    fd.write("\t_FORCE_INLINE_ void version_set_uniform(Uniforms p_uniform, double p_value, RID p_version, ShaderVariant p_variant) { _FU glUniform1f(version_get_uniform(p_uniform, p_version, p_variant), p_value); }\n\n")
    fd.write("\t_FORCE_INLINE_ void version_set_uniform(Uniforms p_uniform, uint8_t p_value, RID p_version, ShaderVariant p_variant) { _FU glUniform1i(version_get_uniform(p_uniform, p_version, p_variant), p_value); }\n\n")
    fd.write("\t_FORCE_INLINE_ void version_set_uniform(Uniforms p_uniform, int8_t p_value, RID p_version, ShaderVariant p_variant) { _FU glUniform1i(version_get_uniform(p_uniform, p_version, p_variant), p_value); }\n\n")
    fd.write("\t_FORCE_INLINE_ void version_set_uniform(Uniforms p_uniform, uint16_t p_value, RID p_version, ShaderVariant p_variant) { _FU glUniform1i(version_get_uniform(p_uniform, p_version, p_variant), p_value); }\n\n")
    fd.write("\t_FORCE_INLINE_ void version_set_uniform(Uniforms p_uniform, int16_t p_value, RID p_version, ShaderVariant p_variant) { _FU glUniform1i(version_get_uniform(p_uniform, p_version, p_variant), p_value); }\n\n")
    fd.write("\t_FORCE_INLINE_ void version_set_uniform(Uniforms p_uniform, uint32_t p_value, RID p_version, ShaderVariant p_variant) { _FU glUniform1i(version_get_uniform(p_uniform, p_version, p_variant), p_value); }\n\n")
    fd.write("\t_FORCE_INLINE_ void version_set_uniform(Uniforms p_uniform, int32_t p_value, RID p_version, ShaderVariant p_variant) { _FU glUniform1i(version_get_uniform(p_uniform, p_version, p_variant), p_value); }\n\n")
    fd.write("\t_FORCE_INLINE_ void version_set_uniform(Uniforms p_uniform, const Color& p_color, RID p_version, ShaderVariant p_variant) { _FU GLfloat col[4]={p_color.r,p_color.g,p_color.b,p_color.a}; glUniform4fv(version_get_uniform(p_uniform, p_version, p_variant),1,col); }\n\n")
    fd.write("\t_FORCE_INLINE_ void version_set_uniform(Uniforms p_uniform, const Vector2& p_vec2, RID p_version, ShaderVariant p_variant) { _FU GLfloat vec2[2]={p_vec2.x,p_vec2.y}; glUniform2fv(version_get_uniform(p_uniform, p_version, p_variant),1,vec2); }\n\n")
    fd.write("\t_FORCE_INLINE_ void version_set_uniform(Uniforms p_uniform, const Size2i& p_vec2, RID p_version, ShaderVariant p_variant) { _FU GLint vec2[2]={p_vec2.x,p_vec2.y}; glUniform2iv(version_get_uniform(p_uniform, p_version, p_variant),1,vec2); }\n\n")
    fd.write("\t_FORCE_INLINE_ void version_set_uniform(Uniforms p_uniform, const Vector3& p_vec3, RID p_version, ShaderVariant p_variant) { _FU GLfloat vec3[3]={p_vec3.x,p_vec3.y,p_vec3.z}; glUniform3fv(version_get_uniform(p_uniform, p_version, p_variant),1,vec3); }\n\n")
    fd.write("\t_FORCE_INLINE_ void version_set_uniform(Uniforms p_uniform, float p_a, float p_b, RID p_version, ShaderVariant p_variant) { _FU glUniform2f(version_get_uniform(p_uniform, p_version, p_variant),p_a,p_b); }\n\n")
    fd.write("\t_FORCE_INLINE_ void version_set_uniform(Uniforms p_uniform, float p_a, float p_b, float p_c, RID p_version, ShaderVariant p_variant) { _FU glUniform3f(version_get_uniform(p_uniform, p_version, p_variant),p_a,p_b,p_c); }\n\n")
    fd.write("\t_FORCE_INLINE_ void version_set_uniform(Uniforms p_uniform, float p_a, float p_b, float p_c, float p_d, RID p_version, ShaderVariant p_variant) { _FU glUniform4f(version_get_uniform(p_uniform, p_version, p_variant),p_a,p_b,p_c,p_d); }\n\n")

    fd.write("\t#undef _FU\n\n\n")

    fd.write("protected:\n")
    fd.write("\tvirtual void _init() override {\n\n")

    if header_data.conditionals:
        fd.write("\t\tstatic const char* _conditional_strings[]={")
        for x in header_data.conditionals:
            fd.write('"#define ' + x + '\\n",')
        fd.write("nullptr};\n\n")
    else:
        fd.write("\t\tstatic const char **_conditional_strings=nullptr;\n")

    if header_data.texunits:
        fd.write("\t\tstatic TexUnitPair _texunit_pairs[]={\n")
        for x in header_data.texunits:
            fd.write('\t\t\t{"' + x[0] + '",' + x[1] + "},\n")
        fd.write("\t\t};\n\n")
    else:
        fd.write("\t\tstatic TexUnitPair *_texunit_pairs=nullptr;\n")

    if header_data.uniforms:
        fd.write("\t\tstatic const char* _uniform_strings[]={")
        for x in header_data.uniforms:
            fd.write('"' + x + '",')
        fd.write("nullptr};\n\n")
    else:
        fd.write("\t\tstatic const char **_uniform_strings=nullptr;\n")

    fd.write("\t\tstatic const char _vertex_code[]={\n")
    fd.write(to_raw_cstring(header_data.vertex_lines))
    fd.write("\n\t\t};\n\n")

    fd.write("\t\tstatic const char _fragment_code[]={\n")
    fd.write(to_raw_cstring(header_data.fragment_lines))
    fd.write("\n\t\t};\n\n")

    fd.write("\t\tstatic const int _vertex_code_start=" + str(header_data.vertex_offset) + ";\n")
    fd.write("\t\tstatic const int _fragment_code_start=" + str(header_data.fragment_offset) + ";\n")

    fd.write("\t\t_setup(_vertex_code,_fragment_code," +
         "\"" + out_file_class + "\"," +  # p_name parameter
         str(len(header_data.uniforms)) + ",_uniform_strings," +
         "0,nullptr," +  # p_ubo_count and p_ubos
         "0,nullptr," +  # p_feedback_count and p_feedback
         str(len(header_data.texunits)) + ",_texunit_pairs," +
         str(len(header_data.conditionals)) + ",_conditional_strings);\n")
    fd.write("\t}\n")

    fd.write("};\n\n")
    fd.write("#endif\n")
    fd.close()


def build_gles2_headers(target, source, env):
    for x in source:
        build_legacygl_header(str(x), include="drivers/gles2/shader_gles2.h", output_attribs=True, class_suffix="GLES2", gles2=True)
