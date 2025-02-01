#pragma once

#include "image.hpp"

#include <glm/vec4.hpp>
#include <memory>

namespace mari {
    struct MaterialConstants {
        glm::vec4               albedo;
        float                   metallic;
        float                   roughness;
        float                   ior;
        glm::vec4               emission;   // rgb for color, a for strength
    };

    struct MaterialResources {
        std::shared_ptr<Image>  albedoImage;
        std::shared_ptr<Image>  metallicRoughnessImage;
    };

    struct TextureIndices {
        int32_t                 albedo      = -1;
        int32_t                 normal      = -1;
        int32_t                 occlusion   = -1;
        int32_t                 emission    = -1;
    };

    struct Material {
        MaterialConstants constants;
        MaterialResources resources;
        TextureIndices    indices;
        std::string name = "";
    };

    struct SubMesh {
        uint32_t start;
        uint32_t count;
        std::shared_ptr<Material> material;
    };

    struct SubMeshAdresses {
        uint64_t vertexBufferDeviceAddress;
        uint64_t indexBufferDeviceAddress;
        int32_t  textureIndex;
    };
}