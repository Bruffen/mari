#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

struct PrimMeshInfo {
    uint64_t vertexBufferDeviceAddress;
    uint64_t indexBufferDeviceAddress;
    uint64_t materialBufferDeviceAddress;
};

struct Vertex {
    vec3 position;
    float pad0;
    vec4 color;
    vec3 normal;
    float pad1;
    vec2 uv;
    vec2 pad2;
}; 

struct MaterialConstants {
    vec4  albedo;
    float metallic;
    float roughness;
    float ior;
    float pad0;
    vec4  emission;   // rgb for color, a for strength
};

struct TextureIndices {
    int albedo;
    int metallicRoughness;
    int normal;
    int emissive;
    int anisotropy;
    int thickness;
    int iridescence;
    int clearcoat;
};

struct MaterialData {
    MaterialConstants constants;
    TextureIndices    indices;
};

struct Triangle {
    Vertex vertices[3];
    vec4 color;
    vec3 hit;
    vec3 normalS;       // Surface normal from vertex normal and normal map
    vec3 normalG;       // Geometric normal indicating where triangle is facing
    vec2 uv;
    MaterialData material;
};
