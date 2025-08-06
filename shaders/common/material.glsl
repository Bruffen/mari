struct MaterialConstants {
    vec4  albedo;
    float metallic;
    float roughness;
    float ior;
    float thickness;
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