#version 460

#extension GL_GOOGLE_include_directive  : require

#include "common/raycommon.glsl"
#include "common/hitcommon.glsl"

layout(location = 0) rayPayloadInEXT Payload prd;
hitAttributeEXT vec2 attribs;

void main() {
    prd.instance_index  = gl_InstanceID;
    prd.geometry_index  = gl_GeometryIndexEXT;
    prd.primitive_index = gl_PrimitiveID;
    prd.t               = gl_HitTEXT;
    prd.barycentrics    = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);
    prd.hit_kind        = gl_HitKindEXT;
    prd.world_to_object = gl_WorldToObjectEXT;
    prd.object_to_world = gl_ObjectToWorldEXT;
}