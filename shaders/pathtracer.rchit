#version 460
#extension GL_GOOGLE_include_directive  : require
#extension GL_EXT_buffer_reference2     : require
#extension GL_EXT_nonuniform_qualifier  : require
//#extension GL_EXT_debug_printf          : enable

#include "common/material.glsl"
MaterialConstants material;

#include "common/bxdf.glsl"
#include "common/raycommon.glsl"
#include "common/hitcommon.glsl"

layout(binding = 0, set = 0) uniform accelerationStructureEXT tlas;
layout(binding = 6, set = 0) uniform sampler2D textures[];

layout(location = 1) rayPayloadEXT ShadowPayload shadowPrd;

void main() {
    Triangle tri = unpackTriangle(gl_PrimitiveID);

    material = getMaterial(tri);
    int materialId = getMaterialId(material);

    vec3 position = vec3(gl_ObjectToWorldEXT * vec4(tri.hit, 1.0));
    vec3 normal_g = normalize(vec3(tri.normalG * gl_WorldToObjectEXT));
    vec3 normal_s = normalize(vec3(tri.normalS * gl_WorldToObjectEXT));
    vec3 tangent  = normalize(vec3(tri.tangent.xyz * gl_WorldToObjectEXT));
    vec3 btangent = cross(normal_s, tangent.xyz) * tri.tangent.w;

/*    // Tangents visualization test
    prd.radiance = (tri.tangent) * 0.5 + vec3(1, 1, 1);
    prd.done = 1;
    return;
*/

    vec3 wo = toLocal(tangent, btangent, normal_s, -prd.direction);

    BxdfSample bxdfSample;

    for (int i = 0; bxdfSample.failed; i++) {
        vec2 r = vec2(random(prd.seed), random(prd.seed));

        bxdfSample = bxdfSampleMaterial(materialId, wo, prd.seed);
        
        if (i >= 100) {
            prd.done = 1;
            prd.radiance = vec3(10, 0, 10);
            return;
        }
    }

    prd.direction = fromLocal(tangent.xyz, btangent, normal_s, bxdfSample.wi);
    bool isTransmission = dot(normal_s, prd.direction) < 0.0;
    prd.origin = offsetPositionAlongNormal(position, isTransmission ? -normal_g : normal_g);

    // TODO check with geometric normal that new direction doesn't go inside object

    prd.radiance += prd.throughput * material.emission.rgb;
    prd.throughput *= material.albedo.rgb;

    // TODO sample bxdf for indirect light if light direction points away from normal

    if (properties.nextEventEstimation == 1) {
        float light_pdf;
        uvec2 offset;

        vec2 uv = samplePiecewiseConstant2D(
            infiniteLight.marginalFunctionBufferAddress, 
            infiniteLight.marginalCdfBufferAddress, 
            infiniteLight.marginalIntegral, 
            infiniteLight.conditionalFunctionBufferAddress, 
            infiniteLight.conditionalCdfBufferAddress, 
            infiniteLight.conditionalIntegralBufferAddress,
            infiniteLight.textureSize, 
            vec2(random(prd.seed), random(prd.seed)), 
            light_pdf, 
            offset
        );
        light_pdf /= 4.0 * M_PI; // Divide by 4 pi because of area of environment map

        vec3 le = vec3(1.0);
        if (infiniteLight.environmentID > -1) {
            le *= texture(textures[nonuniformEXT(infiniteLight.environmentID)], uv).rgb;
        }

        vec3 dir = equalAreaSquareToSphere(uv);
        vec3 light_dir = vec3(-dir.x, -dir.z, dir.y);
        vec3 wi = toLocal(tangent, btangent, normal_s, light_dir);

        float bxdf_f = bxdfF(materialId, wo, wi, prd.seed);
        float bxdf_pdf = bxdfPDF(materialId, wo, wi, prd.seed);

        float tmin = 0.0;
        float tmax = 1000.0;
        shadowPrd.visibility = false;
        if (bxdf_f > 0.0 && bxdf_pdf > 1e-3 && light_pdf > 1e-3) {
            traceRayEXT(tlas, gl_RayFlagsTerminateOnFirstHitEXT /*| gl_RayFlagsOpaqueEXT*/ | gl_RayFlagsSkipClosestHitShaderEXT, 0xff, 0, 0, 1, prd.origin, tmin, light_dir, tmax, 1);

            if (shadowPrd.visibility) {
                prd.radiance += prd.throughput * le * bxdf_f * absCosTheta(wi) / (light_pdf); // TODO what about bxdf_pdf?
            }
        }
    }

    //prd.throughput *= bxdfSample.f * absCosTheta(bxdfSample.wi) / bxdfSample.pdf; // TODO conductors look a little dark
}