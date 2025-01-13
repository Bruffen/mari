#pragma once

#include "default_objects.hpp"

#include <glm/glm.hpp>
#include <array>

namespace mari {
    DefaultObjects::DefaultObjects(Device &device) : device{device} {
        uint32_t white   = glm::packUnorm4x8(glm::vec4(1, 1, 1, 1));
        uint32_t black   = glm::packUnorm4x8(glm::vec4(0, 0, 0, 0));
        uint32_t magenta = glm::packUnorm4x8(glm::vec4(1, 0, 1, 1));
        const uint32_t size = 16;
        std::array<uint32_t, size * size > pixels; //for 16x16 checkerboard texture
        for (int x = 0; x < size; x++) {
            for (int y = 0; y < size; y++) {
                pixels[y*size + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
            }
        }
        imageWhite = std::make_shared<Image>(
            device, 
            VkExtent3D{1, 1, 1}, 
            VK_FORMAT_R8G8B8A8_UNORM, 
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            (void*)&white
        );
        imageBlack = std::make_shared<Image>(
            device, 
            VkExtent3D{1, 1, 1}, 
            VK_FORMAT_R8G8B8A8_UNORM, 
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            (void*)&black
        );
        imageError = std::make_shared<Image>(
            device, 
            VkExtent3D{size, size, 1}, 
            VK_FORMAT_R8G8B8A8_UNORM, 
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            pixels.data()
        );

        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_NEAREST;
        samplerInfo.minFilter = VK_FILTER_NEAREST;
        vkCreateSampler(device.handle(), &samplerInfo, nullptr, &samplerNearest);
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        vkCreateSampler(device.handle(), &samplerInfo, nullptr, &samplerLinear);

    }

    DefaultObjects::~DefaultObjects() {
        vkDestroySampler(device.handle(), samplerNearest, nullptr);
        vkDestroySampler(device.handle(), samplerLinear, nullptr);
    }
}