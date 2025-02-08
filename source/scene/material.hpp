#pragma once

#include "image.hpp"

#include <glm/vec4.hpp>
#include <memory>

namespace mari {
    struct MaterialConstants {
        glm::vec4                   albedo;
        float                       metallic;
        float                       roughness;
        float                       ior;
        alignas(16) glm::vec4       emission;   // rgb for color, a for strength
    };

    struct MaterialResources {
        std::shared_ptr<Image>      albedoImage;
        std::shared_ptr<Image>      metallicRoughnessImage;
    };

    struct TextureIndices {
        int32_t                     albedo      = -1;
        int32_t                     normal      = -1;
        int32_t                     occlusion   = -1;
        int32_t                     emission    = -1;
    };
    
    struct MaterialData {
        MaterialConstants           constants;
        TextureIndices              indices;
    };

    struct Material {
        std::string                 name = "";
        int32_t                     index;
        MaterialData                data;
        MaterialResources           resources;
        bool                        transparent = false;
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