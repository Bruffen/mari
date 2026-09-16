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
#ifdef SPECTRAL
    vec3 wavelengths; // TODO move out of payload?
#endif
};

struct ShadowPayload {
    bool visibility;
    uint seed;
};

struct VolumeBoundaryHit {
    float t;
    bool entering;
    uint64_t material_bda;
};

#define SHADOW_VOLUMETRIC_MAX_HITS 6

struct ShadowVolumetricPayload {
    bool visibility;
    uint seed;
    uint hit_count;
    VolumeBoundaryHit hits[SHADOW_VOLUMETRIC_MAX_HITS];
};

// Insertion sort
void sort_hits(inout VolumeBoundaryHit hits[SHADOW_VOLUMETRIC_MAX_HITS], uint count) {
    for (int i = 0; i < count; i++) {
        VolumeBoundaryHit k = hits[i];
        int j = i - 1;

        while (j >= 0 && hits[j].t > k.t) {
            hits[j + 1] = hits[j];
            j--;
        }

        hits[j + 1] = k;
    }
}

#endif // _RAY_COMMON_GLSL_