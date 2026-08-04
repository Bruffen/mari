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

    }

    void RayTracingSystem::createImages(uint32_t width, uint32_t height) {
        accumImage   = std::make_unique<Image>(device, VkExtent3D{width, height, 1}, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_STORAGE_BIT, VK_IMAGE_LAYOUT_GENERAL);
        presentImage = std::make_unique<Image>(device, VkExtent3D{width, height, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, VK_IMAGE_LAYOUT_GENERAL);
    }

    void RayTracingSystem::buildScene(const Scene &scene) {
        // TODO reuse the same blas if the node's geometry is the same
        for (auto const &[id, node] : scene.nodes) {
            if (node->mesh) {
                buildBLAS(node, scene.materialDataBuffer->deviceAddress());
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
        buildAreaLights(scene);
    }

    void RayTracingSystem::buildBLAS(const std::shared_ptr<Node> node, uint64_t materialBufferDeviceAddress) {
        auto blas = std::make_unique<AccelerationStructure>(device, VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR);
        blas->node = node;

        std::vector<uint32_t> triangleCounts{};
        std::vector<VkAccelerationStructureGeometryKHR> geometries{};
        std::vector<VkAccelerationStructureBuildRangeInfoKHR> buildRangeInfos{};
        std::vector<VkAccelerationStructureBuildRangeInfoKHR *> pBuildRangeInfos{};

        triangleCounts.reserve(node->mesh->primMeshes.size());
        geometries.reserve(node->mesh->primMeshes.size());
        buildRangeInfos.reserve(node->mesh->primMeshes.size());
        pBuildRangeInfos.reserve(node->mesh->primMeshes.size());

        // Create geometries per primMesh so we can index materials by gl_GeometryIndexEXT
        for (const PrimMesh &primMesh : node->mesh->primMeshes) {
            // Set device adresses for buffers
            VkDeviceOrHostAddressConstKHR vertexBufferDeviceAddress{};
            VkDeviceOrHostAddressConstKHR indexBufferDeviceAddress{};
            
            vertexBufferDeviceAddress.deviceAddress = node->mesh->vertexBuffer->deviceAddress();
            indexBufferDeviceAddress.deviceAddress  = node->mesh->indexBuffer->deviceAddress() + primMesh.start * sizeof(uint32_t);

            // Optimize geometry flags for the material
            VkGeometryFlagsKHR geometryFlags;
            if ((primMesh.material->data.constants.thickness == 1.0f && 
                    (primMesh.material->data.constants.ior == 1.0f || primMesh.material->data.constants.dielectricNeeCheat)) ||
                (primMesh.material->transparent || primMesh.material->data.constants.albedo.a < 1.0f)) {
                geometryFlags = VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR;
            } else {
                geometryFlags = VK_GEOMETRY_OPAQUE_BIT_KHR;
            }

            // Set geometry info
            VkAccelerationStructureGeometryKHR accelerationStructureGeometry{};
            accelerationStructureGeometry.sType                           = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
            accelerationStructureGeometry.flags                           = geometryFlags;
            accelerationStructureGeometry.geometryType                    = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
            accelerationStructureGeometry.geometry.triangles.sType        = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
            accelerationStructureGeometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
            accelerationStructureGeometry.geometry.triangles.vertexData   = vertexBufferDeviceAddress;
            accelerationStructureGeometry.geometry.triangles.vertexStride = sizeof(Mesh::Vertex);
            accelerationStructureGeometry.geometry.triangles.maxVertex    = node->mesh->vertexCount - 1;
            accelerationStructureGeometry.geometry.triangles.indexType    = VK_INDEX_TYPE_UINT32;
            accelerationStructureGeometry.geometry.triangles.indexData    = indexBufferDeviceAddress;

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
            accelerationStructureInstance.transform = vkhelper::glmToVkMatrix(blas->node->worldMatrix);
            accelerationStructureInstance.instanceCustomIndex = 0;
            accelerationStructureInstance.mask = 0xFF;
            accelerationStructureInstance.instanceShaderBindingTableRecordOffset = 0;
            accelerationStructureInstance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
            accelerationStructureInstance.accelerationStructureReference = blas->deviceAddress;

            asInstances.push_back(accelerationStructureInstance);
        }

        accelerationStructuresInstancesBuffer = std::make_unique<Buffer>(
            device,
            sizeof(VkAccelerationStructureInstanceKHR),
            static_cast<uint32_t>(asInstances.size()),
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );
        accelerationStructuresInstancesBuffer->stageToBuffer((void*) asInstances.data());

        VkDeviceOrHostAddressConstKHR instancesBufferDeviceAddress{};
        instancesBufferDeviceAddress.deviceAddress = accelerationStructuresInstancesBuffer->deviceAddress();

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

    void RayTracingSystem::updateTLAS() { /* TODO code is the same as above except for buffer creation and tlas call, move everything else to a function*/
        std::vector<VkAccelerationStructureInstanceKHR> asInstances;
        asInstances.reserve(blases.size());

        int i = 0;
        for (auto& blas : blases) {
            VkAccelerationStructureInstanceKHR accelerationStructureInstance{};
            accelerationStructureInstance.transform = vkhelper::glmToVkMatrix(blas->node->worldMatrix);
            accelerationStructureInstance.instanceCustomIndex = 0;
            accelerationStructureInstance.mask = 0xFF;
            accelerationStructureInstance.instanceShaderBindingTableRecordOffset = 0;
            accelerationStructureInstance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
            accelerationStructureInstance.accelerationStructureReference = blas->deviceAddress;

            asInstances.push_back(accelerationStructureInstance);
        }
        accelerationStructuresInstancesBuffer->stageToBuffer((void*) asInstances.data());
        
        VkDeviceOrHostAddressConstKHR instancesBufferDeviceAddress{};
        instancesBufferDeviceAddress.deviceAddress = accelerationStructuresInstancesBuffer->deviceAddress();

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

        tlas->update(&accelerationStructureGeometry, pBuildRangeInfos.data());
    }

    void RayTracingSystem::buildPipeline(VkDescriptorSetLayout descriptorSetLayout, Integrator integrator) {
        auto descriptorSetLayouts = std::vector<VkDescriptorSetLayout>{descriptorSetLayout};
        pipelineLayout = std::make_unique<PipelineLayout>(device, &descriptorSetLayouts);
        pipeline = std::make_unique<Pipeline>(device);
        this->integrator = integrator;
        
        std::vector<std::string> shadersRayGeneration{};
        std::vector<std::string> shadersClosestHit{};
        std::vector<std::string> shadersAnyHit{};
        std::vector<std::string> shadersMiss{};
        std::vector<std::string> shadersCallable{};

        switch (integrator) {
            case Integrator::PATH_TRACING:
                shadersRayGeneration.push_back("../../shaders/spv/pathtracer.rgen.spv");

                shadersClosestHit.push_back("../../shaders/spv/pathtracer.rchit.spv");
                shadersAnyHit.push_back("../../shaders/spv/pathtracer.rahit.spv");
                shadersAnyHit.push_back("../../shaders/spv/shadow.rahit.spv");
                
                shadersMiss.push_back("../../shaders/spv/pathtracer.rmiss.spv");
                shadersMiss.push_back("../../shaders/spv/shadow.rmiss.spv");
                break;
            case Integrator::PATH_TRACING_VOLUME_ONLY:
                shadersRayGeneration.push_back("../../shaders/spv/pathtracer_volume_only.rgen.spv");
                shadersClosestHit.push_back("../../shaders/spv/pathtracer.rchit.spv");
                shadersMiss.push_back("../../shaders/spv/pathtracer.rmiss.spv");
                break;
            case Integrator::PATH_TRACING_VOLUMETRIC:
                shadersRayGeneration.push_back("../../shaders/spv/pathtracer_volumetric.rgen.spv");

                shadersClosestHit.push_back("../../shaders/spv/pathtracer.rchit.spv");
                shadersAnyHit.push_back("../../shaders/spv/pathtracer.rahit.spv");
                shadersAnyHit.push_back("../../shaders/spv/shadow_volumetric.rahit.spv");
                
                shadersMiss.push_back("../../shaders/spv/pathtracer.rmiss.spv");
                shadersMiss.push_back("../../shaders/spv/shadow.rmiss.spv");
                break;
            default:
                throw std::runtime_error("No valid integrator selected!");
            break;
        }

        pipeline->createRayTracingPipeline(*pipelineLayout, shadersRayGeneration, shadersMiss, shadersClosestHit, shadersAnyHit, shadersCallable);
    }

    // TODO handle case where no lights exist, specially on gpu side
    void RayTracingSystem::buildAreaLights(const Scene &scene) {
        lights.clear();
        
        for (const auto& [id, node] : scene.nodes) {
            if (node->mesh) {
                for (const auto& p : node->mesh->primMeshes) {
                    if (p.material->isEmissive()) {
                        assert(p.count % 3 == 0 && "PrimMesh count is not a multiple of 3 so it can't make an AreaLight!");
                        for (uint32_t i = 0; i < p.count; i += 3) {
                            auto light = std::make_unique<AreaLight>(device, *node, p, i);
                            lights.push_back(light->info);
                        }
                    }
                }
            }
        }
        const int lightID = scene.environmentID - static_cast<int>(scene.images.size()); // TODO getting infinite light this is way is pretty ugly
        lights.push_back(scene.lightObjects[lightID]->light->info);

        std::vector<float> powers{};
        powers.reserve(lights.size());
        for (const auto& light : lights) {
            powers.push_back(light.power);
        }

        vkDeviceWaitIdle(device.handle()); // TODO actual synchronization with the last trace rays queue submit

        lightsSampler = PiecewiseConstant1D(powers, 0.0f, 1.0f, &device);

        if (!lightsBuffer || lightsBuffer->getBufferSize() < lights.size() * sizeof(LightInfo)) {
            lightsBuffer = std::make_unique<Buffer>(
                device,
                sizeof(LightInfo),
                static_cast<uint32_t>(lights.size()),
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
            );
        }
            
        lightsBuffer->stageToBuffer(lights.data());
    }

    void RayTracingSystem::update(const Scene &scene) {
        if (needsRebuild) {
            blases.clear();
            for (auto const &[id, node] : scene.nodes) {
                if (node->mesh) {
                    buildBLAS(node, scene.materialDataBuffer->deviceAddress());
                }
            }
            buildTLAS();
            needsRebuild = false;
        }
        
        if (needsUpdate) {
            updateTLAS();
            needsUpdate = false;
        }

        if (needsLightsRebuild) {
            buildAreaLights(scene);
            needsLightsRebuild = false;
        }
    }

    void RayTracingSystem::render(FrameInfo &frameInfo, Swapchain &swapchain) {
        uint32_t width  = swapchain.getSwapchainExtent().width;
        uint32_t height = swapchain.getSwapchainExtent().height;
        pipeline->bind(frameInfo.commandBuffer);

        vkCmdBindDescriptorSets(frameInfo.commandBuffer, pipeline->bindPoint(), pipelineLayout->handle(), 0, 1, &frameInfo.globalDescriptorSet, 0, 0);
        vkCmdTraceRaysKHR(frameInfo.commandBuffer, &pipeline->raygenSBTEntry, &pipeline->missSBTEntry, &pipeline->hitSBTEntry, &pipeline->callableSBTEntry, width, height, 1);

        device.copyImageToImage(frameInfo.commandBuffer, presentImage->handle, swapchain.getImage(frameInfo.frameIndex), presentImage->size);
    }
}