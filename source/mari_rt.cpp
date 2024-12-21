#pragma once

#include "mari_rt.hpp"

#include "keyboard_controller.hpp"
#include "buffer.hpp"
#include "camera.hpp"
#include "systems/simple_render_system.hpp"
#include "systems/point_light_system.hpp"
#include "systems/ray_tracing_system.hpp"
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
    }

    void MariRT::run() { 
        loadGameObjects();
        getRayTracingFunctionPointers(device.handle());
        createStorageImage();
        createUniformBuffers();

        // Create layout for descriptor sets
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

        RayTracingSystem rayTracingSystem{device, gameObjects, descriptorSetLayout};

        std::cout << "Creating shader binding tables..." << std::endl;

        /** Create descriptor sets */ // TODO
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
            accelerationStructureSet.pAccelerationStructures    = &rayTracingSystem.tlas.handle;

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
        /** */

        std::cout << "Building command buffers..." << std::endl;
        rayTracingSystem.buildCommandBuffers(renderer, descriptorSets, storageImage.image, WIDTH, HEIGHT);

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

                renderer.endFrame(false);
            }
        }

        vkDeviceWaitIdle(device.handle());
    };

    void MariRT::loadGameObjects() {
        //std::shared_ptr<Model> model = Model::createCubeModel(device, {0.0f, 0.0f, 0.0f});
        //std::shared_ptr<Model> model = Model::createModelFromFile(device, "../../../../_Models/CornellBox/CornellBox-Original.obj");

        std::shared_ptr<Model> modelMarie = Model::createModelFromFile(device, "../../../../_Models/DOA/marie_rose_twinkle_rose/marie_rose_twinkle_rose_standing1.obj");
        auto gameObject = GameObject::createGameObject();
        gameObject.model = modelMarie;
        gameObject.transform.translation = {0.0f, -0.01f, 0.0f};
        gameObject.transform.rotation = {0.0f, glm::radians(180.0f), glm::radians(180.0f)};
        gameObject.transform.scale = glm::vec3{3.0f};
        gameObjects.emplace(gameObject.getId(), std::move(gameObject));

        std::shared_ptr<Model> modelVaseF = Model::createModelFromFile(device, "../../models/flat_vase.obj");
        auto gfvase = GameObject::createGameObject();
        gfvase.model = modelVaseF;
        gfvase.transform.translation = {1.0f, 0.0f, 0.0f};
        gfvase.transform.rotation = glm::vec3{0.0f};
        gfvase.transform.scale = glm::vec3{3.0f};
        gameObjects.emplace(gfvase.getId(), std::move(gfvase));

        std::shared_ptr<Model> modelVaseS = Model::createModelFromFile(device, "../../models/smooth_vase.obj");
        auto gsvase = GameObject::createGameObject();
        gsvase.model = modelVaseS;
        gsvase.transform.translation = {1.8f, 0.0f, 0.0f};
        gsvase.transform.rotation = glm::vec3{0.0f};
        gsvase.transform.scale = glm::vec3{3.0f};
        gameObjects.emplace(gsvase.getId(), std::move(gsvase));

        std::shared_ptr<Model> modelFloor = Model::createModelFromFile(device, "../../models/quad.obj");
        auto floor = GameObject::createGameObject();
        floor.model = modelFloor;
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
        device.createImageWithInfo(imageCreateInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, storageImage.image, storageImage.memory);

        // TODO make a device createimageview method
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

        // TODO study barriers and try to incorporate it into single time command method
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
}