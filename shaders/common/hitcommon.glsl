#ifndef _HIT_COMMON_GLSL_
#define _HIT_COMMON_GLSL_

#extension GL_EXT_buffer_reference2                         : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64    : require
#extension GL_EXT_scalar_block_layout                       : require
#extension GL_EXT_nonuniform_qualifier                      : require

#include "common/raycommon.glsl"
#include "common/material.glsl"

struct PrimMeshInfo {
    uint64_t vertices_bda;
    uint64_t indices_bda;
    uint64_t material_bda;
};

struct Vertex {
    vec3 position;
    vec4 tangent;
    vec3 normal;
    vec4 color;
    vec2 uv;
};

struct Hit {
    vec3 vertices[3];
    vec3 position;
    vec3 normal_s;       // Shading normal from interpolated vertex normals and normal map
    vec3 normal_g;       // Geometric normal indicating the direction the triangle is facing
    vec3 tangent;
    vec3 bitangent;
    int  material_type;
    MaterialConstants material;
};

layout(buffer_reference, scalar) readonly buffer PrimMeshInfos { PrimMeshInfo p[]; };
layout(buffer_reference, scalar) readonly buffer Vertices      { Vertex v[];       };
layout(buffer_reference, scalar) readonly buffer Indices       { uint i[];         };
layout(buffer_reference, scalar) readonly buffer Materials     { MaterialData m[]; };

layout(binding = 7, set = 0) buffer PPrimMeshInfos { uint64_t addresses[]; } pPrimMeshInfos;
layout(binding = 8, set = 0) uniform sampler2D textures[];

// Technique by Carsten Wächter and Nikolaus Binder 
// "A Fast and Robust Method for Avoiding Self-Intersection"
// The normal can be negated if one wants the ray to pass through the surface instead.
vec3 offset_position(vec3 position, vec3 normal)
{
    // Convert the normal to an integer offset.
    const float int_scale = 256.0;
    const ivec3 of_i      = ivec3(int_scale * normal);

    // Offset each component of worldPosition using its binary representation.
    // Handle the sign bits correctly.
    const vec3 p_i = vec3(
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

MaterialData get_material(uint64_t bda) {
    return Materials(bda).m[0];
}

MaterialConstants process_material(MaterialData data, vec4 color, vec2 uv) {
    MaterialConstants mc = data.constants;
    mc.albedo     *= color;
    mc.emission   *= mc.emission.a;

    if (data.indices.albedo > -1) {
        mc.albedo *= pow(texture(textures[nonuniformEXT(data.indices.albedo)], uv), vec4(2.2));
    }

    if (data.indices.metallic_roughness > -1) {
        vec2 rm = texture(textures[nonuniformEXT(data.indices.metallic_roughness)], uv).gb;
        mc.roughness *= rm.x;
        mc.metallic  *= rm.y;
    }
    mc.roughness = mc.roughness * mc.roughness;

    if (data.indices.emissive > -1) {
        mc.emission *= texture(textures[nonuniformEXT(data.indices.emissive)], uv);
    }

    return mc;
}

Hit process_hit(Payload payload) {
    Hit hit;

    // Get necessary buffers to get triangle information 
    uint64_t mesh_bda  = pPrimMeshInfos.addresses[payload.instance_index];
    PrimMeshInfo prim_mesh = PrimMeshInfos(mesh_bda).p[payload.geometry_index];

    Indices  indices_buffer  = Indices(prim_mesh.indices_bda);
    Vertices vertices_buffer = Vertices(prim_mesh.vertices_bda);

    Vertex vertices[3];
    for (uint i = 0; i < 3; i++) {
        vertices[i] = vertices_buffer.v[indices_buffer.i[payload.primitive_index * 3 + i]];
        hit.vertices[i] = vertices[i].position;
    }

    // Interpolate data with barycentric coordinates
    hit.position   = vertices[0].position * payload.barycentrics.x + vertices[1].position * payload.barycentrics.y + vertices[2].position * payload.barycentrics.z;
    hit.normal_s   = vertices[0].normal   * payload.barycentrics.x + vertices[1].normal   * payload.barycentrics.y + vertices[2].normal   * payload.barycentrics.z;
    //vec4 tangent   = vertices[0].tangent  * payload.barycentrics.x + vertices[1].tangent  * payload.barycentrics.y + vertices[2].tangent  * payload.barycentrics.z;
    vec4 hit_color = vertices[0].color    * payload.barycentrics.x + vertices[1].color    * payload.barycentrics.y + vertices[2].color    * payload.barycentrics.z;
    vec2 hit_uv    = vertices[0].uv       * payload.barycentrics.x + vertices[1].uv       * payload.barycentrics.y + vertices[2].uv       * payload.barycentrics.z;
    hit.normal_g   = normalize(cross(vertices[1].position - vertices[0].position, vertices[2].position - vertices[0].position));
    hit.normal_s   = normalize(hit.normal_s);
    //hit.tangent    = normalize(tangent.xyz);
    //float fsign    = sign(tangent.w);

    // Alternative to mikktspace tangents
    vec3 up = abs(hit.normal_s.z) < 0.99999 ? vec3(0, 0, 1) : vec3(1, 0, 0);
    hit.tangent = normalize(cross(up, hit.normal_s));
    float fsign = 1.0;

    // Transform information into world space
    hit.position  = vec3(payload.object_to_world * vec4(hit.position, 1.0));
    hit.normal_s  = normalize((hit.normal_s * payload.world_to_object).xyz);
    hit.normal_g  = normalize((hit.normal_g * payload.world_to_object).xyz);
    hit.tangent   = normalize((hit.tangent  * payload.world_to_object).xyz);
    hit.bitangent = cross(hit.normal_s, hit.tangent) * fsign;

    // Process material
    MaterialData material_data = get_material(prim_mesh.material_bda);
    hit.material = process_material(material_data, hit_color, hit_uv);
    hit.material_type = get_material_type(hit.material);

    // Apply normal mapping
    if (material_data.indices.normal > -1) {
        mat3 tbn = mat3(hit.tangent, hit.bitangent, hit.normal_s);
        
        vec3 normal = texture(textures[nonuniformEXT(material_data.indices.normal)], hit_uv).xyz;
        normal = normal * 2.0 - vec3(1.0);
        // Flip from glTF +Y up convention to -Y up
        normal.y = -normal.y;

        // TODO Implement Microfacet-based Normal Mapping

        hit.normal_s  = normalize(tbn * normal);
        hit.tangent   = normalize(hit.tangent - dot(hit.tangent, hit.normal_s) * hit.normal_s);
        hit.bitangent = cross(hit.normal_s, hit.tangent);
    }

    return hit;
}

void flip_orientation(inout Hit hit) {
    hit.normal_g  = -hit.normal_g;
    hit.normal_s  = -hit.normal_s;
    hit.tangent   = -hit.tangent;
    hit.bitangent = -hit.bitangent;
}

#endif // _HIT_COMMON_GLSL_