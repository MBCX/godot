/**************************************************************************/
/*  config.h                                                              */
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

#ifndef CONFIG_GLES2_H
#define CONFIG_GLES2_H

#ifdef GLES2_ENABLED

#include "core/config/project_settings.h"
#include "core/string/ustring.h"
#include "core/templates/hash_set.h"
#include "core/templates/vector.h"

#include "platform_gl.h"

namespace GLES2 {

class Config {
private:
    static Config *singleton;

public:
    bool use_nearest_mip_filter = false;
    bool use_depth_prepass = true;

    GLint max_vertex_texture_image_units = 0;
    GLint max_texture_image_units = 0;
    GLint max_texture_size = 0;
    GLint max_viewport_size[2] = { 0, 0 };

    int64_t max_renderable_elements = 0;
    int64_t max_renderable_lights = 0;
    int64_t max_lights_per_object = 0;

    bool generate_wireframes = false;

    HashSet<String> extensions;

    bool float_texture_supported = false;
    bool s3tc_supported = false;
    bool etc_supported = false;
    bool pvrtc_supported = false;
	bool disable_shader_cache = false;

    bool force_vertex_shading = false;

    bool support_anisotropic_filter = false;
    float anisotropic_level = 0.0f;

    GLint msaa_max_samples = 0;
    bool msaa_supported = false;

    // Adreno 3xx compatibility
    bool disable_particles_workaround = false;
    bool flip_xy_workaround = false;

    static Config *get_singleton() { return singleton; };

    Config();
    ~Config();
};

} // namespace GLES2

#endif // GLES2_ENABLED

#endif // CONFIG_GLES2_H
