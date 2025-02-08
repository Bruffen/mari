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

        VkAccelerationStructureBuildGeometryInfoKHR buildGeometryInfo{};
        buildGeometryInfo.sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        buildGeometryInfo.flags         = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        buildGeometryInfo.type          = type;
        buildGeometryInfo.geometryCount = geometryCount;
        buildGeometryInfo.pGeometries   = pGeometries;

        VkAccelerationStructureBuildSizesInfoKHR buildSizesInfo{};
        buildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
        vkGetAccelerationStructureBuildSizesKHR(
            device.handle(),
            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            &buildGeometryInfo,
            pMaxPrimitiveCounts,
            &buildSizesInfo
        );

        buffer = std::make_unique<Buffer>(
            device, 
            buildSizesInfo.accelerationStructureSize, 
            1, 
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );

        VkAccelerationStructureCreateInfoKHR createInfo{};
        createInfo.sType  = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
        createInfo.buffer = buffer->handle();
        createInfo.size   = buildSizesInfo.accelerationStructureSize;
        createInfo.type   = type;
        if (vkCreateAccelerationStructureKHR(device.handle(), &createInfo, nullptr, &handle)) {
            throw std::runtime_error("Could not create acceleration structure");
        };

        Buffer scratchBuffer {
            device, 
            buildSizesInfo.buildScratchSize,
            1,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        };

        buildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        buildGeometryInfo.dstAccelerationStructure = handle;
        buildGeometryInfo.scratchData.deviceAddress = scratchBuffer.deviceAddress();

        VkCommandBuffer cmdBuffer = device.beginSingleTimeCommands();
        vkCmdBuildAccelerationStructuresKHR(cmdBuffer, 1, &buildGeometryInfo, ppBuildRangeInfos);
        device.endSingleTimeCommands(cmdBuffer);

        VkAccelerationStructureDeviceAddressInfoKHR deviceAddressInfo{};
        deviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
        deviceAddressInfo.accelerationStructure = handle;
        deviceAddress = vkGetAccelerationStructureDeviceAddressKHR(device.handle(), &deviceAddressInfo);
    }
}