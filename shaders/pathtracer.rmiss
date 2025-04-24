#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_nonuniform_qualifier : require

#include "common/raycommon.glsl"
#include "common/constants.glsl"

layout(location = 0) rayPayloadInEXT Payload prd;

layout(binding = 3, set = 0) uniform Properties {
    mat4 viewInverse;
    mat4 projInverse;
    int frameCount;
    int maxDepth;
    float exposure;
    int tonemapper;
    int russianRoulette;
    int environmentID;
} properties;
layout(binding = 5, set = 0) uniform sampler2D textures[];


void main()
{
    vec3 color = vec3(1.0, 1.0, 1.0);

    float u = (1.0 + atan(prd.direction.x, -prd.direction.z) * M_1_PI) * 0.5; // TODO create pi constants
    float v = acos(prd.direction.y) * M_1_PI;

    //color *= texture(environment, vec2(u, -v)).rgb;

    if (properties.environmentID > -1) {
        color *= texture(textures[nonuniformEXT(properties.environmentID)], vec2(u, -v)).rgb;
    }

    prd.radiance += prd.throughput * color;
    prd.done = 1;
}