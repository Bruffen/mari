#ifndef _RAY_COMMON_GLSL_
#define _RAY_COMMON_GLSL_

#extension GL_EXT_ray_tracing                               : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64    : require
#extension GL_EXT_scalar_block_layout                       : require

struct Payload {
    int instance_index;         // BLAS
    int geometry_index;         // PrimMesh
    int primitive_index;        // Triangle
    vec3 barycentrics;
    mat4x3 world_to_object;
    mat4x3 object_to_world;
    uint seed;
};

struct ShadowPayload {
    bool visibility;
};

#endif // _RAY_COMMON_GLSL_