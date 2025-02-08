#pragma once

#include "vk_helper.hpp"

#include <cassert>

namespace mari {
    PFN_vkGetBufferDeviceAddressKHR                vkGetBufferDeviceAddressKHR;
    PFN_vkCreateAccelerationStructureKHR           vkCreateAccelerationStructureKHR;
    PFN_vkDestroyAccelerationStructureKHR          vkDestroyAccelerationStructureKHR;
    PFN_vkGetAccelerationStructureBuildSizesKHR    vkGetAccelerationStructureBuildSizesKHR;
    PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR;
    PFN_vkCmdBuildAccelerationStructuresKHR        vkCmdBuildAccelerationStructuresKHR;
    PFN_vkBuildAccelerationStructuresKHR           vkBuildAccelerationStructuresKHR;
    PFN_vkCmdTraceRaysKHR                          vkCmdTraceRaysKHR;
    PFN_vkGetRayTracingShaderGroupHandlesKHR       vkGetRayTracingShaderGroupHandlesKHR;
    PFN_vkCreateRayTracingPipelinesKHR             vkCreateRayTracingPipelinesKHR;

    void getRayTracingFunctionPointers(VkDevice device) {
        vkGetBufferDeviceAddressKHR                 = reinterpret_cast<PFN_vkGetBufferDeviceAddressKHR>(
            vkGetDeviceProcAddr(device, "vkGetBufferDeviceAddressKHR"));
        vkCmdBuildAccelerationStructuresKHR         = reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(
            vkGetDeviceProcAddr(device, "vkCmdBuildAccelerationStructuresKHR"));
        vkBuildAccelerationStructuresKHR            = reinterpret_cast<PFN_vkBuildAccelerationStructuresKHR>(
            vkGetDeviceProcAddr(device, "vkBuildAccelerationStructuresKHR"));
        vkCreateAccelerationStructureKHR            = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(
            vkGetDeviceProcAddr(device, "vkCreateAccelerationStructureKHR"));
        vkDestroyAccelerationStructureKHR           = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(
            vkGetDeviceProcAddr(device, "vkDestroyAccelerationStructureKHR"));
        vkGetAccelerationStructureBuildSizesKHR     = reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(
            vkGetDeviceProcAddr(device, "vkGetAccelerationStructureBuildSizesKHR"));
        vkGetAccelerationStructureDeviceAddressKHR  = reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(
            vkGetDeviceProcAddr(device, "vkGetAccelerationStructureDeviceAddressKHR"));
        vkCmdTraceRaysKHR                           = reinterpret_cast<PFN_vkCmdTraceRaysKHR>(
            vkGetDeviceProcAddr(device, "vkCmdTraceRaysKHR"));
        vkGetRayTracingShaderGroupHandlesKHR        = reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(
            vkGetDeviceProcAddr(device, "vkGetRayTracingShaderGroupHandlesKHR"));
        vkCreateRayTracingPipelinesKHR              = reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(
            vkGetDeviceProcAddr(device, "vkCreateRayTracingPipelinesKHR"));
    }

    namespace vkhelper {
        VkTransformMatrixKHR glmToVkMatrix(const glm::mat4& matrix) {
            VkTransformMatrixKHR transformMatrix{};
            glm::mat4 m = glm::transpose(matrix);
            memcpy(&transformMatrix, (void*)&m, sizeof(VkTransformMatrixKHR));
            return transformMatrix;
        }

        // Taken from vk_common.cpp in Vulkan Samples https://github.com/KhronosGroup/Vulkan-Samples
        VkAccessFlags getAccessFlags(VkImageLayout layout)
        {
            switch (layout)
            {
                case VK_IMAGE_LAYOUT_UNDEFINED:
                case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
                    return 0;
                case VK_IMAGE_LAYOUT_PREINITIALIZED:
                    return VK_ACCESS_HOST_WRITE_BIT;
                case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
                    return VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL:
                    return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                //case VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR:
                //    return VK_ACCESS_FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT_KHR;
                case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
                    return VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
                case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
                    return VK_ACCESS_TRANSFER_READ_BIT;
                case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
                    return VK_ACCESS_TRANSFER_WRITE_BIT;
                case VK_IMAGE_LAYOUT_GENERAL:
                    assert(false && "Don't know how to get a meaningful VkAccessFlags for VK_IMAGE_LAYOUT_GENERAL! Don't use it!");
                    return 0;
                default:
                    assert(false);
                    return 0;
            }
        }

        VkPipelineStageFlags getPipelineStageFlags(VkImageLayout layout) {
            switch (layout)
            {
                case VK_IMAGE_LAYOUT_UNDEFINED:
                    return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                case VK_IMAGE_LAYOUT_PREINITIALIZED:
                    return VK_PIPELINE_STAGE_HOST_BIT;
                case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
                case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
                    return VK_PIPELINE_STAGE_TRANSFER_BIT;
                case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
                    return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL:
                    return VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
                //case VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR:
                //    return VK_PIPELINE_STAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR;
                case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
                    return VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
                case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
                    return VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
                case VK_IMAGE_LAYOUT_GENERAL:
                    assert(false && "Don't know how to get a meaningful VkPipelineStageFlags for VK_IMAGE_LAYOUT_GENERAL! Don't use it!");
                    return 0;
                default:
                    assert(false);
                    return 0;
            }
        }

        void transitionImageLayout(VkCommandBuffer                commandBuffer,
                                VkImage                        image,
                                VkPipelineStageFlags           srcStageMask,
                                VkPipelineStageFlags           dstStageMask,
                                VkAccessFlags                  srcAccessMask,
                                VkAccessFlags                  dstAccessMask,
                                VkImageLayout                  oldLayout,
                                VkImageLayout                  newLayout,
                                VkImageSubresourceRange const &subresourceRange) {
            VkImageMemoryBarrier imageMemoryBarrier{};
            imageMemoryBarrier.sType                = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            imageMemoryBarrier.srcAccessMask        = srcAccessMask;
            imageMemoryBarrier.dstAccessMask        = dstAccessMask;
            imageMemoryBarrier.oldLayout            = oldLayout;
            imageMemoryBarrier.newLayout            = newLayout;
            imageMemoryBarrier.srcQueueFamilyIndex  = VK_QUEUE_FAMILY_IGNORED;
            imageMemoryBarrier.dstQueueFamilyIndex  = VK_QUEUE_FAMILY_IGNORED;
            imageMemoryBarrier.image                = image;
            imageMemoryBarrier.subresourceRange     = subresourceRange;

            vkCmdPipelineBarrier(commandBuffer, srcStageMask, dstStageMask, 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
        }

        void transitionImageLayout(VkCommandBuffer                commandBuffer,
                                VkImage                        image,
                                VkImageLayout                  oldLayout,
                                VkImageLayout                  newLayout,
                                VkImageSubresourceRange const &subresourceRange) {
            VkPipelineStageFlags srcStageMask = getPipelineStageFlags(oldLayout);
            VkPipelineStageFlags dstStageMask = getPipelineStageFlags(newLayout);
            VkAccessFlags srcAccessMask = getAccessFlags(oldLayout);
            VkAccessFlags dstAccessMask = getAccessFlags(newLayout);

            transitionImageLayout(commandBuffer, image, srcStageMask, dstStageMask, srcAccessMask, dstAccessMask, oldLayout, newLayout, subresourceRange);
        }
        
        void transitionImageLayout(VkCommandBuffer                commandBuffer,
                                VkImage                        image,
                                VkImageLayout                  oldLayout,
                                VkImageLayout                  newLayout) {
            VkImageSubresourceRange subresourceRange{};
            subresourceRange.aspectMask             = VK_IMAGE_ASPECT_COLOR_BIT;
            subresourceRange.baseMipLevel           = 0;
            subresourceRange.levelCount             = 1;
            subresourceRange.baseArrayLayer         = 0;
            subresourceRange.layerCount             = 1;

            transitionImageLayout(commandBuffer, image, oldLayout, newLayout, subresourceRange);
        }

    }
}