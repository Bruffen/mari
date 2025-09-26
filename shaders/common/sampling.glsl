#ifndef _SAMPLING_GLSL_
#define _SAMPLING_GLSL_

/*
 * Random generation taken from nvpro-samples/vk_raytracing_tutorial_KHR jitter camera
 * https://github.com/nvpro-samples/vk_raytracing_tutorial_KHR
 */

// Generate a random unsigned int from two unsigned int values, using 16 pairs
// of rounds of the Tiny Encryption Algorithm. See Zafar, Olano, and Curtis,
// "GPU Random Numbers via the Tiny Encryption Algorithm"
uint tea(uint val0, uint val1) {
    uint v0 = val0;
    uint v1 = val1;
    uint s0 = 0;

    for(uint n = 0; n < 16; n++) {
        s0 += 0x9e3779b9;
        v0 += ((v1 << 4) + 0xa341316c) ^ (v1 + s0) ^ ((v1 >> 5) + 0xc8013ea4);
        v1 += ((v0 << 4) + 0xad90777d) ^ (v0 + s0) ^ ((v0 >> 5) + 0x7e95761e);
    }

    return v0;
}

// Generate a random unsigned int in [0, 2^24) given the previous RNG state
// using the Numerical Recipes linear congruential generator
uint lcg(inout uint prev) {
    uint LCG_A = 1664525u;
    uint LCG_C = 1013904223u;
    prev       = (LCG_A * prev + LCG_C);
    return prev & 0x00FFFFFF;
}

// Generate a random float in [0, 1) given the previous RNG state
float random1D(inout uint prev) {
    return (float(lcg(prev)) / float(0x01000000));
}

vec2 random2D(inout uint prev) {
    return vec2(random1D(prev), random1D(prev));
}

vec3 random3D(inout uint prev) {
    return vec3(random1D(prev), random1D(prev), random1D(prev));
}

#include "constants.glsl"
#include "math.glsl"

/**
 * 1 Dimensional Sampling
 * 
 * Exponential
 */
float sample_exponential(float random, float a) {
    return -log(1.0 - random) / a;
}

/**
 * Piecewise Constant 1D
 */ 
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

layout(buffer_reference, scalar) readonly buffer FunctionBuffer { float func[]; };
layout(buffer_reference, scalar) readonly buffer CdfBuffer      { float cdf[];  };

float sample_piecewise_constant_1D(uint64_t functionAddress, uint64_t cdfAddress, uint functionSize, float integral, float random, inout float pdf, inout uint offset) {
    FunctionBuffer funcBuffer = FunctionBuffer(functionAddress);
    CdfBuffer cdfBuffer = CdfBuffer(cdfAddress);

    // Find interval
    uint size = functionSize - 1; // cdfSize - 2
    uint first = 1;
    while (size > 0) {
        uint halfsize = size >> 1;
        uint middle = first + halfsize;

        bool result = cdfBuffer.cdf[middle] <= random;
        first = result ? middle + 1 : first;
        size  = result ? size - (halfsize + 1) : halfsize;
    }

    offset = clamp(first - 1, 0, size - 2);

    // Compute offset along CDF segment
    float du = random - cdfBuffer.cdf[offset];
    if (cdfBuffer.cdf[offset + 1] - cdfBuffer.cdf[offset] > 0) {
        du /= cdfBuffer.cdf[offset + 1] - cdfBuffer.cdf[offset];
    }

    pdf = integral > 0.0 ? funcBuffer.func[offset] / integral : 0.0;
    return mix(0.0, 1.0, (offset + du) / functionSize);
}

/**
 * 2 Dimensional Sampling
 *
 * Piecewise Constant 2D
 */
layout(buffer_reference, scalar) readonly buffer ConditionalIntegralBuffer { float integrals[]; };

vec2 sample_piecewise_constant_2D(uint64_t marginalFunctionAddress, uint64_t marginalCdfAddress, float marginalIntegral, uint64_t conditionalFunctionAddress, uint64_t conditionalCdfAddress, uint64_t conditionalIntegralAddress,
uvec2 functionSize, vec2 random, inout float pdf, inout uvec2 offset) {
    float pdfy;
    float pdfx;
    uint offsety;
    uint offsetx;
    float d1 = sample_piecewise_constant_1D(marginalFunctionAddress, marginalCdfAddress, functionSize.y, marginalIntegral, random.y, pdfy, offsety);
    float conditionalIntegral = ConditionalIntegralBuffer(conditionalIntegralAddress).integrals[offsety];
    uint64_t byteOffset = 4 * offsety;
    float d0 = sample_piecewise_constant_1D(conditionalFunctionAddress + (byteOffset * uint64_t(functionSize.x)), conditionalCdfAddress + (byteOffset * uint64_t(functionSize.x + 1)), functionSize.x, conditionalIntegral, random.x, pdfx, offsetx);

    pdf = pdfx * pdfy;
    offset = uvec2(offsetx, offsety);
    return vec2(d0, d1);
}

/**
 * Uniform disk polar
 */
vec2 sample_uniform_disk_polar(float r1, float r2) {
    float radius = sqrt(r1);
    float theta  = 2.0 * M_PI * r2;
    return vec2(radius * cos(theta), radius * sin(theta));
}

/**
 * Uniform disk concentric
 */
vec2 sample_uniform_disk_concentric(float r1, float r2) {
    vec2 offset = 2.0 * vec2(r1, r2) - vec2(1, 1);
    if (offset.x == 0.0 && offset.y == 0.0) {
        return vec2(0, 0);
    }

    float theta;
    float r;

    if (abs(offset.x) > abs(offset.y)) {
        r = offset.x;
        theta = M_PI_4 * (offset.y / offset.x);
    } else {
        r = offset.y;
        theta = M_PI_2 - M_PI_4 * (offset.x / offset.y);
    }

    return r * vec2(cos(theta), sin(theta));
}

/**
 * 3 Dimensional Sampling
 * These methods come from the pbrt book, which uses a +z up shading frame
 *
 * Uniform hemisphere
 */
vec3 sample_uniform_hemisphere(float r1, float r2) {
    float z = r1;
    float radius = sqrt(1.0 - z*z);
    float phi = 2 * M_PI * r2;
    return vec3(radius * cos(phi), radius * sin(phi), z);
}

float pdf_uniform_hemisphere() { return M_1_2PI; }

/**
 * Cosine weighted hemisphere
 */
vec3 sample_cosine_hemisphere(float r1, float r2) {
    vec2 d  = sample_uniform_disk_concentric(r1, r2);
    float z = sqrt(max(0.0, 1.0 - d.x*d.x - d.y*d.y));
    return vec3(d.x, d.y, z);
}

float pdf_cosine_hemisphere(float cos_theta) { 
    return cos_theta * M_1_PI; 
}

/**
 * Uniform sphere
 */
vec3 sample_uniform_sphere(float r1, float r2) {
    float z = 1.0 - 2.0 * r1;
    float radius = sqrt(1.0 - z*z);
    float phi = 2 * M_PI * r2;
    return vec3(radius * cos(phi), radius * sin(phi), z);
}

float pdf_uniform_sphere() { return M_1_4PI; }

/**
 * Uniform triangle
 */
vec3 sample_uniform_triangle(vec2 random) {
    vec3 b;
    if (random.x < random.y) {
        b.x = random.x / 2.0;
        b.y = random.y - b.x;
    } else {
        b.y = random.y / 2.0;
        b.x = random.x - b.y;
    }
    b.z = 1.0 - b.x - b.y;
    return b;
}

/**
 * Samples a new diffuse direction with just a normal with no shading frame. Only for testing purposes
 */
vec3 sample_diffuse_test(float r1, float r2, vec3 worldNormal) {
    const float theta = 2 * M_PI * r1;  // Random in [0, 2pi]
    const float u     = 2.0 * r2 - 1.0;  // Random in [-1, 1]
    const float r     = sqrt(1.0 - u * u);
    return normalize(worldNormal + vec3(r * cos(theta), u, r * sin(theta)));
}

/**
 * Multiple Importance Sampling
 */
float balance_heuristic(float pdf1, float pdf2) {
    return pdf1 / (pdf1 + pdf2);
}

float power_heuristic(float pdf1, float pdf2) {
    return balance_heuristic(sqr(pdf1), sqr(pdf2));
}

#endif // _SAMPLING_GLSL_