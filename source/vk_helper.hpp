#pragma once

#include <vulkan/vulkan.h>

namespace vkhelper {

    VkAccessFlags           getAccessFlags(VkImageLayout layout);
    VkPipelineStageFlags    getPipelineStageFlags(VkImageLayout layout);

    void                    transitionImageLayout(
                                VkCommandBuffer                commandBuffer,
                                VkImage                        image,
                                VkPipelineStageFlags           srcStageMask,
                                VkPipelineStageFlags           dstStageMask,
                                VkAccessFlags                  srcAccessMask,
                                VkAccessFlags                  dstAccessMask,
                                VkImageLayout                  oldLayout,
                                VkImageLayout                  newLayout,
                                VkImageSubresourceRange const &subresourceRange);

    void                    transitionImageLayout(
                                VkCommandBuffer                commandBuffer,
                                VkImage                        image,
                                VkImageLayout                  oldLayout,
                                VkImageLayout                  newLayout,
                                VkImageSubresourceRange const &subresourceRange);

    void                    transitionImageLayout(
                                VkCommandBuffer                commandBuffer,
                                VkImage                        image,
                                VkImageLayout                  oldLayout,
                                VkImageLayout                  newLayout);


}