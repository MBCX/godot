// GLES2 doesn't support layout qualifiers, so we remove them
// and declare individual uniforms instead of a uniform buffer
uniform highp float exposure;
uniform highp float white;
uniform lowp int tonemapper;

// Remove the 'mediump' qualifier as it's not always supported in GLES2
uniform float brightness;
uniform float contrast;
uniform float saturation;

// This expects 0-1 range input.
vec3 linear_to_srgb(vec3 color) {
    // Approximation from http://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html
    return max(vec3(1.055) * pow(color, vec3(0.416666667)) - vec3(0.055), vec3(0.0));
}

// This expects 0-1 range input, outside that range it behaves poorly.
vec3 srgb_to_linear(vec3 color) {
    // Approximation from http://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html
    return color * (color * (color * 0.305306011 + 0.682171111) + 0.012522878);
}

#ifdef APPLY_TONEMAPPING

vec3 tonemap_filmic(vec3 color, float p_white) {
    // exposure bias: input scale (color *= bias, white *= bias) to make the brightness consistent with other tonemappers
    // also useful to scale the input to the range that the tonemapper is designed for (some require very high input values)
    // has no effect on the curve's general shape or visual properties
    const float exposure_bias = 2.0;
    const float A = 0.22 * exposure_bias * exposure_bias; // bias baked into constants for performance
    const float B = 0.30 * exposure_bias;
    const float C = 0.10;
    const float D = 0.20;
    const float E = 0.01;
    const float F = 0.30;

    vec3 color_tonemapped = ((color * (A * color + C * B) + D * E) / (color * (A * color + B) + D * F)) - E / F;
    float p_white_tonemapped = ((p_white * (A * p_white + C * B) + D * E) / (p_white * (A * p_white + B) + D * F)) - E / F;

    return color_tonemapped / p_white_tonemapped;
}

// Adapted from https://github.com/TheRealMJP/BakingLab/blob/master/BakingLab/ACES.hlsl
// (MIT License).
vec3 tonemap_aces(vec3 color, float p_white) {
    const float exposure_bias = 1.8;
    const float A = 0.0245786;
    const float B = 0.000090537;
    const float C = 0.983729;
    const float D = 0.432951;
    const float E = 0.238081;

    // Exposure bias baked into transform to save shader instructions. Equivalent to `color *= exposure_bias`
    const mat3 rgb_to_rrt = mat3(
        vec3(0.59719 * exposure_bias, 0.35458 * exposure_bias, 0.04823 * exposure_bias),
        vec3(0.07600 * exposure_bias, 0.90834 * exposure_bias, 0.01566 * exposure_bias),
        vec3(0.02840 * exposure_bias, 0.13383 * exposure_bias, 0.83777 * exposure_bias));

    const mat3 odt_to_rgb = mat3(
        vec3(1.60475, -0.53108, -0.07367),
        vec3(-0.10208, 1.10813, -0.00605),
        vec3(-0.00327, -0.07276, 1.07602));

    color *= rgb_to_rrt;
    vec3 color_tonemapped = (color * (color + A) - B) / (color * (C * color + D) + E);
    color_tonemapped *= odt_to_rgb;

    p_white *= exposure_bias;
    float p_white_tonemapped = (p_white * (p_white + A) - B) / (p_white * (C * p_white + D) + E);

    return color_tonemapped / p_white_tonemapped;
}

vec3 tonemap_reinhard(vec3 color, float p_white) {
    return (p_white * color + color) / (color * p_white + p_white);
}

#define TONEMAPPER_LINEAR 0
#define TONEMAPPER_REINHARD 1
#define TONEMAPPER_FILMIC 2
#define TONEMAPPER_ACES 3

vec3 apply_tonemapping(vec3 color, float p_white) { // inputs are LINEAR, always outputs clamped [0;1] color
    // Ensure color values passed to tonemappers are positive.
    // They can be negative in the case of negative lights, which leads to undesired behavior.
    if (tonemapper == TONEMAPPER_LINEAR) {
        return color;
    } else if (tonemapper == TONEMAPPER_REINHARD) {
        return tonemap_reinhard(max(vec3(0.0), color), p_white);
    } else if (tonemapper == TONEMAPPER_FILMIC) {
        return tonemap_filmic(max(vec3(0.0), color), p_white);
    } else { // TONEMAPPER_ACES
        return tonemap_aces(max(vec3(0.0), color), p_white);
    }
}

#endif // APPLY_TONEMAPPING