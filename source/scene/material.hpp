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
        VkSampler               colorSampler;
        std::shared_ptr<Image>  metallicRoughnessImage;
        VkSampler               metallicRoughnessSampler;
        std::unique_ptr<Buffer> dataBuffer;
        uint32_t                dataBufferOffset;
    };
    
    struct Material {
        MaterialConstants constants;
        MaterialResources resources;
    };

    struct Primitive {
        uint32_t startIndex;
        uint32_t count;
        std::shared_ptr<Material> material;
    };
}