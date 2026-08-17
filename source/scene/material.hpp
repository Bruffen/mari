#pragma once

#include "image.hpp"
#include "scene/participating_media.hpp"

#include <glm/vec4.hpp>
#include <memory>

namespace mari {
    struct MaterialTextures {
        // PBR Textures
        std::shared_ptr<Image>      albedo;
        std::shared_ptr<Image>      metallicRoughness;

        // Data textures
        std::shared_ptr<Image>      normal;
        std::shared_ptr<Image>      occlusion;
        std::shared_ptr<Image>      emissive;
        
        // Packed textures
        std::shared_ptr<Image>      packedRoughnessMetallicOcclusion;
        std::shared_ptr<Image>      packedOcclusionRoughnessMetallic;
        
        // Extensions
        // KHR_materials_anisotropy
        std::shared_ptr<Image>      anisotropy;
        
        // KHR_materials_specular
        std::shared_ptr<Image>      specular;
        std::shared_ptr<Image>      specularColor;
        
        // KHR_materials_iridescence
        std::shared_ptr<Image>      iridescence;
        std::shared_ptr<Image>      iridescenceThickness;

        // KHR_materials_transmission
        std::shared_ptr<Image>      transmission;
        
        // KHR_materials_volume
        // We are never going to need a volume thickness texture in a path tracer
        // The traced rays' travelled distance tells us the thickness
        // std::shared_ptr<Image>      thickness;

        // KHR_materials_clearcoat
        std::shared_ptr<Image>      clearcoat;
        std::shared_ptr<Image>      clearcoatRoughness;
        std::shared_ptr<Image>      clearcoatNormal;

        // KHR_materials_sheen
        std::shared_ptr<Image>      sheenColor;
        std::shared_ptr<Image>      sheenRoughness;

    };

    struct MaterialConstants {
        glm::vec4                   albedo;
        float                       metallic;
        float                       roughness;

        // KHR_materials_ior
        float                       ior = 1.5f;

        // KHR_materials_volume
        // Realistically, we do not need the glTF thicknessFactor value
        // yet a material with transmission of 1 can be both thin walled and a volume
        // and thickness value is used to tell them apart, so we set it to 1 when it's a volume
        float                       thickness = 0.0f;
        Medium                      medium;

        // KHR_materials_anisotropy // TODO convert from gltf anisotropy parameters to pbrt's
        //float                       anisotropyStrength = 0.0f;
        //float                       anisotropyRotation = 0.0f; 

        // rgb color, a strength
        glm::vec4                   emission;
        VkBool32                    doubleSidedEmission = false;
        
        // Allows shadow rays to pass through dielectric surfaces unaffected
        VkBool32                    dielectricNeeCheat = false;
        auto operator<=>(const MaterialConstants&) const = default;
    };

    struct TextureIndices {
        int32_t                     albedo              = -1;
        int32_t                     metallicRoughness   = -1;
        int32_t                     normal              = -1;
        int32_t                     emissive            = -1;
        int32_t                     anisotropy          = -1;
        int32_t                     iridescence         = -1;
        int32_t                     clearcoat           = -1;
    };
    
    struct MaterialData {
        MaterialConstants           constants;
        TextureIndices              indices;
    };

    struct Material {
        std::string                 name = "";
        int32_t                     index;
        MaterialData                data;
        MaterialTextures            textures;
        bool                        transparent = false;
        bool                        insideMedia = false; // Only for lights, for transmittance calculation. // TODO Think of a better way

        bool                        isEmissive() const { 
            return glm::length(
                data.constants.emission.a * 
                glm::vec3(data.constants.emission)
            ) > 0.0f; 
        }
    };

    struct PrimMesh {
        uint32_t                    start;
        uint32_t                    count;
        std::shared_ptr<Material>   material;
    };

    struct PrimMeshInfo {
        uint64_t                    vertexBufferDeviceAddress;
        uint64_t                    indexBufferDeviceAddress;
        uint64_t                    materialBufferDeviceAddress;
    };
}