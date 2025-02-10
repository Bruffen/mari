#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_nonuniform_qualifier : require

#include "random.glsl"
#include "raycommon.glsl"
#include "hitcommon.glsl"

layout(location = 0) rayPayloadInEXT Payload prd;
hitAttributeEXT vec2 attribs;

layout(binding = 3, set = 0) buffer PPrimMeshInfos { uint64_t addresses[]; } pPrimMeshInfos;
layout(binding = 4, set = 0) uniform sampler2D textures[];

layout(buffer_reference, scalar) buffer PrimMeshInfos { PrimMeshInfo p[];   };
layout(buffer_reference, scalar) buffer Vertices      { Vertex v[];         };
layout(buffer_reference, scalar) buffer Indices       { uint i[];           };
layout(buffer_reference, scalar) buffer Materials     { MaterialData m[];   };

Triangle unpackTriangle(uint index) {
    Triangle tri;

    uint64_t meshDeviceAddress  = pPrimMeshInfos.addresses[gl_InstanceID];

    PrimMeshInfos primMeshInfos = PrimMeshInfos(meshDeviceAddress);
    PrimMeshInfo  primMeshInfo  = primMeshInfos.p[gl_GeometryIndexEXT];

    Indices indices     = Indices(primMeshInfo.indexBufferDeviceAddress);
    Vertices vertices   = Vertices(primMeshInfo.vertexBufferDeviceAddress);
    Materials materials = Materials(primMeshInfo.materialBufferDeviceAddress);

    for (uint i = 0; i < 3; i++) {
        tri.vertices[i] = vertices.v[indices.i[index * 3 + i]];
    }
    
    vec3 barycentricCoords = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);
    tri.color   = tri.vertices[0].color    * barycentricCoords.x + tri.vertices[1].color    * barycentricCoords.y + tri.vertices[2].color    * barycentricCoords.z;
    tri.hit     = tri.vertices[0].position * barycentricCoords.x + tri.vertices[1].position * barycentricCoords.y + tri.vertices[2].position * barycentricCoords.z;
    tri.uv      = tri.vertices[0].uv       * barycentricCoords.x + tri.vertices[1].uv       * barycentricCoords.y + tri.vertices[2].uv       * barycentricCoords.z;
    tri.normalS = tri.vertices[0].normal   * barycentricCoords.x + tri.vertices[1].normal   * barycentricCoords.y + tri.vertices[2].normal   * barycentricCoords.z;
    tri.normalS = normalize(tri.normalS);
    tri.material = materials.m[0];

    tri.normalG = normalize(cross(tri.vertices[1].position - tri.vertices[0].position, tri.vertices[2].position - tri.vertices[0].position));

    return tri;
}

void main() {
    Triangle tri = unpackTriangle(gl_PrimitiveID);

    vec3  color     = tri.material.constants.albedo.rgb;
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

    const vec3 worldPosition = vec3(gl_ObjectToWorldEXT * vec4(tri.hit, 1.0));      // Transforming the position to world space
    const vec3 worldNormalG = normalize(vec3(tri.normalG * gl_WorldToObjectEXT));   // Transforming the geometric normal to world space
    vec3 worldNormalS = normalize(vec3(tri.normalS * gl_WorldToObjectEXT));         // Transforming the surface normal to world space

    if (dot(-prd.direction, worldNormalG) < 0.0) {
        worldNormalS = -worldNormalS;
    }

    prd.origin = worldPosition;
    prd.direction = normalize(worldNormalS + vec3(vec3(rnd(prd.seed) * 2.0 - 1.0, rnd(prd.seed) * 2.0 - 1.0, rnd(prd.seed) * 2.0 - 1.0) * gl_WorldToObjectEXT)); // Non realistic new direction

    // TODO check with geometric normal that new direction doesn't go inside object


    const float d = dot(prd.direction, worldNormalS);
    prd.radiance += prd.throughput * emission;
    prd.throughput *= color * d;
}
