#pragma once

#include "ray_tracing_system.hpp"
#include "buffer.hpp"
#include "vk_helper.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include <stdexcept>



#include <iostream>
#include <glm/glm.hpp>

namespace mari {
    RayTracingSystem::RayTracingSystem(Device &device, Window &window) : device{device} {
        createImages(window.getExtent().width, window.getExtent().height);
    }

    RayTracingSystem::~RayTracingSystem() {
        vkDestroyPipelineLayout(device.handle(), pipelineLayout, nullptr);
    }

    void RayTracingSystem::createImages(uint32_t width, uint32_t height) {
        accumImage   = std::make_unique<Image>(device, VkExtent3D{width, height, 1}, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_STORAGE_BIT, VK_IMAGE_LAYOUT_GENERAL);
        presentImage = std::make_unique<Image>(device, VkExtent3D{width, height, 1}, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_IMAGE_LAYOUT_GENERAL);
    }

    void RayTracingSystem::buildScene(const Scene &scene) {
        // TODO reuse the same blas if the object's geometry is the same
        for (auto const &[id, object] : scene.nodes) {
            if (object->mesh) {
                VkTransformMatrixKHR transformMatrix = vkhelper::glmToVkMatrix(object->worldMatrix);
                buildBLAS(*object->mesh, transformMatrix, scene.materialDataBuffer->deviceAddress());
            }
        }
        
        VkDeviceSize primMeshesAdressesBufferSize = sizeof(PrimMeshInfo) * primMeshesInfos.size();

        primMeshesInfosBuffer = std::make_unique<Buffer>(
            device,
            primMeshesAdressesBufferSize,
            1,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
            | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );
        primMeshesInfosBuffer->stageToBuffer((void*) primMeshesInfos.data());

        std::vector<uint64_t> pPrimMeshesAdresses{};
        pPrimMeshesAdresses.reserve(blases.size());

        uint64_t start = primMeshesInfosBuffer->deviceAddress();
        for (auto& blas : blases) {
            pPrimMeshesAdresses.push_back(start);
            start += sizeof(PrimMeshInfo) * blas->geometryCount;
        }

        VkDeviceSize pPrimMeshesAdressesBufferSize = sizeof(uint64_t) * pPrimMeshesAdresses.size();

        pPrimMeshesInfosBuffer = std::make_unique<Buffer>(
            device,
            pPrimMeshesAdressesBufferSize,
            1,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
            | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );
        pPrimMeshesInfosBuffer->stageToBuffer((void*) pPrimMeshesAdresses.data());

        buildTLAS();
    }

    void RayTracingSystem::buildBLAS(const Mesh &mesh, VkTransformMatrixKHR transformMatrix, uint64_t materialBufferDeviceAddress) {
        auto blas = std::make_unique<AccelerationStructure>(device, VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR);
        blas->transformMatrix = transformMatrix;

        std::vector<uint32_t> triangleCounts{};
        std::vector<VkAccelerationStructureGeometryKHR> geometries{};
        std::vector<VkAccelerationStructureBuildRangeInfoKHR> buildRangeInfos{};
        std::vector<VkAccelerationStructureBuildRangeInfoKHR *> pBuildRangeInfos{};

        triangleCounts.reserve(mesh.primMeshes.size());
        geometries.reserve(mesh.primMeshes.size());
        buildRangeInfos.reserve(mesh.primMeshes.size());
        pBuildRangeInfos.reserve(mesh.primMeshes.size());

        // Create geometries per primMesh so we can index materials by gl_GeometryIndexEXT
        for (const PrimMesh &primMesh : mesh.primMeshes) {
            // Set device adresses for buffers
            VkDeviceOrHostAddressConstKHR vertexBufferDeviceAddress{};
            VkDeviceOrHostAddressConstKHR indexBufferDeviceAddress{};
            
            vertexBufferDeviceAddress.deviceAddress = mesh.vertexBuffer->deviceAddress();
            indexBufferDeviceAddress.deviceAddress  = mesh.indexBuffer->deviceAddress() + primMesh.start * sizeof(uint32_t);

            // Set geometry info
            VkAccelerationStructureGeometryKHR accelerationStructureGeometry{};
            accelerationStructureGeometry.sType                            = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
            accelerationStructureGeometry.flags                            = primMesh.material->transparent ? 0 : VK_GEOMETRY_OPAQUE_BIT_KHR;
            accelerationStructureGeometry.geometryType                     = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
            accelerationStructureGeometry.geometry.triangles.sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
            accelerationStructureGeometry.geometry.triangles.vertexFormat  = VK_FORMAT_R32G32B32_SFLOAT;
            accelerationStructureGeometry.geometry.triangles.vertexData    = vertexBufferDeviceAddress;
            accelerationStructureGeometry.geometry.triangles.vertexStride  = sizeof(Mesh::Vertex);
            accelerationStructureGeometry.geometry.triangles.maxVertex     = mesh.vertexCount - 1;
            accelerationStructureGeometry.geometry.triangles.indexType     = VK_INDEX_TYPE_UINT32;
            accelerationStructureGeometry.geometry.triangles.indexData     = indexBufferDeviceAddress;            

            // Set build size info
            const uint32_t triangleCount = primMesh.count / 3;
            VkAccelerationStructureBuildRangeInfoKHR accelerationStructureBuildRangeInfo{};
            accelerationStructureBuildRangeInfo.primitiveCount  = triangleCount;
            accelerationStructureBuildRangeInfo.primitiveOffset = 0;
            accelerationStructureBuildRangeInfo.firstVertex     = 0;
            accelerationStructureBuildRangeInfo.transformOffset = 0;

            // Add data back to vectors
            geometries.push_back(accelerationStructureGeometry);
            triangleCounts.push_back(triangleCount);
            buildRangeInfos.push_back(accelerationStructureBuildRangeInfo);
            pBuildRangeInfos.push_back(&buildRangeInfos.back());                // only works if .reserve() was called properly

            PrimMeshInfo primMeshAddresses{};
            primMeshAddresses.vertexBufferDeviceAddress = vertexBufferDeviceAddress.deviceAddress;
            primMeshAddresses.indexBufferDeviceAddress  = indexBufferDeviceAddress.deviceAddress;
            primMeshAddresses.materialBufferDeviceAddress = materialBufferDeviceAddress + primMesh.material->index * sizeof(MaterialData);
            primMeshesInfos.push_back(primMeshAddresses);
        }

        blas->build(geometries.data(), static_cast<uint32_t>(geometries.size()), triangleCounts.data(), pBuildRangeInfos.data());
        blases.emplace_back(std::move(blas));
    }

    void RayTracingSystem::buildTLAS() {
        tlas = std::make_unique<AccelerationStructure>(device, VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR);

        std::vector<VkAccelerationStructureInstanceKHR> asInstances;
        asInstances.reserve(blases.size()); // Might need to change this when we reuse blases

        int i = 0;
        for (auto& blas : blases) {
            VkAccelerationStructureInstanceKHR accelerationStructureInstance{};
            accelerationStructureInstance.transform = blas->transformMatrix;
            accelerationStructureInstance.instanceCustomIndex = 0;
            accelerationStructureInstance.mask = 0xFF;
            accelerationStructureInstance.instanceShaderBindingTableRecordOffset = 0;
            accelerationStructureInstance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
            accelerationStructureInstance.accelerationStructureReference = blas->deviceAddress;

            asInstances.push_back(accelerationStructureInstance);
        }

        Buffer instancesBuffer{
            device,
            sizeof(VkAccelerationStructureInstanceKHR),
            static_cast<uint32_t>(asInstances.size()),
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        };
        instancesBuffer.stageToBuffer((void*) asInstances.data());

        VkDeviceOrHostAddressConstKHR instancesBufferDeviceAddress{};
        instancesBufferDeviceAddress.deviceAddress = instancesBuffer.deviceAddress();

        VkAccelerationStructureGeometryKHR accelerationStructureGeometry{};
        accelerationStructureGeometry.sType                              = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        accelerationStructureGeometry.geometryType                       = VK_GEOMETRY_TYPE_INSTANCES_KHR;
        accelerationStructureGeometry.flags                              = VK_GEOMETRY_OPAQUE_BIT_KHR;
        accelerationStructureGeometry.geometry.instances.sType           = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
        accelerationStructureGeometry.geometry.instances.arrayOfPointers = VK_FALSE;
        accelerationStructureGeometry.geometry.instances.data            = instancesBufferDeviceAddress;

        const uint32_t primitiveCount = static_cast<uint32_t>(asInstances.size());

        VkAccelerationStructureBuildRangeInfoKHR accelerationStructureBuildRangeInfo{};
        accelerationStructureBuildRangeInfo.primitiveCount  = primitiveCount;
        accelerationStructureBuildRangeInfo.primitiveOffset = 0;
        accelerationStructureBuildRangeInfo.firstVertex     = 0;
        accelerationStructureBuildRangeInfo.transformOffset = 0;
        std::vector<VkAccelerationStructureBuildRangeInfoKHR*> pBuildRangeInfos = { &accelerationStructureBuildRangeInfo };

       tlas->build(&accelerationStructureGeometry, 1, &primitiveCount, pBuildRangeInfos.data());
    }

    void RayTracingSystem::buildPipeline(VkDescriptorSetLayout descriptorSetLayout) {
        VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
        pipelineLayoutCreateInfo.sType                      = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutCreateInfo.setLayoutCount             = 1;
        pipelineLayoutCreateInfo.pSetLayouts                = &descriptorSetLayout;
        if (vkCreatePipelineLayout(device.handle(), &pipelineLayoutCreateInfo, nullptr, &pipelineLayout)) {
            throw std::runtime_error("Failed to create pipeline layout");
        }

        pipeline = std::make_unique<Pipeline>(device);
        pipeline->createRayTracingPipeline(pipelineLayout);
    }

    void RayTracingSystem::render(FrameInfo &frameInfo, Swapchain &swapchain) {
        uint32_t width  = swapchain.getSwapchainExtent().width;
        uint32_t height = swapchain.getSwapchainExtent().height;
        pipeline->bind(frameInfo.commandBuffer);

        vkCmdBindDescriptorSets(frameInfo.commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipelineLayout, 0, 1, &frameInfo.globalDescriptorSet, 0, 0);
        vkCmdTraceRaysKHR(frameInfo.commandBuffer, &pipeline->raygenSBTEntry, &pipeline->missSBTEntry, &pipeline->hitSBTEntry, &pipeline->callableSBTEntry, width, height, 1);

        // TODO could this copying of images be a generic function in device
        VkImageSubresourceRange subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vkhelper::transitionImageLayout(
            frameInfo.commandBuffer, 
            swapchain.getImage(frameInfo.frameIndex), 
            VK_IMAGE_LAYOUT_UNDEFINED, 
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
        );

        vkhelper::transitionImageLayout(
            frameInfo.commandBuffer, 
            presentImage->handle, 
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            {},
            VK_ACCESS_TRANSFER_READ_BIT,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            subresourceRange
        );
/*
        VkImageCopy imageCopy{};
        imageCopy.srcSubresource    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        imageCopy.srcOffset         = {0, 0, 0};
        imageCopy.dstSubresource    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        imageCopy.dstOffset         = {0, 0, 0};
        imageCopy.extent            = {width, height, 1};
        
        vkCmdCopyImage(
            frameInfo.commandBuffer, presentImage->handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            swapchain.getImage(frameInfo.frameIndex), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageCopy
        );
*/
        VkImageSubresourceLayers subresourceLayers{};
        subresourceLayers.aspectMask        = VK_IMAGE_ASPECT_COLOR_BIT;
        subresourceLayers.baseArrayLayer    = 0;
        subresourceLayers.layerCount        = 1;
        subresourceLayers.mipLevel          = 0;

        VkImageBlit imageBlit{};
        imageBlit.srcSubresource = subresourceLayers;
        imageBlit.dstSubresource = subresourceLayers;
        imageBlit.srcOffsets[0] = {0, 0, 0}; 
        imageBlit.srcOffsets[1] = {static_cast<int32_t>(width), static_cast<int32_t>(height), 1}; 
        imageBlit.dstOffsets[0] = {0, 0, 0}; 
        imageBlit.dstOffsets[1] = {static_cast<int32_t>(width), static_cast<int32_t>(height), 1};

        vkCmdBlitImage(
            frameInfo.commandBuffer, 
            presentImage->handle, 
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            swapchain.getImage(frameInfo.frameIndex),
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &imageBlit,
            VK_FILTER_NEAREST
        );

        vkhelper::transitionImageLayout(
            frameInfo.commandBuffer, 
            swapchain.getImage(frameInfo.frameIndex), 
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
            VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
        );

        vkhelper::transitionImageLayout(
            frameInfo.commandBuffer, 
            presentImage->handle, 
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