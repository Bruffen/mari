#extension GL_EXT_ray_tracing : require

struct Payload {
    vec3 origin;
    vec3 direction;
    vec3 radiance;
    vec3 throughput;
    uint seed;
    uint done;
};
