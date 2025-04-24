#version 460
#extension GL_GOOGLE_include_directive : enable

#include "raycommon.glsl"
#include "constants.glsl"

layout(location = 0) rayPayloadInEXT Payload prd;

layout(binding = 5, set = 0) uniform sampler2D environment;

void main()
{
    vec3 color = vec3(1.0, 1.0, 1.0);

    float u = (1.0 + atan(prd.direction.x, -prd.direction.z) * M_1_PI) * 0.5; // TODO create pi constants
    float v = acos(prd.direction.y) * M_1_PI;

    color *= texture(environment, vec2(u, -v)).rgb;
    prd.radiance += prd.throughput * color;
    prd.done = 1;
}