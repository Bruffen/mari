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

layout(binding = 3, set = 0) buffer BlasData { uint64_t addresses[]; } blasData;
layout(binding = 4, set = 0) uniform sampler2D textures[];

layout(buffer_reference, scalar) buffer Meshes   { Mesh m[];   };
layout(buffer_reference, scalar) buffer Vertices { Vertex v[]; };
layout(buffer_reference, scalar) buffer Indices  { uint i[];   };

Triangle unpackTriangle(uint index) {
    Triangle tri;

    uint64_t meshDeviceAddress = blasData.addresses[gl_InstanceID];

    Meshes meshes = Meshes(meshDeviceAddress);
    Mesh mesh = meshes.m[gl_GeometryIndexEXT];

    Indices indices   = Indices(mesh.indexBufferDeviceAddress);
    Vertices vertices = Vertices(mesh.vertexBufferDeviceAddress);

    for (uint i = 0; i < 3; i++) {
        tri.vertices[i] = vertices.v[indices.i[index * 3 + i]];
    }
    
    // Calculate values at barycentric coordinates
    vec3 barycentricCoords = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);
    tri.color    = tri.vertices[0].color    * barycentricCoords.x + tri.vertices[1].color    * barycentricCoords.y + tri.vertices[2].color    * barycentricCoords.z;
    tri.position = tri.vertices[0].position * barycentricCoords.x + tri.vertices[1].position * barycentricCoords.y + tri.vertices[2].position * barycentricCoords.z;
    tri.uv       = tri.vertices[0].uv       * barycentricCoords.x + tri.vertices[1].uv       * barycentricCoords.y + tri.vertices[2].uv       * barycentricCoords.z;
    tri.normal   = tri.vertices[0].normal   * barycentricCoords.x + tri.vertices[1].normal   * barycentricCoords.y + tri.vertices[2].normal   * barycentricCoords.z;
    tri.normal   = normalize(tri.normal);
    tri.material.textureIndex = mesh.textureIndex;

    return tri;
}

void main() {
    Triangle tri = unpackTriangle(gl_PrimitiveID);

    vec3 color = vec3(1.0);
    if (tri.material.textureIndex > -1) {
        color = texture(textures[nonuniformEXT(tri.material.textureIndex)], tri.uv).rgb;
    }

    const vec3 worldPosition = vec3(gl_ObjectToWorldEXT * vec4(tri.position, 1.0)); // Transforming the position to world space
    const vec3 worldNormal   = normalize(vec3(tri.normal * gl_WorldToObjectEXT));   // Transforming the normal to world space

    vec3 lightCol1 = vec3(1.0, 0.8, 0.5);
    vec3 lightDir1 = normalize(vec3(1.0, -1.0, 1.0));
    float diffuse1 = max(dot(worldNormal, -lightDir1), 0.05);
    vec3 lightCol2 = vec3(0.5, 0.8, 1.0);
    vec3 lightDir2 = normalize(vec3(-1.0, -0.5, -1.0));
    float diffuse2 = max(dot(worldNormal, -lightDir2), 0.05);

    prd.color = color * (lightCol1 * diffuse1 + lightCol2 * diffuse2);
}
