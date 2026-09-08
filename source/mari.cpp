#pragma once

#include "mari.hpp"

#include "buffer.hpp"
#include "components/camera.hpp"
#include "keyboard_controller.hpp"
#include "systems/ray_tracing_system.hpp"

#include "components/volume.hpp"

#include "test_scenes.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include <array>
#include <chrono>
#include <cassert>
#include <stdexcept>

namespace mari {
    Mari::Mari() {
        DefaultObjects::initialize(device);        

        gui = std::make_unique<Gui>(device, window, renderer, rayTracingSystem);
        scene = TestScenes::loadScene(device, window, 5);
        scene->start();

        rayTracingSystem.buildScene(*scene);

        gui->set(scene);

        globalPool = DescriptorPool::Builder(device)
            .setMaxSets(Swapchain::MAX_FRAMES_IN_FLIGHT)
            .addPoolSize(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, Swapchain::MAX_FRAMES_IN_FLIGHT)
            .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, Swapchain::MAX_FRAMES_IN_FLIGHT)
            .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, Swapchain::MAX_FRAMES_IN_FLIGHT * 3)
            .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, Swapchain::MAX_FRAMES_IN_FLIGHT)
            .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, static_cast<uint32_t>(scene->images.size() + scene->lightObjects.size()))

            .build();
    }

    void Mari::run() { 
        std::vector<std::unique_ptr<Buffer>> rayTracingUboBuffers{Swapchain::MAX_FRAMES_IN_FLIGHT};
        std::vector<std::unique_ptr<Buffer>> infiniteLightUboBuffers{Swapchain::MAX_FRAMES_IN_FLIGHT};
        std::vector<std::unique_ptr<Buffer>> lightUboBuffers{Swapchain::MAX_FRAMES_IN_FLIGHT};
        std::vector<std::unique_ptr<Buffer>> volumeUboBuffers{Swapchain::MAX_FRAMES_IN_FLIGHT};
        for (int i = 0; i < Swapchain::MAX_FRAMES_IN_FLIGHT; i++) {
            rayTracingUboBuffers[i] = std::make_unique<Buffer>(device, sizeof(RayTracingUbo), 1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
            rayTracingUboBuffers[i]->map();
            infiniteLightUboBuffers[i] = std::make_unique<Buffer>(device, sizeof(InfiniteLightUbo), 1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
            infiniteLightUboBuffers[i]->map();
            lightUboBuffers[i] = std::make_unique<Buffer>(device, sizeof(LightUbo), 1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
            lightUboBuffers[i]->map();
            volumeUboBuffers[i] = std::make_unique<Buffer>(device, sizeof(VolumeUbo), 1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
            volumeUboBuffers[i]->map();
        }

        /* TODO create different descriptor set for stuff that doesn't change every frame
         * 0 -> Top-Level Acceleration Structure
         * 1 -> Accumulation image
         * 2 -> Present image
         * 3 -> Path tracing properties uniform
         * 4 -> Environment map data uniform
         * 5 -> Lights data uniform
         * 6 -> Volume data uniform
         * 7 -> Mesh information buffer
         * 8 -> Textures buffer
         */
        const uint32_t imageCount = static_cast<uint32_t>(scene->images.size() + scene->lightObjects.size());
        DescriptorSetLayout rayTracingSetLayout = DescriptorSetLayout::Builder(device)
            .addBinding(0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
            .addBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE             , VK_SHADER_STAGE_RAYGEN_BIT_KHR)
            .addBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE             , VK_SHADER_STAGE_RAYGEN_BIT_KHR)
            .addBinding(3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER            , VK_SHADER_STAGE_RAYGEN_BIT_KHR)
            .addBinding(4, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER            , VK_SHADER_STAGE_RAYGEN_BIT_KHR)
            .addBinding(5, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER            , VK_SHADER_STAGE_RAYGEN_BIT_KHR)
            .addBinding(6, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER            , VK_SHADER_STAGE_RAYGEN_BIT_KHR)
            .addBinding(7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER            , VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR)
            .addBinding(8, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER    , VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR, 
                            // TODO if image count == 1, call VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER instead of variable descriptor
                            imageCount, VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT) 
            .build();
        
        rayTracingSystem.buildPipeline(rayTracingSetLayout.handle(), Integrator::PATH_TRACING_VOLUMETRIC);

        std::vector<VkDescriptorImageInfo> textureDescriptors{};
        for (auto &image : scene->images) {
            textureDescriptors.push_back(image->descriptorInfo());
        }
        for (auto &node : scene->lightObjects) {
            textureDescriptors.push_back(std::dynamic_pointer_cast<InfiniteAreaLight>(node->light)->equalAreaImage->descriptorInfo());
        }

        std::vector<VkDescriptorSet> rayTracingDescriptorSets(Swapchain::MAX_FRAMES_IN_FLIGHT);
        for (int i = 0; i < rayTracingDescriptorSets.size(); i++) {
            DescriptorWriter(rayTracingSetLayout, *globalPool)
                .writeAccelerationStructure(0, rayTracingSystem.tlas->descriptor())
                .writeImage(                1, rayTracingSystem.accumImage->descriptorInfo())
                .writeImage(                2, rayTracingSystem.presentImage->descriptorInfo())
                .writeBuffer(               3, rayTracingUboBuffers[i]->descriptorInfo())
                .writeBuffer(               4, infiniteLightUboBuffers[i]->descriptorInfo())
                .writeBuffer(               5, lightUboBuffers[i]->descriptorInfo())
                .writeBuffer(               6, volumeUboBuffers[i]->descriptorInfo())
                .writeBuffer(               7, rayTracingSystem.pPrimMeshesInfosBuffer->descriptorInfo())
                .writeImages(               8, &textureDescriptors)
                .build(rayTracingDescriptorSets[i]);
        }

        // TODO how to keep multiple pointers for each callback
        //glfwSetWindowUserPointer(window.getGLFWwindow(), &camera);
        //glfwSetScrollCallback(window.getGLFWwindow(), camera.scrollCallback);
        glfwSetWindowUserPointer(window.getGLFWwindow(), &window);

        std::shared_ptr<Node> freeCamera = std::make_shared<Node>();
        freeCamera->name = "Free Camera";
        freeCamera->camera = std::make_shared<Camera>();
        freeCamera->transform.position.y = 0.7f;
        freeCamera->transform.position.z = 1.5f;
        freeCamera->isStatic = false;
        scene->addNode(freeCamera);
        scene->cameraObjects.emplace_back(freeCamera);
        if (!scene->currentCamera) {
            scene->currentCamera = freeCamera;
        }
        
        KeyboardController controller{*gui};
        
        auto startTime      = std::chrono::high_resolution_clock::now();
        auto currentTime    = startTime;
        int  frameCounter   = 0;

        while (!window.shouldClose()) {
            glfwPollEvents();

            auto  newTime     = std::chrono::high_resolution_clock::now();
            float deltaTime   = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
            float elapsedTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime - startTime).count();
            currentTime = newTime;
            frameCounter++;

            // If window is resized and aspect ratio is different
            float aspect = renderer.getAspectRatio();
            scene->currentCamera->camera->setAspectRatio(aspect);

            // TODO on camera move when free camera isn't set, set free camera's transform equal to current camera and change to free camera
            controller.update(window.getGLFWwindow(), deltaTime, *scene->currentCamera);
            scene->update();

            bool rebuild = rayTracingSystem.needsRebuild;
            // Wait until the trace rays command is over before rebuilding the acceleration structure in use
            if (rebuild) {
                // TODO implementation actual synchronization or double buffering the acceleration struture
                vkDeviceWaitIdle(device.handle()); 
            }

            rayTracingSystem.update(*scene);

            if (rebuild) {
                for (int i = 0; i < rayTracingDescriptorSets.size(); i++) {
                    DescriptorWriter(rayTracingSetLayout, *globalPool)
                        .writeAccelerationStructure(0, rayTracingSystem.tlas->descriptor())
                        .overwrite(rayTracingDescriptorSets[i]);
                }
            }

            if (auto commandBuffer = renderer.beginFrame()) {
                int frameIndex = renderer.getFrameIndex();
                frameCounter = controller.checkFrameAccumulationReset() ? 0 : frameCounter;
                FrameInfo frameInfo {
                    frameIndex,
                    frameCounter,
                    deltaTime,
                    elapsedTime,
                    commandBuffer,
                    *scene->currentCamera,
                    rayTracingDescriptorSets[frameIndex],
                    scene->nodes
                };

                // update
                RayTracingUbo ubo{};
                ubo.viewInverse         = scene->currentCamera->camera->getInverseView();
                ubo.projInverse         = scene->currentCamera->camera->getInverseProjection();
                ubo.frameCount          = frameCounter;
                ubo.maxDepth            = rayTracingSystem.maxDepth;
                ubo.frameAccumulation   = static_cast<VkBool32>(rayTracingSystem.frameAccumulation);
                ubo.russianRoulette     = static_cast<VkBool32>(rayTracingSystem.russianRoulette);
                ubo.nextEventEstimation = static_cast<VkBool32>(rayTracingSystem.nextEventEstimation);
                ubo.exposure            = rayTracingSystem.exposure;
                ubo.tonemapper          = rayTracingSystem.tonemapper;
                ubo.transmittanceAlgo   = rayTracingSystem.transmittanceAlgo;
                ubo.samplesPerPixel     = rayTracingSystem.samplesPerPixel;
                rayTracingUboBuffers[frameIndex]->writeToBuffer(&ubo);
                rayTracingUboBuffers[frameIndex]->flush();

                const int lightID = scene->environmentID - static_cast<int>(scene->images.size());
                const InfiniteAreaLight* currentInfiniteLight = std::dynamic_pointer_cast<InfiniteAreaLight>(scene->lightObjects[lightID]->light).get();
                InfiniteLightUbo infiniteLight = { // TODO diferent descriptor set
                    scene->environmentID,
                    rayTracingSystem.environmentRotation,
                    currentInfiniteLight->sampler.getMarginal()->getIntegral(),
                    glm::uvec2(currentInfiniteLight->textureSize, currentInfiniteLight->textureSize),
                    currentInfiniteLight->sampler.getMarginal()->getFunctionBuffer()->deviceAddress(),
                    currentInfiniteLight->sampler.getMarginal()->getCdfBuffer()->deviceAddress(),
                    currentInfiniteLight->sampler.conditionalIntegralsBuffer->deviceAddress(),
                    currentInfiniteLight->sampler.conditionalFunctionsBuffer->deviceAddress(),
                    currentInfiniteLight->sampler.conditionalCdfsBuffer->deviceAddress()
                };

                infiniteLightUboBuffers[frameIndex]->writeToBuffer(&infiniteLight);
                infiniteLightUboBuffers[frameIndex]->flush();

                LightUbo lightUbo = {
                    rayTracingSystem.lightsBuffer->deviceAddress(),
                    rayTracingSystem.lightsSampler.getFunctionBuffer()->deviceAddress(),
                    rayTracingSystem.lightsSampler.getCdfBuffer()->deviceAddress(),
                    rayTracingSystem.lightsSampler.getIntegral(),
                    static_cast<int>(rayTracingSystem.lights.size())
                };

                lightUboBuffers[frameIndex]->writeToBuffer(&lightUbo);
                lightUboBuffers[frameIndex]->flush();

                if (scene->volumeObject) {
                    VolumeUbo volumeUbo = {
                        scene->volumeObject->volume->getDensityDeviceAddress(),
                        scene->volumeObject->volume->getTemperatureDeviceAddress(),
                        scene->volumeObject->volume->medium,
                        scene->volumeObject->volume->temperatureMultiplier,
                        scene->volumeObject->volume->emissivenessMultiplier,
                        scene->volumeObject->volume->jitteringAmount
                    };
                    volumeUboBuffers[frameIndex]->writeToBuffer(&volumeUbo);
                    volumeUboBuffers[frameIndex]->flush();
                }

                // render
                rayTracingSystem.render(frameInfo, renderer.getSwapchain());

                // GUI
                if (gui->isActive) {
                    gui->prepare(frameInfo);
                    renderer.beginSwapchainRenderPass(commandBuffer);
                    gui->render(commandBuffer);
                    renderer.endSwapchainRenderPass(commandBuffer);
                }

                renderer.endFrame();        

/*
                if (frameCounter == 32) {
                    vkDeviceWaitIdle(device.handle());
                    gui->saveImage(*rayTracingSystem.presentImage);
                }
*/
                if (window.wasWindowResized()) { // TODO make this cleaner
                    window.resetWindowsResizedFlag();
                    frameCounter = 0; // TODO put each one in one place;

                    vkDeviceWaitIdle(device.handle());

                    VkExtent2D e = window.getExtent();
                    rayTracingSystem.accumImage->resize(e.width, e.height);
                    rayTracingSystem.presentImage->resize(e.width, e.height);

                    for (int i = 0; i < rayTracingDescriptorSets.size(); i++) {
                        DescriptorWriter(rayTracingSetLayout, *globalPool)
                            .writeImage(1, rayTracingSystem.accumImage->descriptorInfo())
                            .writeImage(2, rayTracingSystem.presentImage->descriptorInfo())
                            .overwrite(rayTracingDescriptorSets[i]);
                    }
                }
            }
        }

        vkDeviceWaitIdle(device.handle());
    };

    Mari::~Mari() {
        DefaultObjects::cleanup(device);
    }
}