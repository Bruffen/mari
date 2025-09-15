#ifndef _MATERIAL_GLSL_
#define _MATERIAL_GLSL_

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
    int metallic_roughness;
    int normal;
    int emissive;
    int anisotropy;
    int iridescence;
    int clearcoat;
};

struct MaterialData {
    MaterialConstants constants;
    TextureIndices    indices;
};

int get_material_type(MaterialConstants m) {
    // Dielectric
    if (m.thickness > 0.0) {
        return 1;
    } 
    
    // Conductor
    else if (m.metallic > 0.0) {
        return 2;
    } 
    
    // Diffuse 
    else {
        return 0;
    }
}

#endif // MATERIAL_GLSL_