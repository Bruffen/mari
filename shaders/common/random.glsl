/*
 * Taken from nvpro-samples/vk_raytracing_tutorial_KHR jitter camera
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
float random(inout uint prev) {
    return (float(lcg(prev)) / float(0x01000000));
}

#include "constants.glsl"

/**
 * 2 Dimensional Sampling
 *
 * Uniform disk polar
 */
 vec2 sampleUniformDiskPolar(float r1, float r2) {
    float radius = sqrt(r1);
    float theta  = 2.0 * M_PI * r2;
    return vec2(radius * cos(theta), radius * sin(theta));
 }

/*
 * Uniform disk concentric
 */

vec2 sampleUniformDiskConcentric(float r1, float r2) {
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
vec3 sampleUniformHemisphere(float r1, float r2) {
    float z = r1;
    float radius = sqrt(1.0 - z*z);             // pbrt uses a safesqrt method here
    float phi = 2 * M_PI * r2;
    return vec3(radius * cos(phi), radius * sin(phi), z);
}

float pdfUniformHemisphere() { return M_1_2PI; }

/**
 * Cosine weighted hemisphere
 */
vec3 sampleCosineHemisphere(float r1, float r2) {
    vec2 d  = sampleUniformDiskConcentric(r1, r2);
    float z = sqrt(max(0.0, 1.0 - d.x*d.x - d.y*d.y));
    return vec3(d.x, d.y, z);
}

float pdfCosineHemisphere(float cosTheta) { 
    return cosTheta * M_1_PI; 
}

/**
 * Uniform sphere
 */
vec3 sampleUniformSphere(float r1, float r2) {
    float z = 1.0 - 2.0 * r1;
    float radius = sqrt(1.0 - z*z);             // pbrt uses a safesqrt method here
    float phi = 2 * M_PI * r2;
    return vec3(radius * cos(phi), radius * sin(phi), z);
}

float pdfUniformSphere() { return M_1_4PI; }

/**
 * Samples a new diffuse direction with just a normal with no shading frame only for testing purposes
 */
vec3 sampleDiffuseTest(float r1, float r2, vec3 worldNormal) {
    const float theta = 6.2831853 * r1;  // Random in [0, 2pi]
    const float u     = 2.0 * r2 - 1.0;  // Random in [-1, 1]
    const float r     = sqrt(1.0 - u * u);
    return normalize(worldNormal + vec3(r * cos(theta), u, r * sin(theta)));
}