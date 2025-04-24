#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_nonuniform_qualifier : require

#include "common/random.glsl"
#include "common/raycommon.glsl"
#include "common/hitcommon.glsl"

void main() {
    Triangle tri = unpackTriangle(gl_PrimitiveID);

    vec3  color     = tri.material.constants.albedo.rgb * tri.color.rgb;
    float metallic  = tri.material.constants.metallic;
    float roughness = tri.material.constants.roughness;
    vec3  emission  = tri.material.constants.emission.rgb * tri.material.constants.emission.a;

    if (tri.material.indices.albedo > -1) {
        color *= texture(textures[nonuniformEXT(tri.material.indices.albedo)], tri.uv).rgb;
    }

    if (tri.material.indices.metallicRoughness > -1) {
        vec2 rm = texture(textures[nonuniformEXT(tri.material.indices.metallicRoughness)], tri.uv).gb;
        roughness *= rm.x;
        metallic  *= rm.y;
    }

    if (tri.material.indices.emissive > -1) {
        emission *= texture(textures[nonuniformEXT(tri.material.indices.emissive)], tri.uv).rgb;
    }

    vec3 worldPosition = vec3(gl_ObjectToWorldEXT * vec4(tri.hit, 1.0));
    vec3 worldNormalG = normalize(vec3(tri.normalG * gl_WorldToObjectEXT));
    vec3 worldNormalS = normalize(vec3(tri.normalS * gl_WorldToObjectEXT));
    vec4 worldTangent = vec4(normalize(vec3(tri.tangent.xyz * gl_WorldToObjectEXT)), tri.tangent.w);

    if (dot(-prd.direction, worldNormalG) < 0.0) { // TODO use faceforward()
        worldNormalG = -worldNormalG;
    }
    if (dot(worldNormalG, worldNormalS) < 0.0) { // TODO use faceforward()
        worldNormalS = -worldNormalS;
        worldTangent = -worldTangent;
    }

/*
    if (worldTangent.w == 1.0) {
        prd.radiance = vec3(1, 0, 1);
    }
    if (worldTangent.w == -1.0) {
        prd.radiance = vec3(0, 1, 1);
    }
    prd.radiance = (worldTangent.xyz);
    prd.done = 1;
    return;
*/

    //prd.origin = worldPosition;
    prd.origin = offsetPositionAlongNormal(worldPosition, worldNormalG);

    //prd.direction = sampleDiffuseTest(random(prd.seed), random(prd.seed), worldNormalS);
    vec3 dir = sampleCosineHemisphere(random(prd.seed), random(prd.seed));
    prd.direction = fromLocal(worldTangent, worldNormalS, dir); // TODO change toLocal

    // TODO check with geometric normal that new direction doesn't go inside object

    prd.radiance += prd.throughput * emission;
    prd.throughput *= color;
}