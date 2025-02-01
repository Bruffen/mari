#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

struct Mesh {
    uint64_t vertexBufferDeviceAddress;
    uint64_t indexBufferDeviceAddress;
    int      textureIndex;
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

struct Material {
    int textureIndex;
};

struct Triangle {
    Vertex vertices[3];
    vec4 color;
    vec3 position;
    vec3 normal;
    vec2 uv;
    Material material;
};
