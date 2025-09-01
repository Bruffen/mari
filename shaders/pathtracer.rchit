#version 460

#extension GL_GOOGLE_include_directive  : require
#extension GL_EXT_buffer_reference2     : require
#extension GL_EXT_nonuniform_qualifier  : require
#extension GL_EXT_debug_printf          : enable
#extension GL_EXT_ray_query             : require

#include "common/material.glsl"
MaterialConstants material;

#include "common/bxdf.glsl"
#include "common/raycommon.glsl"
#include "common/hitcommon.glsl"
#include "common/light.glsl"

layout(binding = 0, set = 0) uniform accelerationStructureEXT tlas;

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

    if ((prd.depth == 0 || properties.nextEventEstimation == 0)) {
        // TODO if single side -> if dot(normal_g, -prd.direction) > 0.0
        prd.radiance += prd.throughput * material.emission.rgb;
    }
    prd.throughput *= material.albedo.rgb;

    prd.direction = fromLocal(tangent.xyz, btangent, normal_s, bxdfSample.wi);
    bool isTransmission = dot(normal_s, prd.direction) < 0.0;
    prd.origin = offsetPositionAlongNormal(position, isTransmission ? -normal_g : normal_g);

    // TODO check with geometric normal that new direction doesn't go inside object

    // TODO sample bxdf for indirect light if light direction points away from normal

    if (properties.nextEventEstimation == 1) {
        float tmin = 0.0;
        float piecewise_pdf = 1.0;

        LightInfo light = sampleLight(random(prd.seed), piecewise_pdf);

        /*if (prd.thread.x == 500 && prd.thread.y == 500 && properties.frameCount % 100 == 0 && prd.depth == 0) {
            debugPrintfEXT("pdf = %f", piecewise_pdf);
        }*/

        LiSample li_sample;
        vec2 r = vec2(random(prd.seed), random(prd.seed));
        switch(light.type) {
            case LightType_Area:
                li_sample = sampleLiArea(prd.origin, r, light);
                break;
            case LightType_Infinite:
                li_sample = sampleLiInfinite(piecewise_pdf, r, light);
                break;
        }

        if (li_sample.pdf > 0.0) {
            piecewise_pdf /= lights.size;

            vec3 wi = toLocal(tangent, btangent, normal_s, li_sample.wi);

            float bxdf_f = bxdfF(materialId, wo, wi, prd.seed);
            float bxdf_pdf = bxdfPDF(materialId, wo, wi, prd.seed);

            shadowPrd.visibility = false;
            if (bxdf_f > 0.0 && bxdf_pdf > 0.0 && piecewise_pdf > 0.0) {
                traceRayEXT(
                    tlas, 
                    gl_RayFlagsTerminateOnFirstHitEXT /*| gl_RayFlagsOpaqueEXT*/ | gl_RayFlagsSkipClosestHitShaderEXT, 
                    0xff, 0, 0, 1, 
                    prd.origin, 
                    tmin, 
                    li_sample.wi, 
                    li_sample.distance * 0.999,
                    1
                );

                if (shadowPrd.visibility) {
                    prd.radiance += prd.throughput * li_sample.radiance * bxdf_f * absCosTheta(wi) / (li_sample.pdf * piecewise_pdf); // TODO what about bxdf_pdf?
                }
            }
        }
    }

    //prd.throughput *= bxdfSample.f * absCosTheta(bxdfSample.wi) / bxdfSample.pdf; // TODO conductors look a little darker
}