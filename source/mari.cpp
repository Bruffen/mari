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

namespace mari {
    Mari::Mari() {
        DefaultObjects::initialize(device);
        gui = std::make_unique<Gui>(device, window, renderer);
        loadScene();
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
        for (int i = 0; i < rasterizationUboBuffers.size(); i++) {
            rasterizationUboBuffers[i] = std::make_unique<Buffer>(
                device,
                sizeof(RasterizationUbo),
                1,
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
            );
            rasterizationUboBuffers[i]->map();
        }

        std::vector<std::unique_ptr<Buffer>> rayTracingUboBuffers{Swapchain::MAX_FRAMES_IN_FLIGHT};
        for (int i = 0; i < rayTracingUboBuffers.size(); i++) {
            rayTracingUboBuffers[i] = std::make_unique<Buffer>(
                device,
                sizeof(RayTracingUbo),
                1,
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
            );
            rayTracingUboBuffers[i]->map();
        }

        std::unique_ptr<mari::DescriptorSetLayout> rasterizationSetLayout = DescriptorSetLayout::Builder(device)
            .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
            .build();

        const uint32_t imageCount = static_cast<uint32_t>(scene->images.size());
        std::unique_ptr<mari::DescriptorSetLayout> rayTracingSetLayout = DescriptorSetLayout::Builder(device)
            .addBinding(0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
            .addBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE             , VK_SHADER_STAGE_RAYGEN_BIT_KHR)
            .addBinding(2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER            , VK_SHADER_STAGE_RAYGEN_BIT_KHR)
            .addBinding(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER            , VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR)
            .addBinding(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER    , VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR, 
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
        RayTracingSystem rayTracingSystem{device, window, *scene, rayTracingSetLayout->handle()};
        std::shared_ptr<Camera> freeCamera = std::make_shared<Camera>();

        std::vector<VkDescriptorImageInfo> textureDescriptors{};
        for (auto &image : scene->images) {
            textureDescriptors.push_back(image->descriptorInfo());
        }

        std::vector<VkDescriptorSet> rayTracingDescriptorSets(Swapchain::MAX_FRAMES_IN_FLIGHT);
        for (int i = 0; i < rayTracingDescriptorSets.size(); i++) {
            DescriptorWriter(*rayTracingSetLayout, *globalPool)
                .writeAccelerationStructure(0, &rayTracingSystem.tlas->descriptor())
                .writeImage(                1, &rayTracingSystem.accumImage->descriptorInfo())
                .writeBuffer(               2, &rayTracingUboBuffers[i]->descriptorInfo())
                .writeBuffer(               3, &rayTracingSystem.pPrimMeshesInfosBuffer->descriptorInfo())
                .writeImages(               4, &textureDescriptors)
                .build(rayTracingDescriptorSets[i]);
        }

        // TODO how to keep multiple pointers for each callback
        //glfwSetWindowUserPointer(window.getGLFWwindow(), &camera);
        //glfwSetScrollCallback(window.getGLFWwindow(), camera.scrollCallback);
        glfwSetWindowUserPointer(window.getGLFWwindow(), &window);

        std::shared_ptr<GameObject> cameraObject = std::make_shared<GameObject>();
        std::shared_ptr<GameObject> currentCamera = cameraObject;
        cameraObject->name = "Free Camera";
        cameraObject->camera = freeCamera;
        cameraObject->transform.position.y = -0.7f;
        cameraObject->transform.position.z = -1.5f;
        scene->topNodes.emplace_back(cameraObject);
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

            if (controller.changeCamera(window.getGLFWwindow())) {
                currentCamera = scene->cameraObjects[0]; // TODO
            }

            // If window is resized and aspect ratio is different
            float aspect = renderer.getAspectRatio();
            currentCamera->camera->setAspectRatio(aspect);

            controller.update(window.getGLFWwindow(), deltaTime, *currentCamera);
            currentCamera->update(); // TODO call every gameobject's update
            
            if (auto commandBuffer = renderer.beginFrame()) {
                int frameIndex = renderer.getFrameIndex();
                FrameInfo frameInfo {
                    frameIndex,
                    frameCounter,
                    deltaTime,
                    elapsedTime,
                    commandBuffer,
                    *currentCamera->camera,
                    controller.isRayTracingOn() ? rayTracingDescriptorSets[frameIndex] : rasterizationDescriptorSets[frameIndex],
                    gameObjects
                };

                if (controller.isRayTracingOn()) {
                    // update
                    RayTracingUbo ubo{};
                    ubo.viewInverse = currentCamera->camera->getInverseView();
                    ubo.projInverse = currentCamera->camera->getInverseProjection();
                    frameCounter    = controller.checkFrameAccumulationReset() ? 0 : frameCounter;
                    ubo.frameCount  = frameCounter;
                    rayTracingUboBuffers[frameIndex]->writeToBuffer(&ubo);
                    rayTracingUboBuffers[frameIndex]->flush();

                    // render
                    rayTracingSystem.render(frameInfo, renderer.getSwapchain());
                }
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

                    for (int i = 0; i < rayTracingDescriptorSets.size(); i++) {
                        DescriptorWriter::DescriptorWriter(*rayTracingSetLayout, *globalPool)
                            .writeImage(1, &rayTracingSystem.accumImage->descriptorInfo())
                            .overwrite(rayTracingDescriptorSets[i]);
                    }
                }
            }
        }

        vkDeviceWaitIdle(device.handle());
    };

    void Mari::loadScene() {
        //scene = std::make_shared<Scene>(device, "../../models/FlightHelmet/glTF/FlightHelmet.gltf");
        //scene = std::make_shared<Scene>(device, "../../../../_Models/gltf/bistro_interior.glb");
        //scene = std::make_shared<Scene>(device, "../../../../_Models/gltf/living_room.glb");
        //scene = std::make_shared<Scene>(device, "../../../../_Models/gltf/glTF-Sample-Models/2.0/Sponza/glTF/Sponza.gltf");

        switch (  0  ) {
            case 0:
                scene = std::make_shared<Scene>(device, "../../../../_Models/DOA/marie_rose_twinkle_rose/marie_rose_twinkle_rose_standing1.glb");
                scene->transform.position = {0.0f, -0.01f, 0.0f};
                scene->transform.rotation = glm::vec3(glm::radians(180.0f), 0.0f, 0.0f);
                break;
            case 1:
                scene = std::make_shared<Scene>(device, "../../../../_Models/gltf/glTF-Sample-Models/2.0/ABeautifulGame/glTF/ABeautifulGame.glTF"); // TODO this scene uses instancing
                scene->transform.rotation = glm::vec3(glm::radians(180.0f), 0.0f, 0.0f);
                break;
            case 2:
                scene = std::make_shared<Scene>(device, "../../../../_Models/gltf/mylivingroom.glb");
                scene->transform.rotation = glm::vec3(glm::radians(180.0f), 0.0f, 0.0f);
                break;
            case 3:
                scene = std::make_shared<Scene>(device, "../../../../_Models/gltf/box.glb");
                scene->transform.position = {0.0f, 0.0f, 0.0f};
                scene->transform.rotation = glm::vec3(glm::radians(-90.0f), glm::radians(0.0f), 0.0f);
                scene->transform.scale = {0.2f, 0.2, 0.2f};
                break;
            case 4:
                scene = std::make_shared<Scene>(device, "../../../../_Models/gltf/bistro_exterior.glb");
                scene->transform.rotation = glm::vec3(glm::radians(180.0f), 0.0f, 0.0f);
                break;
            case 5:
                scene = std::make_shared<Scene>(device, "../../../../_Models/gltf/sphere.glb");
                break;
        }
        scene->update();
        
        /*std::vector<glm::vec3> lightColors {
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
        }*/
    }
}