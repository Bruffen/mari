#version 460
#extension GL_GOOGLE_include_directive  : require
#extension GL_EXT_buffer_reference2     : require
#extension GL_EXT_nonuniform_qualifier  : require
#extension GL_EXT_debug_printf          : enable

layout(binding = 7, set = 0) uniform sampler2D textures[];

#include "common/raycommon.glsl"
#include "common/constants.glsl"
#include "common/math.glsl"
#include "common/light.glsl"

layout(location = 0) rayPayloadInEXT Payload prd;

void main() {
    float mis_weight = 1.0;
    vec3 dir = vec3(-prd.direction.x, prd.direction.z, -prd.direction.y);
    dir = rotateAroundAxis(dir, vec3(0.0f, 0.0f, 1.0f), -infiniteLight.environmentRotation.x);
    dir = rotateAroundAxis(dir, vec3(0.0f, 1.0f, 0.0f), -infiniteLight.environmentRotation.y);
    vec2 uv = equalAreaSphereToSquare(dir);

    if (!prd.dirac && prd.depth != 0 && properties.nextEventEstimation == 1) {
        mis_weight = powerHeuristic(prd.pdf, pdfLightInfinite(uv));
    }

    vec3 color = vec3(1.0, 1.0, 1.0);
    if (infiniteLight.environmentID > -1) {
        color *= texture(textures[nonuniformEXT(infiniteLight.environmentID)], uv).rgb;
    }

    if (!isnan(mis_weight)) prd.radiance += prd.throughput * color * mis_weight;
    prd.done = 1;
}