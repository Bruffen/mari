#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_nonuniform_qualifier : require

#include "raycommon.glsl"

layout(location = 0) rayPayloadInEXT payload prd;
hitAttributeEXT vec2 attribs;

struct GeometryNode {
    uint64_t vertexBufferDeviceAddress;
    uint64_t indexBufferDeviceAddress;
    int      textureIndex;
};

layout(binding = 3, set = 0) buffer GeometryNodes { GeometryNode nodes[]; } geometryNodes;
layout(binding = 4, set = 0) uniform sampler2D textures[];

struct Vertex {
    vec3 position;
    float pad0;
    vec4 color;
    vec3 normal;
    float pad1;
    vec2 uv;
    vec2 pad2;
};

struct Triangle {
    Vertex vertices[3];
    vec4 color;
    vec3 normal;
    vec2 uv;
};

layout(buffer_reference, scalar) buffer Vertices { Vertex v[]; };
layout(buffer_reference, scalar) buffer Indices  { uint i[];   };


Triangle unpackTriangle(uint index) {
    Triangle tri;

    GeometryNode geometryNode = geometryNodes.nodes[gl_GeometryIndexEXT];

    Indices indices   = Indices(geometryNode.indexBufferDeviceAddress);
    Vertices vertices = Vertices(geometryNode.vertexBufferDeviceAddress);

    // Unpack vertices
    // glm::vec3 position;
    // glm::vec4 color;
    // glm::vec3 normal;
    // glm::vec2 uv;
    for (uint i = 0; i < 3; i++) {
        /*const uint offset = indices.i[index * 3 + i] * 2;
        vec4 d0 = vertices.v[offset + 0]; // pos.xyz, color.r
        vec4 d1 = vertices.v[offset + 1]; // color.gba, n.x
        vec4 d2 = vertices.v[offset + 2]; // n.yz, uv.xy
        tri.vertices[i].pos = d0.xyz;
        tri.vertices[i].color = vec4(d0.w, d1.xyz);
        tri.vertices[i].normal = vec3(d1.w, d2.xy);
        tri.vertices[i].uv = d2.zw;*/

        tri.vertices[i] = vertices.v[indices.i[index * 3 + i]];
    }
    
    // Calculate values at barycentric coordinates
    vec3 barycentricCoords = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);
    tri.uv      = tri.vertices[0].uv     * barycentricCoords.x + tri.vertices[1].uv     * barycentricCoords.y + tri.vertices[2].uv     * barycentricCoords.z;
    tri.normal  = tri.vertices[0].normal * barycentricCoords.x + tri.vertices[1].normal * barycentricCoords.y + tri.vertices[2].normal * barycentricCoords.z;
    tri.color   = tri.vertices[0].color  * barycentricCoords.x + tri.vertices[1].color  * barycentricCoords.y + tri.vertices[2].color  * barycentricCoords.z;
    return tri;
}

void main()
{
    Triangle tri = unpackTriangle(gl_PrimitiveID);

    GeometryNode geometryNode = geometryNodes.nodes[gl_GeometryIndexEXT];

	vec3 color = texture(textures[nonuniformEXT(geometryNode.textureIndex)], tri.uv).rgb;

    vec3 lightCol1 = vec3(1.0, 0.8, 0.5);
    vec3 lightDir1 = normalize(vec3(-1.0, 1.0, -1.0));
    float diffuse1 = max(dot(tri.normal, lightDir1), 0.0);
    vec3 lightCol2 = vec3(0.5, 0.8, 1.0);
    vec3 lightDir2 = normalize(vec3(1.0, -0.5, -1.0));
    float diffuse2 = max(dot(tri.normal, lightDir2), 0.0);

	prd.color = color * (lightCol1 * diffuse1 + lightCol2 * diffuse2);
}
