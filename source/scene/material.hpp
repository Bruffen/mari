#pragma once

#include "image.hpp"

#include <glm/vec4.hpp>
#include <memory>

namespace mari {
    struct MaterialConstants {
        glm::vec4 color;
        float     metallic;
        float     roughness; 
    };
    
    struct MaterialResources {
        std::shared_ptr<Image>  colorImage;
        int32_t                 colorImageIndex;
        std::shared_ptr<Image>  metallicRoughnessImage;
        std::unique_ptr<Buffer> dataBuffer;
        uint32_t                dataBufferOffset;
    };
    
    struct Material {
        MaterialConstants constants;
        MaterialResources resources;
        std::string name = "";
    };

    struct SubMesh {
        uint32_t start;
        uint32_t count;
        std::shared_ptr<Material> material;
    };

    struct GeometryAdresses {
        uint64_t vertexBufferDeviceAddress;
        uint64_t indexBufferDeviceAddress;
        int32_t  textureIndex;
    };
}