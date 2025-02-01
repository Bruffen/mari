#version 460
#extension GL_GOOGLE_include_directive : enable

#include "raycommon.glsl"

layout(location = 0) rayPayloadInEXT Payload prd;

void main()
{
    prd.color = vec3(0.1, 0.1, 0.1);
}