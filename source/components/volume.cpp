#pragma once

#include "volume.hpp"

namespace mari {
    Volume::Volume(Device &device) {

        VkExtent3D extent{10, 10, 10};

        std::vector<float> data{};
        for (uint32_t x = 0; x < extent.width; x++) {
            for (uint32_t y = 0; y < extent.width; y++) {
                for (uint32_t z = 0; z < extent.width; z++) {
                    data.push_back(1.0f);
                }
            }
        }

        image = std::make_unique<Image>(
            device, 
            extent, 
            VK_FORMAT_R32_SFLOAT, 
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            data.data()
        );
    }
}