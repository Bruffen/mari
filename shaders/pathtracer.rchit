#version 460
#extension GL_GOOGLE_include_directive  : require
#extension GL_EXT_buffer_reference2     : require
#extension GL_EXT_scalar_block_layout   : require
#extension GL_EXT_nonuniform_qualifier  : require

#include "common/material.glsl"
MaterialConstants material;

#include "common/bxdf.glsl"
#include "common/raycommon.glsl"
#include "common/hitcommon.glsl"

void main() {
    Triangle tri = unpackTriangle(gl_PrimitiveID);

    material = getMaterial(tri);

    vec3 position = vec3(gl_ObjectToWorldEXT * vec4(tri.hit, 1.0));
    vec3 normal_g = normalize(vec3(tri.normalG * gl_WorldToObjectEXT));
    vec3 normal_s = normalize(vec3(tri.normalS * gl_WorldToObjectEXT));
    vec3 tangent  = normalize(vec3(tri.tangent.xyz * gl_WorldToObjectEXT));
    vec3 btangent = cross(normal_s, tangent.xyz) * tri.tangent.w;

/*
    if (dot(-prd.direction, normal_g) < 0.0) { // TODO use faceforward()
        normal_g = -normal_g;
    }
    if (dot(normal_g, normal_s) < 0.0) { // TODO use faceforward()
        normal_s = -normal_s;
        tangent  = -tangent;
        btangent = -btangent;
    }
*/
/*    // Tangents visualization test
    prd.radiance = (tri.tangent) * 0.5 + vec3(1, 1, 1);
    prd.done = 1;
    return;
*/

    vec3 wo = toLocal(tangent, btangent, normal_s, -prd.direction);

    BsdfSample bsdfSample;

    for (int i = 0; bsdfSample.failed; i++) {
        vec2 r = vec2(random(prd.seed), random(prd.seed));

        // Dielectric
        if (material.thickness > 0.0) {
            bsdfSample = bsdfDielectricSample(wo, vec3(r.x, r.y, random(prd.seed)));
        } 
        
        // Conductor
        else if (material.metallic > 0.0) {
            float rm = random(prd.seed);
            if (rm < material.metallic) {
                bsdfSample = brdfConductorSample(wo, r);
            } else {
                bsdfSample = brdfDiffuseSample(wo, r);
            }
        } 
        
        // Diffuse 
        else {
            bsdfSample = brdfDiffuseSample(wo, r);
        }

        if (i >= 100) {
            prd.done = 1;
            prd.radiance = vec3(10, 0, 10);
            return;
        }
    }

    prd.direction = fromLocal(tangent.xyz, btangent, normal_s, bsdfSample.wi);
    bool isTransmission = dot(normal_s, prd.direction) < 0.0;
    prd.origin = offsetPositionAlongNormal(position, isTransmission ? -normal_g : normal_g);

    // TODO check with geometric normal that new direction doesn't go inside object

    prd.radiance += prd.throughput * material.emission.rgb;
    prd.throughput *= material.albedo.rgb;
}