#ifndef STDLIB_INC_GLES2
#define STDLIB_INC_GLES2

// GLES2 doesn't support bitwise operations on floats, so we need to use a different approach
// This function converts a float to its bit representation
highp vec4 floatBitsToVec4(highp float f) {
    highp vec4 bitSplit = vec4(1.0, 255.0, 65025.0, 16581375.0);
    highp vec4 bitMask = vec4(1.0/255.0, 1.0/255.0, 1.0/255.0, 1.0/255.0);
    highp vec4 result = fract(f * bitSplit);
    return result - result.xxyz * bitMask;
}

// This function converts a bit representation back to a float
highp float vec4ToFloatBits(highp vec4 v) {
    highp vec4 bitSplit = vec4(1.0, 1.0/255.0, 1.0/65025.0, 1.0/16581375.0);
    return dot(v, bitSplit);
}

// This function emulates the behavior of floatBitsToUint
highp float floatBitsToUint(highp float f) {
    return vec4ToFloatBits(floatBitsToVec4(f)) * 4294967295.0;
}

// This function emulates the behavior of uintBitsToFloat
highp float uintBitsToFloat(highp float u) {
    return vec4ToFloatBits(floatBitsToVec4(u / 4294967295.0));
}

highp vec2 unpackHalf2x16(highp float f) {
    highp vec4 bits = floatBitsToVec4(f);
    highp float x = floor(bits.x * 255.0 + 0.5) + floor(bits.y * 255.0 + 0.5) * 256.0;
    highp float y = floor(bits.z * 255.0 + 0.5) + floor(bits.w * 255.0 + 0.5) * 256.0;
    return vec2(x / 65535.0, y / 65535.0) * 2.0 - 1.0;
}

highp float packHalf2x16(highp vec2 v) {
    v = clamp(v * 0.5 + 0.5, 0.0, 1.0);
    highp vec4 bits = vec4(
        fract(v.x * 255.0),
        fract(v.x * 65280.0) / 256.0,
        fract(v.y * 255.0),
        fract(v.y * 65280.0) / 256.0
    );
    return vec4ToFloatBits(bits);
}

highp vec2 unpackUnorm2x16(highp float p) {
    highp vec4 bits = floatBitsToVec4(p);
    return vec2(
        (floor(bits.x * 255.0 + 0.5) + floor(bits.y * 255.0 + 0.5) * 256.0) / 65535.0,
        (floor(bits.z * 255.0 + 0.5) + floor(bits.w * 255.0 + 0.5) * 256.0) / 65535.0
    );
}

highp float packUnorm2x16(highp vec2 v) {
    v = clamp(v, 0.0, 1.0);
    highp vec4 bits = vec4(
        fract(v.x * 255.0),
        fract(v.x * 65280.0) / 256.0,
        fract(v.y * 255.0),
        fract(v.y * 65280.0) / 256.0
    );
    return vec4ToFloatBits(bits);
}

highp vec2 unpackSnorm2x16(highp float p) {
    highp vec2 v = unpackUnorm2x16(p);
    return v * 2.0 - 1.0;
}

highp float packSnorm2x16(highp vec2 v) {
    return packUnorm2x16((clamp(v, -1.0, 1.0) * 0.5 + 0.5));
}

highp vec4 unpackUnorm4x8(highp float p) {
    highp vec4 bits = floatBitsToVec4(p);
    return bits;
}

highp float packUnorm4x8(highp vec4 v) {
    v = clamp(v, 0.0, 1.0);
    return vec4ToFloatBits(v);
}

highp vec4 unpackSnorm4x8(highp float p) {
    return unpackUnorm4x8(p) * 2.0 - 1.0;
}

highp float packSnorm4x8(highp vec4 v) {
    return packUnorm4x8((clamp(v, -1.0, 1.0) * 0.5 + 0.5));
}

#endif // STDLIB_INC_GLES2
