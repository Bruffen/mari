#pragma once

#include "mari.hpp"

#include "buffer.hpp"
#include "components/camera.hpp"
#include "keyboard_controller.hpp"
#include "systems/simple_render_system.hpp"
#include "systems/point_light_system.hpp"
#include "systems/ray_tracing_system.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include <array>
#include <chrono>
#include <cassert>
#include <stdexcept>
#include <iostream>




#ifndef STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#endif

namespace mari {
    Mari::Mari() {
        DefaultObjects::initialize(device);

        gui = std::make_unique<Gui>(device, window, renderer, rayTracingSystem);
        loadScene();

        for (auto const &[id, g] : scene->nodes) {
            g->start();
        }

        rayTracingSystem.buildScene(*scene);
        rayTracingSystem.tonemapper = 2;

        int width, height, nrChannels;
        float* data = nullptr;
        //data = stbi_loadf("../../models/brown_photostudio_01_4k.hdr", &width, &height, &nrChannels, 4);
        //data = stbi_loadf("../../models/solitude_interior_8k.hdr", &width, &height, &nrChannels, 4);
        //data = stbi_loadf("../../models/meadow_8k.hdr", &width, &height, &nrChannels, 4);
        data = stbi_loadf("../../models/qwantani_noon_8k.hdr", &width, &height, &nrChannels, 4);
        //data = stbi_loadf("../../../../_Models/gltf/IntelSponza/main1_sponza/textures/kloppenheim_05_4k.hdr", &width, &height, &nrChannels, 4);
        // TODO check for image load fail

        std::shared_ptr<Image> environment = std::make_unique<Image>(
            device, 
            VkExtent3D{static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1}, 
            VK_FORMAT_R32G32B32A32_SFLOAT, 
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            (void*) data
        );
        environment->name = "environment"; 
        scene->environments.push_back(environment);
        //scene->environments.push_back(DefaultObjects::getImageWhite());
        scene->environments.push_back(DefaultObjects::getImageBlack());
        
        gui->set(scene);

        globalPool = DescriptorPool::Builder(device)
            .setMaxSets(Swapchain::MAX_FRAMES_IN_FLIGHT * 2)
            // Rasterization
            .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, Swapchain::MAX_FRAMES_IN_FLIGHT)
            // Ray Tracing
            .addPoolSize(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, Swapchain::MAX_FRAMES_IN_FLIGHT)
            .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, Swapchain::MAX_FRAMES_IN_FLIGHT)
            .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, Swapchain::MAX_FRAMES_IN_FLIGHT)
            .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, Swapchain::MAX_FRAMES_IN_FLIGHT)
            .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, static_cast<uint32_t>(scene->images.size()))

            .build();
    }

    Mari::~Mari() {
        DefaultObjects::cleanup(device);
    }

    void Mari::run() { 
        std::vector<std::unique_ptr<Buffer>> rasterizationUboBuffers{Swapchain::MAX_FRAMES_IN_FLIGHT};
        std::vector<std::unique_ptr<Buffer>> rayTracingUboBuffers{Swapchain::MAX_FRAMES_IN_FLIGHT};
        for (int i = 0; i < Swapchain::MAX_FRAMES_IN_FLIGHT; i++) {
            rasterizationUboBuffers[i] = std::make_unique<Buffer>(device, sizeof(RasterizationUbo), 1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
            rasterizationUboBuffers[i]->map();
            rayTracingUboBuffers[i] = std::make_unique<Buffer>(device, sizeof(RayTracingUbo), 1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
            rayTracingUboBuffers[i]->map();
        }

        std::unique_ptr<mari::DescriptorSetLayout> rasterizationSetLayout = DescriptorSetLayout::Builder(device)
            .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
            .build();

        const uint32_t imageCount = static_cast<uint32_t>(scene->images.size());
        std::unique_ptr<mari::DescriptorSetLayout> rayTracingSetLayout = DescriptorSetLayout::Builder(device)
            .addBinding(0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
            .addBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE             , VK_SHADER_STAGE_RAYGEN_BIT_KHR)
            .addBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE             , VK_SHADER_STAGE_RAYGEN_BIT_KHR)
            .addBinding(3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER            , VK_SHADER_STAGE_RAYGEN_BIT_KHR)
            .addBinding(4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER            , VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR)
            .addBinding(5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER    , VK_SHADER_STAGE_MISS_BIT_KHR)
            .addBinding(6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER    , VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR, 
                        imageCount, VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT)
            .build();

        std::vector<VkDescriptorSet> rasterizationDescriptorSets(Swapchain::MAX_FRAMES_IN_FLIGHT);
        for (int i = 0; i < rasterizationDescriptorSets.size(); i++) {
            DescriptorWriter(*rasterizationSetLayout, *globalPool)
                .writeBuffer(0, &rasterizationUboBuffers[i]->descriptorInfo())
                .build(rasterizationDescriptorSets[i]);
        }

        SimpleRenderSystem simpleRenderSystem{device, renderer.getSwapchainRenderPass(), rasterizationSetLayout->handle()};
        PointLightSystem pointLightSystem{device, renderer.getSwapchainRenderPass(), rasterizationSetLayout->handle()};
        
        rayTracingSystem.buildPipeline(rayTracingSetLayout->handle());

        std::vector<VkDescriptorImageInfo> textureDescriptors{};
        for (auto &image : scene->images) {
            textureDescriptors.push_back(image->descriptorInfo());
        }

        std::vector<VkDescriptorSet> rayTracingDescriptorSets(Swapchain::MAX_FRAMES_IN_FLIGHT);
        for (int i = 0; i < rayTracingDescriptorSets.size(); i++) {
            DescriptorWriter(*rayTracingSetLayout, *globalPool)
                .writeAccelerationStructure(0, &rayTracingSystem.tlas->descriptor())
                .writeImage(                1, &rayTracingSystem.accumImage->descriptorInfo())
                .writeImage(                2, &rayTracingSystem.presentImage->descriptorInfo())
                .writeBuffer(               3, &rayTracingUboBuffers[i]->descriptorInfo())
                .writeBuffer(               4, &rayTracingSystem.pPrimMeshesInfosBuffer->descriptorInfo())
                .writeImage(                5, &scene->environments[0]->descriptorInfo())
                .writeImages(               6, &textureDescriptors)
                .build(rayTracingDescriptorSets[i]);
        }

        // TODO how to keep multiple pointers for each callback
        //glfwSetWindowUserPointer(window.getGLFWwindow(), &camera);
        //glfwSetScrollCallback(window.getGLFWwindow(), camera.scrollCallback);
        glfwSetWindowUserPointer(window.getGLFWwindow(), &window);

        std::shared_ptr<GameObject> cameraObject = std::make_shared<GameObject>();
        cameraObject->name = "Free Camera";
        cameraObject->camera = std::make_shared<Camera>();
        cameraObject->transform.position.y = -0.7f;
        cameraObject->transform.position.z = -1.5f;
        scene->topNodes.emplace_back(cameraObject);
        scene->cameraObjects.emplace_back(cameraObject);
        rayTracingSystem.currentCamera = cameraObject;
        
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
            rayTracingSystem.currentCamera->camera->setAspectRatio(aspect);

            // TODO on camera move when free camera isn't set, set free camera's transform equal to current camera and change to free camera
            controller.update(window.getGLFWwindow(), deltaTime, *rayTracingSystem.currentCamera);
            rayTracingSystem.currentCamera->update(); // TODO call every gameobject's update
            
            if (auto commandBuffer = renderer.beginFrame()) {
                int frameIndex = renderer.getFrameIndex();
                FrameInfo frameInfo {
                    frameIndex,
                    frameCounter,
                    deltaTime,
                    elapsedTime,
                    commandBuffer,
                    *rayTracingSystem.currentCamera,
                    controller.isRayTracingOn() ? rayTracingDescriptorSets[frameIndex] : rasterizationDescriptorSets[frameIndex],
                    gameObjects
                };

                if (controller.isRayTracingOn()) {
                    // update
                    RayTracingUbo ubo{};
                    ubo.viewInverse     = rayTracingSystem.currentCamera->camera->getInverseView();
                    ubo.projInverse     = rayTracingSystem.currentCamera->camera->getInverseProjection();
                    frameCounter        = controller.checkFrameAccumulationReset() ? 0 : frameCounter;
                    ubo.frameCount      = frameCounter;
                    ubo.maxDepth        = rayTracingSystem.maxDepth;
                    ubo.russianRoulette = rayTracingSystem.russianRoulette;
                    ubo.exposure        = rayTracingSystem.exposure;
                    ubo.tonemapper      = rayTracingSystem.tonemapper;
                    rayTracingUboBuffers[frameIndex]->writeToBuffer(&ubo);
                    rayTracingUboBuffers[frameIndex]->flush();

                    // render
                    rayTracingSystem.render(frameInfo, renderer.getSwapchain());
                }
                /*
                else {
                    // update
                    RasterizationUbo ubo{};
                    ubo.projection  = currentCamera->camera->getProjection();
                    ubo.view        = currentCamera->camera->getView();
                    ubo.inverseView = currentCamera->camera->getInverseView();
                    pointLightSystem.update(frameInfo, ubo);
                    rasterizationUboBuffers[frameIndex]->writeToBuffer(&ubo);
                    rasterizationUboBuffers[frameIndex]->flush();
                    
                    // render
                    renderer.beginSwapchainRenderPass(commandBuffer);
                    simpleRenderSystem.render(frameInfo);
                    pointLightSystem.render(frameInfo);
                    renderer.endSwapchainRenderPass(commandBuffer);
                }
                */

                // GUI
                
                gui->prepare(frameInfo);
                renderer.beginSwapchainRenderPass(commandBuffer);
                gui->render(commandBuffer);
                renderer.endSwapchainRenderPass(commandBuffer);
                
                renderer.endFrame();

                if (window.wasWindowResized()) { // TODO make this cleaner
                    window.resetWindowsResizedFlag();
                    frameCounter = 0; // TODO put everyone in one place;

                    vkDeviceWaitIdle(device.handle());

                    VkExtent2D e = window.getExtent();
                    rayTracingSystem.accumImage->resize(e.width, e.height);
                    rayTracingSystem.presentImage->resize(e.width, e.height);

                    for (int i = 0; i < rayTracingDescriptorSets.size(); i++) {
                        DescriptorWriter::DescriptorWriter(*rayTracingSetLayout, *globalPool)
                            .writeImage(1, &rayTracingSystem.accumImage->descriptorInfo())
                            .writeImage(2, &rayTracingSystem.presentImage->descriptorInfo())
                            .overwrite(rayTracingDescriptorSets[i]);
                    }
                }
            }
        }

        vkDeviceWaitIdle(device.handle());
    };

    void Mari::loadScene() {
        switch (  7  ) {
            case 0:
                scene = std::make_shared<Scene>(device, "../../../../_Models/DOA/marie_rose_twinkle_rose/marie_rose_twinkle_rose_standing1.glb");
                scene->transform.position = {0.0f, -0.01f, 0.0f};
                break;
            case 1:
                scene = std::make_shared<Scene>(device, "../../../../_Models/gltf/glTF-Sample-Models/2.0/ABeautifulGame/glTF/ABeautifulGame.glTF"); // TODO this scene uses instancing
                break;
            case 2:
                scene = std::make_shared<Scene>(device, "../../../../_Models/gltf/mylivingroom.glb");
                break;
            case 3:
                scene = std::make_shared<Scene>(device, "../../../../_Models/gltf/glTF-Sample-Models/2.0/Sponza/glTF/Sponza.gltf");
                break;
            case 4:
                //scene = std::make_shared<Scene>(device, "../../../../_Models/gltf/glTF-Sample-Models/2.0/OrientationTest/glTF-Binary/OrientationTest.glb");
                scene = std::make_shared<Scene>(device, "../../../../_Models/gltf/glTF-Sample-Models/2.0/GearboxAssy/glTF-Binary/GearboxAssy.glb");
                break;
            case 5:
                scene = std::make_shared<Scene>(device, "../../../../_Models/gltf/IntelSponza/intelsponza_curtains_ivy.glb");
                break;
            case 6:
                scene = std::make_shared<Scene>(device, "../../../../_Models/gltf/boxm.glb");
                break;
            case 7:
                scene = std::make_shared<Scene>(device, "../../../../_Models/gltf/bistro_exterior.glb");
                break;
            case 8:
                scene = std::make_shared<Scene>(device, "../../../../_Models/gltf/sphere.glb");
                break;
        }
        scene->transform.rotation = glm::vec3(glm::radians(180.0f), 0.0f, 0.0f);
        scene->update();
    }
}