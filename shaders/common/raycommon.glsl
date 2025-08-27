#extension GL_EXT_ray_tracing : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_scalar_block_layout : require

struct Payload {
    uvec3   thread;
    uint    depth;
    vec3    origin;
    vec3    direction;
    vec3    radiance;
    vec3    throughput;
    float   eta;
    uint    seed;
    uint    done;
};

struct ShadowPayload {
    bool visibility;
};

layout(binding = 3, set = 0, scalar) uniform Properties {
    mat4 viewInverse;
    mat4 projInverse;
    int frameCount;
    int maxDepth;
    float exposure;
    int tonemapper;
    int russianRoulette;
    int nextEventEstimation;
    int samplesPerPixel;
} properties;

layout(binding = 4, set = 0, scalar) uniform InfiniteLight {
    int environmentID;
    float marginalIntegral;
    uvec2 textureSize;
    uint64_t marginalFunctionBufferAddress;
    uint64_t marginalCdfBufferAddress;
    uint64_t conditionalIntegralBufferAddress;
    uint64_t conditionalFunctionBufferAddress;
    uint64_t conditionalCdfBufferAddress;
} infiniteLight;