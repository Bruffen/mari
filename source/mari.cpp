#pragma once

#include "mari.hpp"

#include "buffer.hpp"
#include "camera.hpp"
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

namespace mari {

    Mari::Mari() {
        globalPool = DescriptorPool::Builder(device)
            .setMaxSets(Swapchain::MAX_FRAMES_IN_FLIGHT * 2)
            // Rasterization
            .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, Swapchain::MAX_FRAMES_IN_FLIGHT)
            // Ray Tracing
            .addPoolSize(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, Swapchain::MAX_FRAMES_IN_FLIGHT)
            .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, Swapchain::MAX_FRAMES_IN_FLIGHT)
            .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, Swapchain::MAX_FRAMES_IN_FLIGHT)

            .build();

        loadGameObjects();
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

        std::unique_ptr<mari::DescriptorSetLayout> rayTracingSetLayout = DescriptorSetLayout::Builder(device)
            .addBinding(0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
            .addBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE             , VK_SHADER_STAGE_RAYGEN_BIT_KHR)
            .addBinding(2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER            , VK_SHADER_STAGE_RAYGEN_BIT_KHR)
            .build();

        std::vector<VkDescriptorSet> rasterizationDescriptorSets(Swapchain::MAX_FRAMES_IN_FLIGHT);
        for (int i = 0; i < rasterizationDescriptorSets.size(); i++) {
            DescriptorWriter(*rasterizationSetLayout, *globalPool)
                .writeBuffer(0, &rasterizationUboBuffers[i]->descriptorInfo())
                .build(rasterizationDescriptorSets[i]);
        }

        SimpleRenderSystem simpleRenderSystem{device, renderer.getSwapchainRenderPass(), rasterizationSetLayout->handle()};
        PointLightSystem pointLightSystem{device, renderer.getSwapchainRenderPass(), rasterizationSetLayout->handle()};
        RayTracingSystem rayTracingSystem{device, window, gameObjects, rayTracingSetLayout->handle()};
        Camera camera{};

        std::vector<VkDescriptorSet> rayTracingDescriptorSets(Swapchain::MAX_FRAMES_IN_FLIGHT);
        for (int i = 0; i < rayTracingDescriptorSets.size(); i++) {
            DescriptorWriter(*rayTracingSetLayout, *globalPool)
                .writeAccelerationStructure(0, &rayTracingSystem.tlas.descriptor())
                .writeImage(                1, &rayTracingSystem.accumImage->descriptorInfo())
                .writeBuffer(               2, &rayTracingUboBuffers[i]->descriptorInfo())
                .build(rayTracingDescriptorSets[i]);
        }

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

        bool isRayTracingOn = true;

        while (!window.shouldClose()) {
            glfwPollEvents();

            auto newTime = std::chrono::high_resolution_clock::now();
            float frameTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
            float elapsedTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime - startTime).count();
            currentTime = newTime;

            cameraController.rotateCamera(window.getGLFWwindow(), frameTime, cameraObject);
            cameraController.moveCamera(window.getGLFWwindow(), frameTime, cameraObject);
            cameraController.handleInput(window.getGLFWwindow(), isRayTracingOn);
            camera.setViewYXZ(cameraObject.transform.translation, cameraObject.transform.rotation);

            float aspect = renderer.getAspectRatio();
            //camera.setOrthographicProjection(-aspect, aspect, -1, 1, 0.1f, 1000.0f);
            camera.setPerspectiveProjection(aspect, 0.1f, 1000.0f);
            
            if (auto commandBuffer = renderer.beginFrame()) {
                int frameIndex = renderer.getFrameIndex();
                FrameInfo frameInfo {
                    frameIndex,
                    frameTime,
                    elapsedTime,
                    commandBuffer,
                    camera,
                    isRayTracingOn ? rayTracingDescriptorSets[frameIndex] : rasterizationDescriptorSets[frameIndex],
                    gameObjects
                };

                if (isRayTracingOn) {
                    // update
                    RayTracingUbo ubo{};
                    ubo.viewInverse = camera.getInverseView();
                    ubo.projInverse = camera.getInverseProjection();
                    rayTracingUboBuffers[frameIndex]->writeToBuffer(&ubo);
                    rayTracingUboBuffers[frameIndex]->flush();

                    // render
                    rayTracingSystem.render(frameInfo, renderer.getSwapchain());
                }
                else {
                    // update
                    RasterizationUbo ubo{};
                    ubo.projection = camera.getProjection();
                    ubo.view = camera.getView();
                    ubo.inverseView = camera.getInverseView();
                    pointLightSystem.update(frameInfo, ubo);
                    rasterizationUboBuffers[frameIndex]->writeToBuffer(&ubo);
                    rasterizationUboBuffers[frameIndex]->flush();

                    // render
                    renderer.beginSwapchainRenderPass(commandBuffer);
                    simpleRenderSystem.render(frameInfo);
                    pointLightSystem.render(frameInfo);
                    renderer.endSwapchainRenderPass(commandBuffer);
                }
                renderer.endFrame();

                if (window.wasWindowResized()) { // TODO make this cleaner
                    window.resetWindowsResizedFlag();

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

    void Mari::loadGameObjects() {
        //std::shared_ptr<Mesh> model = Mesh::createCubeModel(device, {0.0f, 0.0f, 0.0f});
        //std::shared_ptr<Mesh> model = Mesh::createModelFromFile(device, "../../../../_Models/CornellBox/CornellBox-Original.obj");
/*
        std::shared_ptr<Mesh> modelMarie = Mesh::createModelFromFile(device, "../../../../_Models/DOA/marie_rose_twinkle_rose/marie_rose_twinkle_rose_standing1.obj");
        auto gameObject = GameObject::createGameObject();
        gameObject.mesh = modelMarie;
        gameObject.transform.translation = {0.0f, -0.01f, 0.0f};
        gameObject.transform.rotation = {0.0f, glm::radians(180.0f), glm::radians(180.0f)};
        gameObject.transform.scale = glm::vec3{3.0f};
        gameObjects.emplace(gameObject.getId(), std::move(gameObject));

        std::shared_ptr<Mesh> modelVaseF = Mesh::createModelFromFile(device, "../../models/flat_vase.obj");
        auto gfvase = GameObject::createGameObject();
        gfvase.mesh = modelVaseF;
        gfvase.transform.translation = {1.0f, 0.0f, 0.0f};
        gfvase.transform.rotation = glm::vec3{0.0f};
        gfvase.transform.scale = glm::vec3{3.0f};
        gameObjects.emplace(gfvase.getId(), std::move(gfvase));

        std::shared_ptr<Mesh> modelVaseS = Mesh::createModelFromFile(device, "../../models/smooth_vase.obj");
        auto gsvase = GameObject::createGameObject();
        gsvase.mesh = modelVaseS;
        gsvase.transform.translation = {1.8f, 0.0f, 0.0f};
        gsvase.transform.rotation = glm::vec3{0.0f};
        gsvase.transform.scale = glm::vec3{3.0f};
        gameObjects.emplace(gsvase.getId(), std::move(gsvase));

        std::shared_ptr<Mesh> modelFloor = Mesh::createModelFromFile(device, "../../models/quad.obj");
        auto floor = GameObject::createGameObject();
        floor.mesh = modelFloor;
        floor.transform.translation = {0.0f, 0.0f, 0.0f};
        floor.transform.rotation = glm::vec3{0.0f};
        floor.transform.scale = glm::vec3{3.0f};
        gameObjects.emplace(floor.getId(), std::move(floor));
*/
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

        auto gameObject = GameObject::createGameObject();
        //scene = std::make_shared<Scene>(device, defaultObjects, "../../../../_Models/gltf/bistro_exterior.glb");
        //scene = std::make_shared<Scene>(device, defaultObjects, "../../../../_Models/gltf/the-white-room/the-white-room.gltf");
        scene = std::make_shared<Scene>(device, defaultObjects, "../../../../_Models/DOA/marie_rose_twinkle_rose/marie_rose_twinkle_rose_standing1.glb");
        gameObject.transform.translation = {0.0f, -0.01f, 0.0f};
        gameObject.transform.rotation = {glm::radians(-90.0f), 0.0f, glm::radians(0.0f)};
        gameObject.transform.scale = glm::vec3{3.0f};
        gameObject.mesh.reserve(scene->meshes.size());
        for (auto mesh : scene->meshes) {
            gameObject.mesh.emplace_back(mesh.second);
        }
        gameObjects.emplace(gameObject.getId(), std::move(gameObject));
    }
}