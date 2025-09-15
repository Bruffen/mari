#version 460
#extension GL_GOOGLE_include_directive  : require

#include "common/raycommon.glsl"

layout(location = 0) rayPayloadInEXT Payload prd;

void main() {
    prd.instance_index = -1;
}