#pragma once

#include "ray_tracing_system.hpp"
#include "buffer.hpp"
#include "vk_helper.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include <array>
#include <cassert>
#include <stdexcept>

namespace mari {
    RayTracingSystem::RayTracingSystem(Device &device, Window &window, GameObject::Map &gameObjects, VkDescriptorSetLayout descriptorSetLayout) : device{device} {
        getRayTracingFunctionPointers(device.handle());
        createImages(window.getExtent().width, window.getExtent().height);
        buildScene(gameObjects);
        createPipelineLayout(descriptorSetLayout);
        createPipeline();
    }

    RayTracingSystem::~RayTracingSystem() {
        vkDestroyPipelineLayout(device.handle(), pipelineLayout, nullptr);
        vkDestroyAccelerationStructureKHR(device.handle(), tlas.handle, nullptr);
        for (auto &blas : blases) {
            vkDestroyAccelerationStructureKHR(device.handle(), blas.handle, nullptr);
        }
    }

    void RayTracingSystem::createImages(uint32_t width, uint32_t height) {
        accumImage   = std::make_unique<Image>(device, width, height, VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT);
        presentImage = std::make_unique<Image>(device, width, height, VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT);
    }

    void RayTracingSystem::buildScene(const GameObject::Map &scene) {
        // TODO reuse the same blas if the object's geometry is the same
        for (auto const &[id, object] : scene) {
            if (object.model) {
                buildBLAS(object);
            }
        }

        buildTLAS();
    }

    void RayTracingSystem::buildBLAS(const GameObject &object) {
        VkTransformMatrixKHR transformMatrix = object.transform.matKHR();

        AccelerationStructure blas{};

        Buffer stagingBuffer{
            device,
            sizeof(transformMatrix),
            1,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        };

        stagingBuffer.map();
        stagingBuffer.writeToBuffer((void*) &transformMatrix);

        auto transformBuffer = std::make_unique<Buffer>(
            device,
            sizeof(transformMatrix),
            1,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT
            | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );

        device.copyBuffer(stagingBuffer.handle(), transformBuffer->handle(), sizeof(transformMatrix));

        VkDeviceOrHostAddressConstKHR vertexBufferDeviceAddress{};
        VkDeviceOrHostAddressConstKHR indexBufferDeviceAddress{};
        VkDeviceOrHostAddressConstKHR transformBufferDeviceAddress{};
        
        vertexBufferDeviceAddress.deviceAddress = object.model->vertexBuffer->deviceAddress();
        indexBufferDeviceAddress.deviceAddress  = object.model->indexBuffer->deviceAddress();
        transformBufferDeviceAddress.deviceAddress = transformBuffer->deviceAddress();

        // Build
        VkAccelerationStructureGeometryKHR accelerationStructureGeometry{};
        accelerationStructureGeometry.sType                            = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        accelerationStructureGeometry.flags                            = VK_GEOMETRY_OPAQUE_BIT_KHR;
        accelerationStructureGeometry.geometryType                     = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
        accelerationStructureGeometry.geometry.triangles.sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
        accelerationStructureGeometry.geometry.triangles.vertexFormat  = VK_FORMAT_R32G32B32_SFLOAT;
        accelerationStructureGeometry.geometry.triangles.vertexData    = vertexBufferDeviceAddress;
        accelerationStructureGeometry.geometry.triangles.maxVertex     = object.model->vertexCount - 1;
        accelerationStructureGeometry.geometry.triangles.vertexStride  = sizeof(Model::Vertex);
        accelerationStructureGeometry.geometry.triangles.indexType     = VK_INDEX_TYPE_UINT32;
        accelerationStructureGeometry.geometry.triangles.indexData     = indexBufferDeviceAddress;
        accelerationStructureGeometry.geometry.triangles.transformData = transformBufferDeviceAddress;
        
        // Get size info
        VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo{};
        accelerationStructureBuildGeometryInfo.sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        accelerationStructureBuildGeometryInfo.type          = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        accelerationStructureBuildGeometryInfo.flags         = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        accelerationStructureBuildGeometryInfo.geometryCount = 1;
        accelerationStructureBuildGeometryInfo.pGeometries   = &accelerationStructureGeometry;

        const uint32_t numTriangles = object.model->indexCount / 3;
        VkAccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo{};
        accelerationStructureBuildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
        vkGetAccelerationStructureBuildSizesKHR(
            device.handle(),
            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            &accelerationStructureBuildGeometryInfo,
            &numTriangles,
            &accelerationStructureBuildSizesInfo);

        blas.buffer = std::make_unique<Buffer>(
            device, 
            accelerationStructureBuildSizesInfo.accelerationStructureSize, 
            1, 
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );

        VkAccelerationStructureCreateInfoKHR accelerationStructureCreateInfo{};
        accelerationStructureCreateInfo.sType  = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
        accelerationStructureCreateInfo.buffer = blas.buffer->handle();
        accelerationStructureCreateInfo.size   = accelerationStructureBuildSizesInfo.accelerationStructureSize;
        accelerationStructureCreateInfo.type   = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        if (vkCreateAccelerationStructureKHR(device.handle(), &accelerationStructureCreateInfo, nullptr, &blas.handle)) {
            throw std::runtime_error("Could not create blas");
        }

        ScratchBuffer scratchBuffer = ScratchBuffer::createScratchBuffer(device, accelerationStructureBuildSizesInfo.buildScratchSize);

        accelerationStructureBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        accelerationStructureBuildGeometryInfo.dstAccelerationStructure = blas.handle;
        accelerationStructureBuildGeometryInfo.scratchData.deviceAddress = scratchBuffer.deviceAddress;

        VkAccelerationStructureBuildRangeInfoKHR accelerationStructureBuildRangeInfo{};
        accelerationStructureBuildRangeInfo.primitiveCount  = numTriangles;
        accelerationStructureBuildRangeInfo.primitiveOffset = 0;
        accelerationStructureBuildRangeInfo.firstVertex     = 0;
        accelerationStructureBuildRangeInfo.transformOffset = 0;
        std::vector<VkAccelerationStructureBuildRangeInfoKHR *> accelerationStructureBuildRangeInfos = {&accelerationStructureBuildRangeInfo};

        VkCommandBuffer cmdBuffer = device.beginSingleTimeCommands();
        vkCmdBuildAccelerationStructuresKHR(cmdBuffer, 1, &accelerationStructureBuildGeometryInfo, accelerationStructureBuildRangeInfos.data());
        device.endSingleTimeCommands(cmdBuffer);

        ScratchBuffer::deleteScratchBuffer(device, scratchBuffer);

        VkAccelerationStructureDeviceAddressInfoKHR accelerationStructureDeviceAddressInfo{};
        accelerationStructureDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
        accelerationStructureDeviceAddressInfo.accelerationStructure = blas.handle;
        blas.deviceAddress = vkGetAccelerationStructureDeviceAddressKHR(device.handle(), &accelerationStructureDeviceAddressInfo);

        blases.emplace_back(std::move(blas));
    }

    void RayTracingSystem::buildTLAS() {
        VkTransformMatrixKHR transformMatrix = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f
        };

        std::vector<VkAccelerationStructureInstanceKHR> asInstances;
        asInstances.reserve(blases.size()); // Might need to change this when we reuse blases

        for (AccelerationStructure &blas : blases) {
            VkAccelerationStructureInstanceKHR accelerationStructureInstance{};
            accelerationStructureInstance.transform = transformMatrix;
            accelerationStructureInstance.instanceCustomIndex = 0;
            accelerationStructureInstance.mask = 0xFF;
            accelerationStructureInstance.instanceShaderBindingTableRecordOffset = 0;
            accelerationStructureInstance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
            accelerationStructureInstance.accelerationStructureReference = blas.deviceAddress;

            asInstances.push_back(accelerationStructureInstance);
        }

        VkDeviceSize asInstancesSize = sizeof(VkAccelerationStructureInstanceKHR) * asInstances.size();

        Buffer stagingBuffer{
            device,
            asInstancesSize,
            1,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        };

        stagingBuffer.map();
        stagingBuffer.writeToBuffer((void*) asInstances.data());

        auto instancesBuffer = std::make_unique<Buffer>(
            device,
            asInstancesSize,
            1,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT
            | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );

        device.copyBuffer(stagingBuffer.handle(), instancesBuffer->handle(), asInstancesSize);

        VkDeviceOrHostAddressConstKHR instancesBufferDeviceAddress{};
        instancesBufferDeviceAddress.deviceAddress = instancesBuffer->deviceAddress();

        VkAccelerationStructureGeometryKHR accelerationStructureGeometry{};
        accelerationStructureGeometry.sType                              = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        accelerationStructureGeometry.geometryType                       = VK_GEOMETRY_TYPE_INSTANCES_KHR;
        accelerationStructureGeometry.flags                              = VK_GEOMETRY_OPAQUE_BIT_KHR;
        accelerationStructureGeometry.geometry.instances.sType           = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
        accelerationStructureGeometry.geometry.instances.arrayOfPointers = VK_FALSE;
        accelerationStructureGeometry.geometry.instances.data            = instancesBufferDeviceAddress;

        VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo{};
        accelerationStructureBuildGeometryInfo.sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        accelerationStructureBuildGeometryInfo.type          = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        accelerationStructureBuildGeometryInfo.flags         = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        accelerationStructureBuildGeometryInfo.geometryCount = 1;
        accelerationStructureBuildGeometryInfo.pGeometries   = &accelerationStructureGeometry;

        const uint32_t primitiveCount = static_cast<uint32_t>(asInstances.size());
        
        VkAccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo{};
        accelerationStructureBuildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
        vkGetAccelerationStructureBuildSizesKHR(
            device.handle(),
            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            &accelerationStructureBuildGeometryInfo,
            &primitiveCount,
            &accelerationStructureBuildSizesInfo
        );

        tlas.buffer = std::make_unique<Buffer>(
            device, 
            accelerationStructureBuildSizesInfo.accelerationStructureSize, 
            1, 
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );

        VkAccelerationStructureCreateInfoKHR accelerationStructureCreateInfo{};
        accelerationStructureCreateInfo.sType  = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
        accelerationStructureCreateInfo.buffer = tlas.buffer->handle();
        accelerationStructureCreateInfo.size   = accelerationStructureBuildSizesInfo.accelerationStructureSize;
        accelerationStructureCreateInfo.type   = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        if (vkCreateAccelerationStructureKHR(device.handle(), &accelerationStructureCreateInfo, nullptr, &tlas.handle)) {
            throw std::runtime_error("Could not create tlas");
        };

        ScratchBuffer scratchBuffer = ScratchBuffer::createScratchBuffer(device, accelerationStructureBuildSizesInfo.buildScratchSize);

        accelerationStructureBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        accelerationStructureBuildGeometryInfo.dstAccelerationStructure = tlas.handle;
        accelerationStructureBuildGeometryInfo.scratchData.deviceAddress = scratchBuffer.deviceAddress;

        VkAccelerationStructureBuildRangeInfoKHR accelerationStructureBuildRangeInfo{};
        accelerationStructureBuildRangeInfo.primitiveCount  = primitiveCount;
        accelerationStructureBuildRangeInfo.primitiveOffset = 0;
        accelerationStructureBuildRangeInfo.firstVertex     = 0;
        accelerationStructureBuildRangeInfo.transformOffset = 0;
        std::vector<VkAccelerationStructureBuildRangeInfoKHR *> accelerationStructureBuildRangeInfos = {&accelerationStructureBuildRangeInfo};

        VkCommandBuffer cmdBuffer = device.beginSingleTimeCommands();
        vkCmdBuildAccelerationStructuresKHR(cmdBuffer, 1, &accelerationStructureBuildGeometryInfo, accelerationStructureBuildRangeInfos.data());
        device.endSingleTimeCommands(cmdBuffer);

        ScratchBuffer::deleteScratchBuffer(device, scratchBuffer);

        VkAccelerationStructureDeviceAddressInfoKHR accelerationStructureDeviceAddressInfo{};
        accelerationStructureDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
        accelerationStructureDeviceAddressInfo.accelerationStructure = tlas.handle;
        tlas.deviceAddress = vkGetAccelerationStructureDeviceAddressKHR(device.handle(), &accelerationStructureDeviceAddressInfo);
    }

    void RayTracingSystem::createPipelineLayout(VkDescriptorSetLayout descriptorSetLayout) {
        VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
        pipelineLayoutCreateInfo.sType                      = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutCreateInfo.setLayoutCount             = 1;
        pipelineLayoutCreateInfo.pSetLayouts                = &descriptorSetLayout;
        if (vkCreatePipelineLayout(device.handle(), &pipelineLayoutCreateInfo, nullptr, &pipelineLayout)) {
            throw std::runtime_error("Failed to create pipeline layout");
        }
    }

    void RayTracingSystem::createPipeline() {
        pipeline = std::make_unique<Pipeline>(device);
        pipeline->createRayTracingPipeline(pipelineLayout);
    }

    void RayTracingSystem::render(FrameInfo &frameInfo, Swapchain &swapchain) {
        uint32_t width  = swapchain.getSwapchainExtent().width;
        uint32_t height = swapchain.getSwapchainExtent().height;
        pipeline->bind(frameInfo.commandBuffer);

        vkCmdBindDescriptorSets(frameInfo.commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipelineLayout, 0, 1, &frameInfo.globalDescriptorSet, 0, 0);
        vkCmdTraceRaysKHR(frameInfo.commandBuffer, &pipeline->raygenSBTEntry, &pipeline->missSBTEntry, &pipeline->hitSBTEntry, &pipeline->callableSBTEntry, width, height, 1);

        VkImageSubresourceRange subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vkhelper::transitionImageLayout(
            frameInfo.commandBuffer, 
            swapchain.getImage(frameInfo.frameIndex), 
            VK_IMAGE_LAYOUT_UNDEFINED, 
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
        );

        vkhelper::transitionImageLayout(
            frameInfo.commandBuffer, 
            accumImage->handle, 
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            {},
            VK_ACCESS_TRANSFER_READ_BIT,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            subresourceRange
        );

        VkImageCopy imageCopy{};
        imageCopy.srcSubresource    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        imageCopy.srcOffset         = {0, 0, 0};
        imageCopy.dstSubresource    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        imageCopy.dstOffset         = {0, 0, 0};
        imageCopy.extent            = {width, height, 1};
        
        vkCmdCopyImage(
            frameInfo.commandBuffer, accumImage->handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            swapchain.getImage(frameInfo.frameIndex), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageCopy
        );

        vkhelper::transitionImageLayout(
            frameInfo.commandBuffer, 
            swapchain.getImage(frameInfo.frameIndex), 
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
        );

        vkhelper::transitionImageLayout(
            frameInfo.commandBuffer, 
            accumImage->handle, 
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 
            VK_ACCESS_TRANSFER_READ_BIT,
            {},
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_IMAGE_LAYOUT_GENERAL,
            subresourceRange
        );
    }
}