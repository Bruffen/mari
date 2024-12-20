#include "mari_rt.hpp"

#include "keyboard_controller.hpp"
#include "buffer.hpp"
#include "camera.hpp"
#include "systems/simple_render_system.hpp"
#include "systems/point_light_system.hpp"
#include "vk_helper.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include <array>
#include <chrono>
#include <cassert>
#include <stdexcept>

#include <iostream>

namespace mari {

    MariRT::MariRT() {
        globalPool = DescriptorPool::Builder(device)
            .setMaxSets(Swapchain::MAX_FRAMES_IN_FLIGHT)
            .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, Swapchain::MAX_FRAMES_IN_FLIGHT)
            .build();
        
        std::cout << "Loading game objects..." << std::endl;
        loadGameObjects();
        std::cout << "Initializing ray tracing..." << std::endl;
        initializeRayTracing();
        std::cout << "Get function pointers..." << std::endl;
        getRayTracingFunctionPointers();
        std::cout << "Creating storage image..." << std::endl;
        createStorageImage();
        std::cout << "Building BLAS..." << std::endl;
        buildBottomLevelAccelerationStructure();
        std::cout << "Building TLAS..." << std::endl;
        buildTopLevelAccelerationStructure();
        std::cout << "Creating uniform buffers..." << std::endl;
        createUniformBuffers();
        std::cout << "Creating ray tracing pipeline..." << std::endl;
        createRayTracingPipeline();
        std::cout << "Creating shader binding tables..." << std::endl;
        createShaderBindingTables();
        std::cout << "Creating descriptor sets..." << std::endl;
        createDescriptorSets();
        std::cout << "Building command buffers..." << std::endl;
        buildCommandBuffers();
    }

    MariRT::~MariRT() {
        for (VkShaderModule sm : shaderModules) {
            vkDestroyShaderModule(device.handle(), sm, nullptr); // TODO rt
        }
    }

    void MariRT::run() { 
        Camera camera{};
        
        // TODO how to keep multiple pointers for each callback
        //glfwSetWindowUserPointer(window.getGLFWwindow(), &camera);
        //glfwSetScrollCallback(window.getGLFWwindow(), camera.scrollCallback);
        glfwSetWindowUserPointer(window.getGLFWwindow(), &window);

        auto cameraObject = GameObject::createGameObject();
        cameraObject.transform.translation.y = -0.5f;
        cameraObject.transform.translation.z = -2.5f;
        KeyboardController cameraController{};

        auto startTime = std::chrono::high_resolution_clock::now();
        auto currentTime = startTime;

        while (!window.shouldClose()) {
            glfwPollEvents();

            auto newTime = std::chrono::high_resolution_clock::now();
            float frameTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
            float elapsedTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime - startTime).count();
            currentTime = newTime;

            cameraController.rotateCamera(window.getGLFWwindow(), frameTime, cameraObject);
            cameraController.moveCamera(window.getGLFWwindow(), frameTime, cameraObject);
            camera.setViewYXZ(cameraObject.transform.translation, cameraObject.transform.rotation);

            float aspect = renderer.getAspectRatio();
            //camera.setOrthographicProjection(-aspect, aspect, -1, 1, 0.1f, 100.0f);
            camera.setPerspectiveProjection(aspect, 0.1f, 100.0f);
            
            if (auto commandBuffer = renderer.beginFrame(false)) {
                int frameIndex = renderer.getFrameIndex();
                FrameInfo frameInfo {
                    frameIndex,
                    frameTime,
                    elapsedTime,
                    commandBuffer,
                    camera,
                    0,
                    gameObjects
                };
                
                // update
                UniformData ubo{};
                ubo.viewInverse = camera.getInverseView();
                ubo.projInverse = camera.getInverseProjection();

                uboBuffers[frameIndex]->writeToBuffer(&ubo);
                uboBuffers[frameIndex]->flush();

                // render
                //renderer.beginSwapchainRenderPass(commandBuffer); // TODO ???
                //renderer.endSwapchainRenderPass(commandBuffer);   // TODO ???
                

                renderer.endFrame(false);
            }
        }

        vkDeviceWaitIdle(device.handle());
    };

    void MariRT::loadGameObjects() {
        //std::shared_ptr<Model> model = Model::createCubeModel(device, {0.0f, 0.0f, 0.0f});
        //std::shared_ptr<Model> model = Model::createModelFromFile(device, "../../../../_Models/CornellBox/CornellBox-Original.obj");

        std::shared_ptr<Model> model = Model::createModelFromFile(device, "../../../../_Models/DOA/marie_rose_twinkle_rose/marie_rose_twinkle_rose_standing1.obj");
        auto gameObject = GameObject::createGameObject();
        gameObject.model = model;
        gameObject.transform.translation = {3.0f, -0.01f, 0.0f};
        gameObject.transform.rotation = {0.0f, glm::radians(180.0f), glm::radians(180.0f)};
        gameObject.transform.scale = glm::vec3{3.0f};
        gameObjects.emplace(gameObject.getId(), std::move(gameObject));

        model = Model::createModelFromFile(device, "../../models/flat_vase.obj");
        auto gfvase = GameObject::createGameObject();
        gfvase.model = model;
        gfvase.transform.translation = {1.0f, 0.0f, 0.0f};
        gfvase.transform.rotation = glm::vec3{0.0f};
        gfvase.transform.scale = glm::vec3{3.0f};
        gameObjects.emplace(gfvase.getId(), std::move(gfvase));

        model = Model::createModelFromFile(device, "../../models/smooth_vase.obj");
        auto gsvase = GameObject::createGameObject();
        gsvase.model = model;
        gsvase.transform.translation = {1.8f, 0.0f, 0.0f};
        gsvase.transform.rotation = glm::vec3{0.0f};
        gsvase.transform.scale = glm::vec3{3.0f};
        gameObjects.emplace(gsvase.getId(), std::move(gsvase));

        model = Model::createModelFromFile(device, "../../models/quad.obj");
        auto floor = GameObject::createGameObject();
        floor.model = model;
        floor.transform.translation = {0.0f, 0.0f, 0.0f};
        floor.transform.rotation = glm::vec3{0.0f};
        floor.transform.scale = glm::vec3{3.0f};
        gameObjects.emplace(floor.getId(), std::move(floor));

        std::vector<glm::vec3> lightColors {
            {1.f, .1f, .1f},
            {.1f, .1f, 1.f},
            {.1f, 1.f, .1f},
            {1.f, 1.f, .1f},
            {.1f, 1.f, 1.f},
            {1.f, 1.f, 1.f}
        };

        for (int i = 0; i < lightColors.size(); i++) {
            auto pointLight = GameObject::makePointLight(0.2f);
            pointLight.color = lightColors[i];
            auto rotateLight = glm::rotate(
                glm::mat4(1.0f),
                (i * glm::two_pi<float>()) / lightColors.size(),
                {0.0f, -1.0f, 0.0f}
            );
            pointLight.transform.translation = glm::vec3(rotateLight * glm::vec4(-1.0f, -1.0f, -1.0f, 1.0f));
            gameObjects.emplace(pointLight.getId(), std::move(pointLight));
        }
    }

    void MariRT::initializeRayTracing() {
        // Requesting ray tracing properties
        VkPhysicalDeviceProperties2 prop2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2}; //TODO do we need to get this again?
        prop2.pNext = &rtProperties;
        vkGetPhysicalDeviceProperties2(device.getPhysicalDevice(), &prop2);

        // TODO
        /*
        // Get ray tracing pipeline properties, which will be used later on in the sample
		rayTracingPipelineProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
		VkPhysicalDeviceProperties2 deviceProperties2{};
		deviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		deviceProperties2.pNext = &rayTracingPipelineProperties;
		vkGetPhysicalDeviceProperties2(device.getPhysicalDevice(), &deviceProperties2);

		// Get acceleration structure properties, which will be used later on in the sample
		accelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
		VkPhysicalDeviceFeatures2 deviceFeatures2{};
		deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		deviceFeatures2.pNext = &accelerationStructureFeatures;
		vkGetPhysicalDeviceFeatures2(device.getPhysicalDevice(), &deviceFeatures2)
        */
    }

    void MariRT::getRayTracingFunctionPointers() {
        device.vkGetBufferDeviceAddressKHR = reinterpret_cast<PFN_vkGetBufferDeviceAddressKHR>(vkGetDeviceProcAddr(device.handle(), "vkGetBufferDeviceAddressKHR"));
        device.vkCmdBuildAccelerationStructuresKHR = reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(vkGetDeviceProcAddr(device.handle(), "vkCmdBuildAccelerationStructuresKHR"));
        device.vkBuildAccelerationStructuresKHR = reinterpret_cast<PFN_vkBuildAccelerationStructuresKHR>(vkGetDeviceProcAddr(device.handle(), "vkBuildAccelerationStructuresKHR"));
        device.vkCreateAccelerationStructureKHR = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(vkGetDeviceProcAddr(device.handle(), "vkCreateAccelerationStructureKHR"));
        device.vkDestroyAccelerationStructureKHR = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(vkGetDeviceProcAddr(device.handle(), "vkDestroyAccelerationStructureKHR"));
        device.vkGetAccelerationStructureBuildSizesKHR = reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(vkGetDeviceProcAddr(device.handle(), "vkGetAccelerationStructureBuildSizesKHR"));
        device.vkGetAccelerationStructureDeviceAddressKHR = reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(vkGetDeviceProcAddr(device.handle(), "vkGetAccelerationStructureDeviceAddressKHR"));
        device.vkCmdTraceRaysKHR = reinterpret_cast<PFN_vkCmdTraceRaysKHR>(vkGetDeviceProcAddr(device.handle(), "vkCmdTraceRaysKHR"));
        device.vkGetRayTracingShaderGroupHandlesKHR = reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(vkGetDeviceProcAddr(device.handle(), "vkGetRayTracingShaderGroupHandlesKHR"));
        device.vkCreateRayTracingPipelinesKHR = reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(vkGetDeviceProcAddr(device.handle(), "vkCreateRayTracingPipelinesKHR"));
    }

    void MariRT::createStorageImage() {
        storageImage.width  = WIDTH;
        storageImage.height = HEIGHT;

        VkImageCreateInfo imageCreateInfo{};
        imageCreateInfo.sType           = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageCreateInfo.imageType       = VK_IMAGE_TYPE_2D;
        imageCreateInfo.format          = VK_FORMAT_B8G8R8A8_UNORM;
        imageCreateInfo.extent.width    = storageImage.width;
        imageCreateInfo.extent.height   = storageImage.height;
        imageCreateInfo.extent.depth    = 1;
        imageCreateInfo.mipLevels       = 1;
        imageCreateInfo.arrayLayers     = 1;
        imageCreateInfo.samples         = VK_SAMPLE_COUNT_1_BIT;
        imageCreateInfo.tiling          = VK_IMAGE_TILING_OPTIMAL;
        imageCreateInfo.usage           = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
        imageCreateInfo.initialLayout   = VK_IMAGE_LAYOUT_UNDEFINED;
        if (vkCreateImage(device.handle(), &imageCreateInfo, nullptr, &storageImage.image)) {
            throw std::runtime_error("Failed to create image");
        }

        VkMemoryRequirements memoryRequirements;
        vkGetImageMemoryRequirements(device.handle(), storageImage.image, &memoryRequirements);

        VkMemoryAllocateInfo memoryAllocateInfo{};
        memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        memoryAllocateInfo.allocationSize = memoryRequirements.size;
        memoryAllocateInfo.memoryTypeIndex = device.findMemoryType(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (vkAllocateMemory(device.handle(), &memoryAllocateInfo, nullptr, &storageImage.memory)) {
            throw std::runtime_error("Failed to allocate memory for image");
        }        
        if (vkBindImageMemory(device.handle(), storageImage.image, storageImage.memory, 0)) {
            throw std::runtime_error("Failed to bind memory for image");
        }
        
        VkImageViewCreateInfo imageViewCreateInfo{};
        imageViewCreateInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewCreateInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        imageViewCreateInfo.format                          = VK_FORMAT_B8G8R8A8_UNORM;
        imageViewCreateInfo.subresourceRange                = {};
        imageViewCreateInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        imageViewCreateInfo.subresourceRange.baseMipLevel   = 0;
        imageViewCreateInfo.subresourceRange.levelCount     = 1;
        imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        imageViewCreateInfo.subresourceRange.layerCount     = 1;
        imageViewCreateInfo.image                           = storageImage.image;
        if (vkCreateImageView(device.handle(), &imageViewCreateInfo, nullptr, &storageImage.view)) {
            throw std::runtime_error("Failed to create image view");
        }

        VkCommandBuffer cmdBuffer = device.beginSingleTimeCommands();
        VkImageMemoryBarrier imgBarrier{};
        imgBarrier.sType                = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        imgBarrier.srcAccessMask        = {};
        imgBarrier.dstAccessMask        = {};
        imgBarrier.oldLayout            = VK_IMAGE_LAYOUT_UNDEFINED;
        imgBarrier.newLayout            = VK_IMAGE_LAYOUT_GENERAL;
        imgBarrier.srcQueueFamilyIndex  = VK_QUEUE_FAMILY_IGNORED;
        imgBarrier.dstQueueFamilyIndex  = VK_QUEUE_FAMILY_IGNORED;
        imgBarrier.image                = storageImage.image;
        imgBarrier.subresourceRange     = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

        vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &imgBarrier);
        device.endSingleTimeCommands(cmdBuffer);
    }

    void MariRT::createUniformBuffers() {
        uboBuffers = std::vector<std::unique_ptr<Buffer>>{Swapchain::MAX_FRAMES_IN_FLIGHT};

        for (int i = 0; i < uboBuffers.size(); i++) {
            uboBuffers[i] = std::make_unique<Buffer>(
                device,
                sizeof(uniformData),
                1,
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
            );
            uboBuffers[i]->map();
        }
    }

    ScratchBuffer MariRT::createScratchBuffer(VkDeviceSize size) {
        ScratchBuffer scratchBuffer{};

        VkBufferCreateInfo bufferCreateInfo = {};
        bufferCreateInfo.sType              = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferCreateInfo.size               = size;
        bufferCreateInfo.usage              = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        vkCreateBuffer(device.handle(), &bufferCreateInfo, nullptr, &scratchBuffer.handle);

        VkMemoryRequirements memoryRequirements = {};
        vkGetBufferMemoryRequirements(device.handle(), scratchBuffer.handle, &memoryRequirements);

        VkMemoryAllocateFlagsInfo memoryAllocateFlagsInfo = {};
        memoryAllocateFlagsInfo.sType                     = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
        memoryAllocateFlagsInfo.flags                     = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;

        VkMemoryAllocateInfo memoryAllocateInfo = {};
        memoryAllocateInfo.sType                = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        memoryAllocateInfo.pNext                = &memoryAllocateFlagsInfo;
        memoryAllocateInfo.allocationSize       = memoryRequirements.size;
        memoryAllocateInfo.memoryTypeIndex      = device.findMemoryType(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        vkAllocateMemory(device.handle(), &memoryAllocateInfo, nullptr, &scratchBuffer.memory);
        vkBindBufferMemory(device.handle(), scratchBuffer.handle, scratchBuffer.memory, 0);

        VkBufferDeviceAddressInfoKHR bufferDeviceAddressInfo{};
        bufferDeviceAddressInfo.sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        bufferDeviceAddressInfo.buffer = scratchBuffer.handle;
        scratchBuffer.device_address   = device.vkGetBufferDeviceAddressKHR(device.handle(), &bufferDeviceAddressInfo);

        return scratchBuffer;
    }

    void MariRT::deleteScratchBuffer(ScratchBuffer &scratchBuffer) {
        if (scratchBuffer.memory != VK_NULL_HANDLE) {
            vkFreeMemory(device.handle(), scratchBuffer.memory, nullptr);
        }
        if (scratchBuffer.handle != VK_NULL_HANDLE) {
            vkDestroyBuffer(device.handle(), scratchBuffer.handle, nullptr);
        }
    }

    void MariRT::buildBottomLevelAccelerationStructure() {
        // Test identity transform matrix TODO rt use gameobject's
        VkTransformMatrixKHR transformMatrix = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f
        };

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

        GameObject* marie = &gameObjects.at(0);
        
        vertexBufferDeviceAddress.deviceAddress = marie->model->vertexBuffer->deviceAddress();
        indexBufferDeviceAddress.deviceAddress = marie->model->indexBuffer->deviceAddress();
        transformBufferDeviceAddress.deviceAddress = transformBuffer->deviceAddress();

        // Build
        VkAccelerationStructureGeometryKHR accelerationStructureGeometry{};
        accelerationStructureGeometry.sType                            = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        accelerationStructureGeometry.flags                            = VK_GEOMETRY_OPAQUE_BIT_KHR;
        accelerationStructureGeometry.geometryType                     = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
        accelerationStructureGeometry.geometry.triangles.sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
        accelerationStructureGeometry.geometry.triangles.vertexFormat  = VK_FORMAT_R32G32B32_SFLOAT;
        accelerationStructureGeometry.geometry.triangles.vertexData    = vertexBufferDeviceAddress;
        accelerationStructureGeometry.geometry.triangles.maxVertex     = marie->model->vertexCount - 1;
        accelerationStructureGeometry.geometry.triangles.vertexStride  = sizeof(Model::Vertex::position);
        accelerationStructureGeometry.geometry.triangles.indexType     = VK_INDEX_TYPE_UINT32;
        accelerationStructureGeometry.geometry.triangles.indexData     = indexBufferDeviceAddress;
        //accelerationStructureGeometry.geometry.triangles.transformData.deviceAddress = 0;
        //accelerationStructureGeometry.geometry.triangles.transformData.hostAddress = nullptr;
        accelerationStructureGeometry.geometry.triangles.transformData = transformBufferDeviceAddress;
        
        // Get size info
        VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo{};
        accelerationStructureBuildGeometryInfo.sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        accelerationStructureBuildGeometryInfo.type          = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        accelerationStructureBuildGeometryInfo.flags         = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        accelerationStructureBuildGeometryInfo.geometryCount = 1;
        accelerationStructureBuildGeometryInfo.pGeometries   = &accelerationStructureGeometry;

        const uint32_t numTriangles = marie->model->indexCount / 3;
        VkAccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo{};
        accelerationStructureBuildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
        device.vkGetAccelerationStructureBuildSizesKHR(
            device.handle(),
            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            &accelerationStructureBuildGeometryInfo,
            &numTriangles,
            &accelerationStructureBuildSizesInfo);

        bottomLevelAS.buffer = std::make_unique<Buffer>(
            device, 
            accelerationStructureBuildSizesInfo.accelerationStructureSize, 
            1, 
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );

        VkAccelerationStructureCreateInfoKHR accelerationStructureCreateInfo{};
        accelerationStructureCreateInfo.sType  = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
        accelerationStructureCreateInfo.buffer = bottomLevelAS.buffer->handle();
        accelerationStructureCreateInfo.size   = accelerationStructureBuildSizesInfo.accelerationStructureSize;
        accelerationStructureCreateInfo.type   = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        if (device.vkCreateAccelerationStructureKHR(device.handle(), &accelerationStructureCreateInfo, nullptr, &bottomLevelAS.handle)) {
            throw std::runtime_error("Could not create blas");
        }

        ScratchBuffer scratchBuffer = createScratchBuffer(accelerationStructureBuildSizesInfo.buildScratchSize);

        accelerationStructureBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        accelerationStructureBuildGeometryInfo.dstAccelerationStructure = bottomLevelAS.handle;
        accelerationStructureBuildGeometryInfo.scratchData.deviceAddress = scratchBuffer.device_address;

        VkAccelerationStructureBuildRangeInfoKHR accelerationStructureBuildRangeInfo{};
        accelerationStructureBuildRangeInfo.primitiveCount  = numTriangles;
        accelerationStructureBuildRangeInfo.primitiveOffset = 0;
        accelerationStructureBuildRangeInfo.firstVertex     = 0;
        accelerationStructureBuildRangeInfo.transformOffset = 0;
        std::vector<VkAccelerationStructureBuildRangeInfoKHR *> accelerationStructureBuildRangeInfos = {&accelerationStructureBuildRangeInfo};

        VkCommandBuffer cmdBuffer = device.beginSingleTimeCommands();
        device.vkCmdBuildAccelerationStructuresKHR(cmdBuffer, 1, &accelerationStructureBuildGeometryInfo, accelerationStructureBuildRangeInfos.data());
        device.endSingleTimeCommands(cmdBuffer);

        deleteScratchBuffer(scratchBuffer);

        VkAccelerationStructureDeviceAddressInfoKHR accelerationStructureDeviceAddressInfo{};
        accelerationStructureDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
        accelerationStructureDeviceAddressInfo.accelerationStructure = bottomLevelAS.handle;
        bottomLevelAS.device_address = device.vkGetAccelerationStructureDeviceAddressKHR(device.handle(), &accelerationStructureDeviceAddressInfo);
    }

    VkPipelineShaderStageCreateInfo MariRT::loadShader(const std::string &filepath, VkShaderStageFlagBits stage) {
        VkShaderModule shaderModule = Pipeline::createShaderModule(device, Pipeline::readFile(filepath));

        VkPipelineShaderStageCreateInfo shaderStage{};
        shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStage.stage = stage;
        shaderStage.module = shaderModule;
        shaderStage.pName = "main";
        assert(shaderStage.module != VK_NULL_HANDLE);

        shaderModules.push_back(std::move(shaderModule));
        return shaderStage;
    }

    void MariRT::buildTopLevelAccelerationStructure() {
        VkTransformMatrixKHR transformMatrix = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f
        };

        VkAccelerationStructureInstanceKHR accelerationStructureInstance{};
        accelerationStructureInstance.transform = transformMatrix;
        accelerationStructureInstance.instanceCustomIndex = 0;
        accelerationStructureInstance.mask = 0xFF;
        accelerationStructureInstance.instanceShaderBindingTableRecordOffset = 0;
        accelerationStructureInstance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        accelerationStructureInstance.accelerationStructureReference = bottomLevelAS.device_address;

        Buffer stagingBuffer{
            device,
            sizeof(accelerationStructureInstance),
            1,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        };

        stagingBuffer.map();
        stagingBuffer.writeToBuffer((void*) &accelerationStructureInstance);

        auto instancesBuffer = std::make_unique<Buffer>(
            device,
            sizeof(accelerationStructureInstance),
            1,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT
            | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );

        device.copyBuffer(stagingBuffer.handle(), instancesBuffer->handle(), sizeof(accelerationStructureInstance));

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

        const uint32_t primitiveCount = 1;
        
        VkAccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo{};
        accelerationStructureBuildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
        device.vkGetAccelerationStructureBuildSizesKHR(
            device.handle(),
            VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
            &accelerationStructureBuildGeometryInfo,
            &primitiveCount,
            &accelerationStructureBuildSizesInfo
        );

        topLevelAS.buffer = std::make_unique<Buffer>(
            device, 
            accelerationStructureBuildSizesInfo.accelerationStructureSize, 
            1, 
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );

        VkAccelerationStructureCreateInfoKHR accelerationStructureCreateInfo{};
        accelerationStructureCreateInfo.sType  = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
        accelerationStructureCreateInfo.buffer = topLevelAS.buffer->handle();
        accelerationStructureCreateInfo.size   = accelerationStructureBuildSizesInfo.accelerationStructureSize;
        accelerationStructureCreateInfo.type   = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        if (device.vkCreateAccelerationStructureKHR(device.handle(), &accelerationStructureCreateInfo, nullptr, &topLevelAS.handle)) {
            throw std::runtime_error("Could not create tlas");
        };

        ScratchBuffer scratchBuffer = createScratchBuffer(accelerationStructureBuildSizesInfo.buildScratchSize);

        accelerationStructureBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        accelerationStructureBuildGeometryInfo.dstAccelerationStructure = topLevelAS.handle;
        accelerationStructureBuildGeometryInfo.scratchData.deviceAddress = scratchBuffer.device_address;

        VkAccelerationStructureBuildRangeInfoKHR accelerationStructureBuildRangeInfo{};
        accelerationStructureBuildRangeInfo.primitiveCount  = primitiveCount;
        accelerationStructureBuildRangeInfo.primitiveOffset = 0;
        accelerationStructureBuildRangeInfo.firstVertex     = 0;
        accelerationStructureBuildRangeInfo.transformOffset = 0;
        std::vector<VkAccelerationStructureBuildRangeInfoKHR *> accelerationStructureBuildRangeInfos = {&accelerationStructureBuildRangeInfo};

        VkCommandBuffer cmdBuffer = device.beginSingleTimeCommands();
        device.vkCmdBuildAccelerationStructuresKHR(cmdBuffer, 1, &accelerationStructureBuildGeometryInfo, accelerationStructureBuildRangeInfos.data());
        device.endSingleTimeCommands(cmdBuffer);

        deleteScratchBuffer(scratchBuffer);

        VkAccelerationStructureDeviceAddressInfoKHR accelerationStructureDeviceAddressInfo{};
        accelerationStructureDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
        accelerationStructureDeviceAddressInfo.accelerationStructure = topLevelAS.handle;
        topLevelAS.device_address = device.vkGetAccelerationStructureDeviceAddressKHR(device.handle(), &accelerationStructureDeviceAddressInfo);
    }

    void MariRT::createRayTracingPipeline() {
        VkDescriptorSetLayoutBinding accelerationStructureLayoutBinding{};
        accelerationStructureLayoutBinding.binding          = 0;
        accelerationStructureLayoutBinding.descriptorType   = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        accelerationStructureLayoutBinding.descriptorCount  = 1;
        accelerationStructureLayoutBinding.stageFlags       = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

        VkDescriptorSetLayoutBinding resultImageLayoutBinding{};
        resultImageLayoutBinding.binding                    = 1;
        resultImageLayoutBinding.descriptorType             = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        resultImageLayoutBinding.descriptorCount            = 1;
        resultImageLayoutBinding.stageFlags                 = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

        VkDescriptorSetLayoutBinding uniformBufferLayoutBinding{};
        uniformBufferLayoutBinding.binding                  = 2;
        uniformBufferLayoutBinding.descriptorType           = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uniformBufferLayoutBinding.descriptorCount          = 1;
        uniformBufferLayoutBinding.stageFlags               = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

        std::vector<VkDescriptorSetLayoutBinding> bindings = {
            accelerationStructureLayoutBinding,
            resultImageLayoutBinding,
            uniformBufferLayoutBinding
        };

        VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{};
        descriptorSetLayoutCreateInfo.sType                 = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptorSetLayoutCreateInfo.bindingCount          = static_cast<uint32_t>(bindings.size());
        descriptorSetLayoutCreateInfo.pBindings             = bindings.data();
        if (vkCreateDescriptorSetLayout(device.handle(), &descriptorSetLayoutCreateInfo, nullptr, &descriptorSetLayout)) {
            throw std::runtime_error("Failed to create descriptor set layout");
        }

        VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
        pipelineLayoutCreateInfo.sType                      = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutCreateInfo.setLayoutCount             = 1;
        pipelineLayoutCreateInfo.pSetLayouts                = &descriptorSetLayout;
        if (vkCreatePipelineLayout(device.handle(), &pipelineLayoutCreateInfo, nullptr, &pipelineLayout)) {
            throw std::runtime_error("Failed to create pipeline layout");
        }

        std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
        
        {
            shaderStages.push_back(loadShader("../../shaders/raygen.rgen.spv", VK_SHADER_STAGE_RAYGEN_BIT_KHR));
            VkRayTracingShaderGroupCreateInfoKHR raygenGroupCreateInfo{};
            raygenGroupCreateInfo.sType                     = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
            raygenGroupCreateInfo.type                      = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
            raygenGroupCreateInfo.generalShader             = static_cast<uint32_t>(shaderStages.size() - 1);
            raygenGroupCreateInfo.closestHitShader          = VK_SHADER_UNUSED_KHR;
            raygenGroupCreateInfo.anyHitShader              = VK_SHADER_UNUSED_KHR;
            raygenGroupCreateInfo.intersectionShader        = VK_SHADER_UNUSED_KHR;
            shaderGroups.push_back(raygenGroupCreateInfo);
        }

        {
            shaderStages.push_back(loadShader("../../shaders/miss.rmiss.spv", VK_SHADER_STAGE_MISS_BIT_KHR));
            VkRayTracingShaderGroupCreateInfoKHR missGroupCreateInfo{};
            missGroupCreateInfo.sType                       = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
            missGroupCreateInfo.type                        = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
            missGroupCreateInfo.generalShader               = static_cast<uint32_t>(shaderStages.size() - 1);
            missGroupCreateInfo.closestHitShader            = VK_SHADER_UNUSED_KHR;
            missGroupCreateInfo.anyHitShader                = VK_SHADER_UNUSED_KHR;
            missGroupCreateInfo.intersectionShader          = VK_SHADER_UNUSED_KHR;
            shaderGroups.push_back(missGroupCreateInfo);
        }

        {
            shaderStages.push_back(loadShader("../../shaders/closesthit.rchit.spv", VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR));
            VkRayTracingShaderGroupCreateInfoKHR chitGroupCreateInfo{};
            chitGroupCreateInfo.sType                       = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
            chitGroupCreateInfo.type                        = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
            chitGroupCreateInfo.generalShader               = VK_SHADER_UNUSED_KHR;
            chitGroupCreateInfo.closestHitShader            = static_cast<uint32_t>(shaderStages.size() - 1);
            chitGroupCreateInfo.anyHitShader                = VK_SHADER_UNUSED_KHR;
            chitGroupCreateInfo.intersectionShader          = VK_SHADER_UNUSED_KHR;
            shaderGroups.push_back(chitGroupCreateInfo);
        }

        VkRayTracingPipelineCreateInfoKHR rayTracingPipelineCreateInfo{};
        rayTracingPipelineCreateInfo.sType                  = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
        rayTracingPipelineCreateInfo.stageCount             = static_cast<uint32_t>(shaderStages.size());
        rayTracingPipelineCreateInfo.pStages                = shaderStages.data();
        rayTracingPipelineCreateInfo.groupCount             = static_cast<uint32_t>(shaderGroups.size());
        rayTracingPipelineCreateInfo.pGroups                = shaderGroups.data();
        rayTracingPipelineCreateInfo.maxPipelineRayRecursionDepth = 1;
        rayTracingPipelineCreateInfo.layout                 = pipelineLayout;
        if (device.vkCreateRayTracingPipelinesKHR(device.handle(), VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &rayTracingPipelineCreateInfo, nullptr, &pipeline)) {
            throw std::runtime_error("Failed to create ray tracing pipeline");
        }

    }

    inline uint32_t alignedSize(uint32_t value, uint32_t alignment) {
        return (value + alignment - 1) & ~(alignment - 1);
    }

    void MariRT::createShaderBindingTables() {
        const uint32_t              handleSize          = rtProperties.shaderGroupHandleSize;
        const uint32_t              handleSizeAligned   = alignedSize(rtProperties.shaderGroupHandleSize, rtProperties.shaderGroupHandleAlignment);
        const uint32_t              handleAlignment     = rtProperties.shaderGroupHandleAlignment;
        const uint32_t              groupCount          = static_cast<uint32_t>(shaderGroups.size());
        const uint32_t              sbtSize             = groupCount * handleSizeAligned;
        const VkBufferUsageFlags    sbtBufferUsageFlags = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        // TODO vma

        raygenSBT = std::make_unique<Buffer>(device, handleSize, 1, sbtBufferUsageFlags, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        missSBT   = std::make_unique<Buffer>(device, handleSize, 1, sbtBufferUsageFlags, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        hitSBT    = std::make_unique<Buffer>(device, handleSize, 1, sbtBufferUsageFlags, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

        std::vector<uint8_t> shaderHandleStorage(sbtSize);
        if (device.vkGetRayTracingShaderGroupHandlesKHR(device.handle(), pipeline, 0, groupCount, sbtSize, shaderHandleStorage.data())) {
            throw std::runtime_error("Failed to get ray tracing shader group handles");
        }

        uint8_t *data = static_cast<uint8_t *>(raygenSBT->getMappedMemory()); // TODO rt make sure this works
        memcpy(data, shaderHandleStorage.data(), handleSize);
        data = static_cast<uint8_t *>(missSBT->getMappedMemory());
        memcpy(data, shaderHandleStorage.data() + handleSizeAligned, handleSize);
        data = static_cast<uint8_t *>(hitSBT->getMappedMemory());
        memcpy(data, shaderHandleStorage.data() + handleSizeAligned * 2, handleSize);

        raygenSBT->unmap();
        missSBT->unmap();
        hitSBT->unmap();
    }

    void MariRT::createDescriptorSets() {
        std::vector<VkDescriptorPoolSize> poolSizes = {
            {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1},
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
        };

        // TODO rt inline function(?)
        VkDescriptorPoolCreateInfo poolCreateInfo{};
        poolCreateInfo.sType                        = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolCreateInfo.poolSizeCount                = static_cast<uint32_t>(poolSizes.size());
        poolCreateInfo.pPoolSizes                   = poolSizes.data();
        poolCreateInfo.maxSets                      = Swapchain::MAX_FRAMES_IN_FLIGHT;
        if (vkCreateDescriptorPool(device.handle(), &poolCreateInfo, nullptr, &descriptorPool)) {
            throw std::runtime_error("Failed to create descriptor pool");
        }

        descriptorSets = std::vector<VkDescriptorSet>();
        descriptorSets.resize(3);

        std::vector<VkDescriptorSetLayout> descriptorSetLayouts{ // TODO
            descriptorSetLayout,
            descriptorSetLayout,
            descriptorSetLayout
        };

        VkDescriptorSetAllocateInfo descriptorAllocateInfo{};
        descriptorAllocateInfo.sType                = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        descriptorAllocateInfo.descriptorPool       = descriptorPool;
        descriptorAllocateInfo.pSetLayouts          = descriptorSetLayouts.data();
        descriptorAllocateInfo.descriptorSetCount   = static_cast<uint32_t>(descriptorSets.size());
        if (vkAllocateDescriptorSets(device.handle(), &descriptorAllocateInfo, descriptorSets.data())) {
            throw std::runtime_error("Failed to allocate descriptor sets");
        }

        for (int i = 0; i < descriptorSets.size(); i++) {
            VkWriteDescriptorSetAccelerationStructureKHR accelerationStructureSet{};
            accelerationStructureSet.sType                      = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
            accelerationStructureSet.accelerationStructureCount = 1;
            accelerationStructureSet.pAccelerationStructures    = &topLevelAS.handle;

            VkWriteDescriptorSet accelerationStructureWrite{};
            accelerationStructureWrite.sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            accelerationStructureWrite.dstSet           = descriptorSets[i];
            accelerationStructureWrite.dstBinding       = 0;
            accelerationStructureWrite.descriptorType   = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
            accelerationStructureWrite.descriptorCount  = 1;
            accelerationStructureWrite.pNext            = &accelerationStructureSet;

            VkDescriptorImageInfo imageDescriptor{};
            imageDescriptor.imageView                   = storageImage.view;
            imageDescriptor.imageLayout                 = VK_IMAGE_LAYOUT_GENERAL;

            VkWriteDescriptorSet imageWriteSet{};
            imageWriteSet.sType                         = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            imageWriteSet.dstSet                        = descriptorSets[i];
            imageWriteSet.dstBinding                    = 1;
            imageWriteSet.descriptorType                = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            imageWriteSet.pImageInfo                    = &imageDescriptor;
            imageWriteSet.descriptorCount               = 1;

            VkWriteDescriptorSet uniformWriteSet{};
            uniformWriteSet.sType                       = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            uniformWriteSet.dstSet                      = descriptorSets[i];
            uniformWriteSet.dstBinding                  = 2;
            uniformWriteSet.descriptorType              = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            uniformWriteSet.pBufferInfo                 = &uboBuffers[i]->descriptorInfo();
            uniformWriteSet.descriptorCount             = 1;

            std::vector<VkWriteDescriptorSet> writeSets = {
                accelerationStructureWrite, imageWriteSet, uniformWriteSet
            };

            vkUpdateDescriptorSets(device.handle(), static_cast<uint32_t>(writeSets.size()), writeSets.data(), 0, VK_NULL_HANDLE);
        }
    }

    void MariRT::buildCommandBuffers() {
        VkCommandBufferBeginInfo cmdBufferBeginInfo{};
        cmdBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        
        VkImageSubresourceRange subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

        for (int32_t i = 0; i < renderer.commandBuffers.size(); i++) {
            if (vkBeginCommandBuffer(renderer.commandBuffers[i], &cmdBufferBeginInfo)) {
                throw std::runtime_error("Failed to begin command buffer");
            }

            const uint32_t handleSizeAligned = alignedSize(rtProperties.shaderGroupHandleSize, rtProperties.shaderGroupHandleAlignment);

            VkStridedDeviceAddressRegionKHR raygenSBTEntry{};
            raygenSBTEntry.deviceAddress    = raygenSBT->deviceAddress();
            raygenSBTEntry.size             = handleSizeAligned;
            raygenSBTEntry.stride           = handleSizeAligned;

            VkStridedDeviceAddressRegionKHR missSBTEntry{};
            missSBTEntry.deviceAddress      = missSBT->deviceAddress();
            missSBTEntry.size               = handleSizeAligned;
            missSBTEntry.stride             = handleSizeAligned;

            VkStridedDeviceAddressRegionKHR hitSBTEntry{};
            hitSBTEntry.deviceAddress       = hitSBT->deviceAddress();
            hitSBTEntry.size                = handleSizeAligned;
            hitSBTEntry.stride              = handleSizeAligned;

            VkStridedDeviceAddressRegionKHR callableSBTEntry{};
            // TODO rt test this outside the loop ^

            vkCmdBindPipeline(renderer.commandBuffers[i], VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline);
            vkCmdBindDescriptorSets(renderer.commandBuffers[i], VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipelineLayout, 0, 1, &descriptorSets[i], 0, 0);
            device.vkCmdTraceRaysKHR(renderer.commandBuffers[i], &raygenSBTEntry, &missSBTEntry, &hitSBTEntry, &callableSBTEntry, WIDTH, HEIGHT, 1);

            vkhelper::transitionImageLayout(
                renderer.commandBuffers[i], 
                renderer.getSwapchain().getImages()[i], 
                VK_IMAGE_LAYOUT_UNDEFINED, 
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
            );

            vkhelper::transitionImageLayout(
                renderer.commandBuffers[i], 
                storageImage.image, 
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
            imageCopy.extent            = {WIDTH, HEIGHT, 1};
            
            vkCmdCopyImage(
                renderer.commandBuffers[i], storageImage.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                renderer.getSwapchain().getImages()[i], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageCopy
            );

            vkhelper::transitionImageLayout(
                renderer.commandBuffers[i], 
                renderer.getSwapchain().getImages()[i], 
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
            );

            vkhelper::transitionImageLayout(
                renderer.commandBuffers[i], 
                storageImage.image, 
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 
                VK_ACCESS_TRANSFER_READ_BIT,
                {},
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                VK_IMAGE_LAYOUT_GENERAL,
                subresourceRange
            );

            if (vkEndCommandBuffer(renderer.commandBuffers[i])) {
                throw std::runtime_error("Failed to end command buffer");
            }
        }
    }
}