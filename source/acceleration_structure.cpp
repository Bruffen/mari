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
        VkWriteDescriptorSetAccelerationStructureKHR accelerationStructureSet{};
        accelerationStructureSet.sType                      = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
        accelerationStructureSet.accelerationStructureCount = 1;
        accelerationStructureSet.pAccelerationStructures    = &handle;
        return accelerationStructureSet;
    }

    void AccelerationStructure::build(const VkAccelerationStructureGeometryKHR *pGeometries, 
                                      const uint32_t geometryCount, 
                                      const uint32_t* pMaxPrimitiveCounts, 
                                      VkAccelerationStructureBuildRangeInfoKHR** ppBuildRangeInfos) {
        this->geometryCount = geometryCount;

        VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo{};
        accelerationStructureBuildGeometryInfo.sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        accelerationStructureBuildGeometryInfo.flags         = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        accelerationStructureBuildGeometryInfo.type          = type;
        accelerationStructureBuildGeometryInfo.geometryCount = geometryCount;
        accelerationStructureBuildGeometryInfo.pGeometries   = pGeometries;

        
        VkAccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo{};
        accelerationStructureBuildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
        vkGetAccelerationStructureBuildSizesKHR(
            device.handle(),
            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            &accelerationStructureBuildGeometryInfo,
            pMaxPrimitiveCounts,
            &accelerationStructureBuildSizesInfo
        );

        buffer = std::make_unique<Buffer>(
            device, 
            accelerationStructureBuildSizesInfo.accelerationStructureSize, 
            1, 
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );

        VkAccelerationStructureCreateInfoKHR accelerationStructureCreateInfo{};
        accelerationStructureCreateInfo.sType  = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
        accelerationStructureCreateInfo.buffer = buffer->handle();
        accelerationStructureCreateInfo.size   = accelerationStructureBuildSizesInfo.accelerationStructureSize;
        accelerationStructureCreateInfo.type   = type;
        if (vkCreateAccelerationStructureKHR(device.handle(), &accelerationStructureCreateInfo, nullptr, &handle)) {
            throw std::runtime_error("Could not create acceleration structure");
        };

        Buffer scratchBuffer {
            device, 
            accelerationStructureBuildSizesInfo.buildScratchSize,
            1,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        };

        accelerationStructureBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        accelerationStructureBuildGeometryInfo.dstAccelerationStructure = handle;
        accelerationStructureBuildGeometryInfo.scratchData.deviceAddress = scratchBuffer.deviceAddress();

        VkCommandBuffer cmdBuffer = device.beginSingleTimeCommands();
        vkCmdBuildAccelerationStructuresKHR(cmdBuffer, 1, &accelerationStructureBuildGeometryInfo, ppBuildRangeInfos);
        device.endSingleTimeCommands(cmdBuffer);

        VkAccelerationStructureDeviceAddressInfoKHR accelerationStructureDeviceAddressInfo{};
        accelerationStructureDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
        accelerationStructureDeviceAddressInfo.accelerationStructure = handle;
        deviceAddress = vkGetAccelerationStructureDeviceAddressKHR(device.handle(), &accelerationStructureDeviceAddressInfo);
    }
}