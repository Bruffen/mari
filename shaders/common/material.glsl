#ifndef _MATERIAL_GLSL_
#define _MATERIAL_GLSL_

struct PhaseFunction {
    uint  type;
    float anisotropy;
    float particle_size;
};

struct Medium {
    vec3  albedo;
    float absorption;
    float scattering;
    PhaseFunction phase_function;
    bool  heterogeneous;
};

struct MaterialConstants {
    vec4   albedo;
    float  metallic;
    float  roughness;
    float  ior;
    float  thickness;
    Medium medium;
    vec4   emission;   // rgb for color, a for strength
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

#define MaterialType_Boundary -1
#define MaterialType_Diffuse 0
#define MaterialType_Dielectric 1
#define MaterialType_Conductor 2

int get_material_type(MaterialConstants m) {
    if (m.thickness > 0.0) {
        if (m.ior == 1.0) {
            return MaterialType_Boundary;
        } else {
            return MaterialType_Dielectric;
        }
    }
    if (m.metallic > 0.0) {
        return MaterialType_Conductor;
    } 
    
    return MaterialType_Diffuse;
}

#endif // MATERIAL_GLSL_