#pragma once

#include "acceleration_structure.hpp"

#include <stdexcept>

namespace mari {
    AccelerationStructure::AccelerationStructure(Device& device, VkAccelerationStructureTypeKHR type) : device{device}, type{type} {

    }

    AccelerationStructure::~AccelerationStructure() {
        vkDestroyAccelerationStructureKHR(device.handle(), handle, nullptr);
    }

    VkWriteDescriptorSetAccelerationStructureKHR AccelerationStructure::descriptor() {
        VkWriteDescriptorSetAccelerationStructureKHR descriptorSet{};
        descriptorSet.sType                      = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
        descriptorSet.accelerationStructureCount = 1;
        descriptorSet.pAccelerationStructures    = &handle;
        return descriptorSet;
    }

    void AccelerationStructure::build(const VkAccelerationStructureGeometryKHR *pGeometries, 
                                      const uint32_t geometryCount, 
                                      const uint32_t* pMaxPrimitiveCounts, 
                                      VkAccelerationStructureBuildRangeInfoKHR** ppBuildRangeInfos) {
        this->geometryCount = geometryCount;

        buildGeometryInfo.sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        buildGeometryInfo.flags         = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        buildGeometryInfo.type          = type;
        buildGeometryInfo.mode          = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        buildGeometryInfo.geometryCount = geometryCount;
        buildGeometryInfo.pGeometries   = pGeometries;

        if (type == VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR) {
            buildGeometryInfo.flags |= VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
        }

        VkAccelerationStructureBuildSizesInfoKHR buildSizesInfo{};
        buildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
        vkGetAccelerationStructureBuildSizesKHR(
            device.handle(),
            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            &buildGeometryInfo,
            pMaxPrimitiveCounts,
            &buildSizesInfo
        );

        if (!accelerationStructureBuffer || accelerationStructureBuffer->getBufferSize() < buildSizesInfo.accelerationStructureSize) {
            accelerationStructureBuffer = std::make_unique<Buffer>(
                device, 
                buildSizesInfo.accelerationStructureSize, 
                1, 
                VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
            );
        }

        VkAccelerationStructureCreateInfoKHR createInfo{};
        createInfo.sType  = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
        createInfo.buffer = accelerationStructureBuffer->handle();
        createInfo.size   = buildSizesInfo.accelerationStructureSize;
        createInfo.type   = type;
        if (vkCreateAccelerationStructureKHR(device.handle(), &createInfo, nullptr, &handle)) {
            throw std::runtime_error("Could not create acceleration structure");
        };

        if (!accelerationStructureScratchBuffer || accelerationStructureBuffer->getBufferSize() < buildSizesInfo.buildScratchSize) {
            accelerationStructureScratchBuffer = std::make_unique<Buffer>(
                device, 
                buildSizesInfo.buildScratchSize,
                1,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
            );
        }

        buildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        buildGeometryInfo.dstAccelerationStructure = handle;
        buildGeometryInfo.scratchData.deviceAddress = accelerationStructureScratchBuffer->deviceAddress();

        VkCommandBuffer cmdBuffer = device.beginSingleTimeCommands();
        vkCmdBuildAccelerationStructuresKHR(cmdBuffer, 1, &buildGeometryInfo, ppBuildRangeInfos);
        device.endSingleTimeCommands(cmdBuffer);

        VkAccelerationStructureDeviceAddressInfoKHR deviceAddressInfo{};
        deviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
        deviceAddressInfo.accelerationStructure = handle;
        deviceAddress = vkGetAccelerationStructureDeviceAddressKHR(device.handle(), &deviceAddressInfo);
    }

    void AccelerationStructure::update(const VkAccelerationStructureGeometryKHR *pGeometries, 
                                       VkAccelerationStructureBuildRangeInfoKHR** ppBuildRangeInfos) {
        assert(type == VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR && "Only top level acceleration structures can use this update call.");

        buildGeometryInfo.mode                      = VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR;
        buildGeometryInfo.srcAccelerationStructure  = handle,
        buildGeometryInfo.dstAccelerationStructure  = handle,
        buildGeometryInfo.geometryCount             = 1;
        buildGeometryInfo.pGeometries               = pGeometries;

        VkCommandBuffer cmdBuffer = device.beginSingleTimeCommands();

        VkMemoryBarrier preBarrier{};
        preBarrier.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        preBarrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
        preBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;

        vkCmdPipelineBarrier(
            cmdBuffer, 
            VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR | VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
            VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
            {},
            1,
            &preBarrier,
            0,
            nullptr,
            0,
            nullptr
        );

        vkCmdBuildAccelerationStructuresKHR(cmdBuffer, 1, &buildGeometryInfo, ppBuildRangeInfos);

        VkMemoryBarrier postBarrier{};
        postBarrier.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        postBarrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
        postBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(
            cmdBuffer, 
            VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
            VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
            {},
            1,
            &postBarrier,
            0,
            nullptr,
            0,
            nullptr
        );

        device.endSingleTimeCommands(cmdBuffer);
    }
}