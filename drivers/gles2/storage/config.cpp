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

#ifdef GLES2_ENABLED

#include "config.h"
#include "core/config/project_settings.h"
#include "core/string/print_string.h"

using namespace GLES2;

Config *Config::singleton = nullptr;

Config::Config() {
    singleton = this;

    {
        GLint n = 0;
        glGetIntegerv(GL_NUM_EXTENSIONS, &n);
        for (GLint i = 0; i < n; i++) {
            const GLubyte *s = glGetStringi(GL_EXTENSIONS, i);
            if (!s) {
                break;
            }
            extensions.insert((const char *)s);
        }
    }

    // Check for GLES2 extensions we care about
    float_texture_supported = extensions.has("GL_OES_texture_float");
    s3tc_supported = extensions.has("GL_EXT_texture_compression_s3tc") || extensions.has("GL_EXT_texture_compression_dxt1");
    etc_supported = extensions.has("GL_OES_compressed_ETC1_RGB8_texture");
    pvrtc_supported = extensions.has("GL_IMG_texture_compression_pvrtc");

    glGetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, &max_vertex_texture_image_units);
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &max_texture_image_units);
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size);
    glGetIntegerv(GL_MAX_VIEWPORT_DIMS, max_viewport_size);

    support_anisotropic_filter = extensions.has("GL_EXT_texture_filter_anisotropic");
    if (support_anisotropic_filter) {
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &anisotropic_level);
        anisotropic_level = MIN(float(1 << int(GLOBAL_GET("rendering/textures/default_filters/anisotropic_filtering_level"))), anisotropic_level);
    }

    glGetIntegerv(GL_MAX_SAMPLES, &msaa_max_samples);
    msaa_supported = msaa_max_samples > 1;

    force_vertex_shading = false; //GLOBAL_GET("rendering/quality/shading/force_vertex_shading");
    use_nearest_mip_filter = GLOBAL_GET("rendering/textures/default_filters/use_nearest_mipmap_filter");

    use_depth_prepass = bool(GLOBAL_GET("rendering/driver/depth_prepass/enable"));
    if (use_depth_prepass) {
        String vendors = GLOBAL_GET("rendering/driver/depth_prepass/disable_for_vendors");
        Vector<String> vendor_match = vendors.split(",");
        String renderer = String((const char *)glGetString(GL_RENDERER)).to_lower();
        for (int i = 0; i < vendor_match.size(); i++) {
            String v = vendor_match[i].strip_edges().to_lower();
            if (v == String())
                continue;

            if (renderer.find(v) != -1) {
                use_depth_prepass = false;
            }
        }
    }

    max_renderable_elements = GLOBAL_GET("rendering/limits/opengl/max_renderable_elements");
    max_renderable_lights = GLOBAL_GET("rendering/limits/opengl/max_renderable_lights");
    max_lights_per_object = GLOBAL_GET("rendering/limits/opengl/max_lights_per_object");

    // Adreno 3xx Compatibility
    const String rendering_device_name = String((const char *)glGetString(GL_RENDERER)).to_lower();
    if (rendering_device_name.find("adreno (tm) 3") != -1) {
        // TODO: Also 'GPUParticles'?
        //https://github.com/godotengine/godot/issues/92662#issuecomment-2161199477
        disable_particles_workaround = true;
        flip_xy_workaround = true;
    }
}

Config::~Config() {
    singleton = nullptr;
}

#endif // GLES2_ENABLED
