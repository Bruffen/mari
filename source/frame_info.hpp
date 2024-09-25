#pragma once

#include "camera.hpp"

#include <vulkan/vulkan.h>

namespace mari {
    struct FrameInfo {
        int frameIndex;
        float frameTime;
        VkCommandBuffer commandBuffer;
        Camera &camera;
        VkDescriptorSet globalDescriptorSet;
    };
}