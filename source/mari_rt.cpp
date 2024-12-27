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

namespace mari {

    MariRT::MariRT() {
        globalPool = DescriptorPool::Builder(device)
            .setMaxSets(Swapchain::MAX_FRAMES_IN_FLIGHT)
            .addPoolSize(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, Swapchain::MAX_FRAMES_IN_FLIGHT)
            .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, Swapchain::MAX_FRAMES_IN_FLIGHT)
            .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, Swapchain::MAX_FRAMES_IN_FLIGHT)
            .build();
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

    void MariRT::run() { 
        loadGameObjects();
        createUniformBuffers();

        std::unique_ptr<mari::DescriptorSetLayout> globalSetLayout = DescriptorSetLayout::Builder(device)
            .addBinding(0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_RAYGEN_BIT_KHR)
            .addBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE             , VK_SHADER_STAGE_RAYGEN_BIT_KHR)
            .addBinding(2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER            , VK_SHADER_STAGE_RAYGEN_BIT_KHR)
            .build();

        RayTracingSystem rayTracingSystem{device, window, gameObjects, globalSetLayout->handle()};

        std::vector<VkDescriptorSet> globalDescriptorSets(Swapchain::MAX_FRAMES_IN_FLIGHT);
        for (int i = 0; i < globalDescriptorSets.size(); i++) {
            DescriptorWriter(*globalSetLayout, *globalPool)
                .writeAccelerationStructure(0, &rayTracingSystem.tlas.descriptor())
                .writeImage(                1, &rayTracingSystem.accumImage->descriptorInfo())
                .writeBuffer(               2, &uboBuffers[i]->descriptorInfo())
                .build(globalDescriptorSets[i]);
        }

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
            
            if (auto commandBuffer = renderer.beginFrame()) {
                int frameIndex = renderer.getFrameIndex();
                FrameInfo frameInfo {
                    frameIndex,
                    frameTime,
                    elapsedTime,
                    commandBuffer,
                    camera,
                    globalDescriptorSets[frameIndex],
                    gameObjects
                };
                
                // update
                UniformData ubo{};
                ubo.viewInverse = camera.getInverseView();
                ubo.projInverse = camera.getInverseProjection();

                uboBuffers[frameIndex]->writeToBuffer(&ubo);
                uboBuffers[frameIndex]->flush();

                rayTracingSystem.render(frameInfo, renderer.getSwapchain());

                renderer.endFrame();

                if (window.wasWindowResized()) { // TODO make this cleaner
                    window.resetWindowsResizedFlag();

                    VkExtent2D e = window.getExtent();
                    rayTracingSystem.accumImage->resize(e.width, e.height);

                    for (int i = 0; i < globalDescriptorSets.size(); i++) {
                        DescriptorWriter::DescriptorWriter(*globalSetLayout, *globalPool)
                            .writeImage(1, &rayTracingSystem.accumImage->descriptorInfo())
                            .overwrite(globalDescriptorSets[i]);
                    }
                }
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
}