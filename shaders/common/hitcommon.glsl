#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_scalar_block_layout   : require

struct PrimMeshInfo {
    uint64_t vertexBufferDeviceAddress;
    uint64_t indexBufferDeviceAddress;
    uint64_t materialBufferDeviceAddress;
};

struct Vertex {
    vec3 position;
    vec4 tangent;
    vec3 normal;
    vec4 color;
    vec2 uv;
};

struct Triangle {
    Vertex vertices[3];
    vec4 color;
    vec3 hit;
    vec3 normalS;       // Shading normal from interpolated vertex normals and normal map
    vec3 normalG;       // Geometric normal indicating where triangle is facing
    vec4 tangent;
    vec2 uv;
    MaterialData material;
};

layout(buffer_reference, scalar) readonly buffer PrimMeshInfos { PrimMeshInfo p[]; };
layout(buffer_reference, scalar) readonly buffer Vertices      { Vertex v[];       };
layout(buffer_reference, scalar) readonly buffer Indices       { uint i[];         };
layout(buffer_reference, scalar) readonly buffer Materials     { MaterialData m[]; };

layout(binding = 5, set = 0) buffer PPrimMeshInfos { uint64_t addresses[]; } pPrimMeshInfos;
layout(binding = 6, set = 0) uniform sampler2D textures[];

layout(location = 0) rayPayloadInEXT Payload prd;
hitAttributeEXT vec2 attribs;

// Technique by Carsten Wächter and Nikolaus Binder 
// "A Fast and Robust Method for Avoiding Self-Intersection"
// The normal can be negated if one wants the ray to pass through the surface instead.
vec3 offsetPositionAlongNormal(vec3 position, vec3 normal)
{
    // Convert the normal to an integer offset.
    const float int_scale = 256.0;
    const ivec3 of_i      = ivec3(int_scale * normal);

    // Offset each component of worldPosition using its binary representation.
    // Handle the sign bits correctly.
    const vec3 p_i = vec3(  //
        intBitsToFloat(floatBitsToInt(position.x) + ((position.x < 0) ? -of_i.x : of_i.x)),
        intBitsToFloat(floatBitsToInt(position.y) + ((position.y < 0) ? -of_i.y : of_i.y)),
        intBitsToFloat(floatBitsToInt(position.z) + ((position.z < 0) ? -of_i.z : of_i.z))
    );

    // Use a floating-point offset instead for points near (0,0,0), the origin.
    const float origin     = 1.0 / 32.0;
    const float floatScale = 1.0 / 65536.0;
    return vec3(
        abs(position.x) < origin ? position.x + floatScale * normal.x : p_i.x,
        abs(position.y) < origin ? position.y + floatScale * normal.y : p_i.y,
        abs(position.z) < origin ? position.z + floatScale * normal.z : p_i.z
    );
}

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
    tri.normalG = normalize(cross(tri.vertices[1].position - tri.vertices[0].position, tri.vertices[2].position - tri.vertices[0].position));

    //tri.tangent.xyz = tri.vertices[0].tangent.xyz * barycentricCoords.x + tri.vertices[1].tangent.xyz * barycentricCoords.y + tri.vertices[2].tangent.xyz * barycentricCoords.z;
    //tri.tangent.xyz = normalize(tri.tangent.xyz);
    //tri.tangent.w   = tri.vertices[0].tangent.w;

    // TODO fix mikktspace tangents so we don't have to calculate them here
    vec3 up = abs(tri.normalS.z) < 0.99999 ? vec3(0, 0, 1) : vec3(1, 0, 0);
    tri.tangent.xyz = normalize(cross(up, tri.normalS));
    tri.tangent.w = 1.0;
    
    //tri.tangent.xyz = abs(tri.normalS.z) < 0.99999 ? normalize(cross(vec3(0, 0, 1), tri.normalS)) : vec3(1, 0, 0);
    //tri.tangent.xyz = cross(tri.normalS, tri.tangent.xyz);
    //tri.tangent.w = 1.0;

    tri.material = materials.m[0];

    return tri;
}


MaterialConstants getMaterial(Triangle tri) {
    MaterialConstants m = tri.material.constants;
    m.albedo     *= tri.color;
    m.emission   *= tri.material.constants.emission.a;

    if (tri.material.indices.albedo > -1) {
        m.albedo *= pow(texture(textures[nonuniformEXT(tri.material.indices.albedo)], tri.uv), vec4(2.2));
    }

    if (tri.material.indices.metallicRoughness > -1) {
        vec2 rm = texture(textures[nonuniformEXT(tri.material.indices.metallicRoughness)], tri.uv).gb;
        m.roughness *= rm.x;
        m.metallic  *= rm.y;
    }
    m.roughness = m.roughness * m.roughness;

    if (tri.material.indices.emissive > -1) {
        m.emission *= texture(textures[nonuniformEXT(tri.material.indices.emissive)], tri.uv);
    }

    return m;
}