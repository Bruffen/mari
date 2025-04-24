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
 * Uniform disk
 */
 vec2 sampleUniformDisk(float r1, float r2) {
    float radius = sqrt(r1);
    float theta  = 2.0 * M_PI * r2;
    return vec2(radius * cos(theta), radius * sin(theta));
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
    vec2 d  = sampleUniformDisk(r1, r2);        // TODO pbrt uses sampleUniformDiskConcentric
    float z = sqrt(1.0 - d.x*d.x - d.y*d.y);    // pbrt uses a safesqrt method here
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
 * Samples a new diffuse direction with a normal only for testing purposes
 */
vec3 sampleDiffuseTest(float r1, float r2, vec3 worldNormal) {
    const float theta = 6.2831853 * r1;  // Random in [0, 2pi]
    const float u     = 2.0 * r2 - 1.0;  // Random in [-1, 1]
    const float r     = sqrt(1.0 - u * u);
    return normalize(worldNormal + vec3(r * cos(theta), u, r * sin(theta)));
}

/**
 * Shading frame to world frame transformations
 */
vec3 toLocal(vec3 t, vec3 b, vec3 n, vec3 v) {
    return vec3(dot(v, t), dot(v, b), dot(v, n));
}

vec3 fromLocal(vec3 t, vec3 b, vec3 n, vec3 v) {
    return v.x * t + v.y * b + v.z * n;
}

vec3 fromLocal(vec4 worldTangent, vec3 worldNormal, vec3 v) {
    vec3 worldBitangent = cross(worldNormal, worldTangent.xyz) * worldTangent.w;
    return fromLocal(worldTangent.xyz, worldBitangent, worldNormal, v);
}