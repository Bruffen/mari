#version 460

#extension GL_GOOGLE_include_directive : require

#include "common/raycommon.glsl"

layout(location = 1) rayPayloadInEXT ShadowPayload shadow_prd;

void main() {
    shadow_prd.visibility = true;
}