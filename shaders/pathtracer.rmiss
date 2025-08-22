#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_nonuniform_qualifier : require

#include "common/raycommon.glsl"
#include "common/constants.glsl"
#include "common/math.glsl"

layout(binding = 6, set = 0) uniform sampler2D textures[];

layout(location = 0) rayPayloadInEXT Payload prd;

void main() {
    if (properties.nextEventEstimation == 1 && prd.depth != 0)
        return;

    vec3 color = vec3(1.0, 1.0, 1.0);

    vec3 dir = vec3(-prd.direction.x, prd.direction.z, -prd.direction.y);
    vec2 uv = equalAreaSphereToSquare(dir);

    if (infiniteLight.environmentID > -1) {
        color *= texture(textures[nonuniformEXT(infiniteLight.environmentID)], uv).rgb;
    }

    prd.radiance += prd.throughput * color;
    prd.done = 1;
}