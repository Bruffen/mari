#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

namespace mari {
    extern PFN_vkGetBufferDeviceAddressKHR                vkGetBufferDeviceAddressKHR;
    extern PFN_vkCreateAccelerationStructureKHR           vkCreateAccelerationStructureKHR;              // TODO group these by extensions and features
    extern PFN_vkDestroyAccelerationStructureKHR          vkDestroyAccelerationStructureKHR;
    extern PFN_vkGetAccelerationStructureBuildSizesKHR    vkGetAccelerationStructureBuildSizesKHR;
    extern PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR;
    extern PFN_vkCmdBuildAccelerationStructuresKHR        vkCmdBuildAccelerationStructuresKHR;
    extern PFN_vkCmdTraceRaysKHR                          vkCmdTraceRaysKHR;
    extern PFN_vkGetRayTracingShaderGroupHandlesKHR       vkGetRayTracingShaderGroupHandlesKHR;
    extern PFN_vkCreateRayTracingPipelinesKHR             vkCreateRayTracingPipelinesKHR;

    void                                                  getRayTracingFunctionPointers(VkDevice device);

    namespace vkhelper {
        VkTransformMatrixKHR    glmToVkMatrix(const glm::mat4& matrix);

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
}