#version 460
#extension GL_GOOGLE_include_directive : enable

#include "raycommon.glsl"

layout(location = 0) rayPayloadInEXT Payload prd;

void main()
{
    prd.radiance += prd.throughput * vec3(0.7, 0.9, 1.0) * 1.0;
    prd.done = 1;
}