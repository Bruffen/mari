#include "mari.hpp"

#include "keyboard_controller.hpp"
#include "buffer.hpp"
#include "camera.hpp"
#include "systems/simple_render_system.hpp"
#include "systems/point_light_system.hpp"

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
            .setMaxSets(Swapchain::MAX_FRAMES_IN_FLIGHT)
            .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, Swapchain::MAX_FRAMES_IN_FLIGHT)
            .build();
        
        loadGameObjects();
    }

    Mari::~Mari() {

    }

    void Mari::run() { 
        std::vector<std::unique_ptr<Buffer>> uboBuffers{Swapchain::MAX_FRAMES_IN_FLIGHT};

        for (int i = 0; i < uboBuffers.size(); i++) {
            uboBuffers[i] = std::make_unique<Buffer>(
                device,
                sizeof(GlobalUbo),
                1,
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
            );
            uboBuffers[i]->map();
        }

        auto globalSetLayout = DescriptorSetLayout::Builder(device)
            .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
            .build();

        std::vector<VkDescriptorSet> globalDescriptorSets(Swapchain::MAX_FRAMES_IN_FLIGHT);
        for (int i = 0; i < globalDescriptorSets.size(); i++) {
            auto bufferInfo = uboBuffers[i]->descriptorInfo();
            DescriptorWriter(*globalSetLayout, *globalPool)
                .writeBuffer(0, &bufferInfo)
                .build(globalDescriptorSets[i]);
        }

        SimpleRenderSystem simpleRenderSystem{device, renderer.getSwapchainRenderPass(), globalSetLayout->getDescriptorSetLayout()};
        PointLightSystem pointLightSystem{device, renderer.getSwapchainRenderPass(), globalSetLayout->getDescriptorSetLayout()};
        Camera camera{};
        
        // TODO how do I keep multiple pointers for each callback
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
                GlobalUbo ubo{};
                ubo.projection = camera.getProjection();
                ubo.view = camera.getView();
                ubo.inverseView = camera.getInverseView();
                pointLightSystem.update(frameInfo, ubo);
                uboBuffers[frameIndex]->writeToBuffer(&ubo);
                uboBuffers[frameIndex]->flush();

                // render
                renderer.beginSwapchainRenderPass(commandBuffer);

                simpleRenderSystem.renderGameObjects(frameInfo);
                pointLightSystem.render(frameInfo);
                
                renderer.endSwapchainRenderPass(commandBuffer);
                renderer.endFrame();
            }
        }

        vkDeviceWaitIdle(device.device());
    };

    std::unique_ptr<Model> createCubeModel(Device& device, glm::vec3 offset) {
        Model::Builder modelBuilder{};
        modelBuilder.vertices = {
            // left face (white)
            {{-.5f, -.5f, -.5f}, {.9f, .9f, .9f}},
            {{-.5f, .5f, .5f}, {.9f, .9f, .9f}},
            {{-.5f, -.5f, .5f}, {.9f, .9f, .9f}},
            {{-.5f, .5f, -.5f}, {.9f, .9f, .9f}},
        
            // right face (yellow)
            {{.5f, -.5f, -.5f}, {.8f, .8f, .1f}},
            {{.5f, .5f, .5f}, {.8f, .8f, .1f}},
            {{.5f, -.5f, .5f}, {.8f, .8f, .1f}},
            {{.5f, .5f, -.5f}, {.8f, .8f, .1f}},
        
            // top face (orange, remember y axis points down)
            {{-.5f, -.5f, -.5f}, {.9f, .6f, .1f}},
            {{.5f, -.5f, .5f}, {.9f, .6f, .1f}},
            {{-.5f, -.5f, .5f}, {.9f, .6f, .1f}},
            {{.5f, -.5f, -.5f}, {.9f, .6f, .1f}},
        
            // bottom face (red)
            {{-.5f, .5f, -.5f}, {.8f, .1f, .1f}},
            {{.5f, .5f, .5f}, {.8f, .1f, .1f}},
            {{-.5f, .5f, .5f}, {.8f, .1f, .1f}},
            {{.5f, .5f, -.5f}, {.8f, .1f, .1f}},
        
            // nose face (blue)
            {{-.5f, -.5f, 0.5f}, {.1f, .1f, .8f}},
            {{.5f, .5f, 0.5f}, {.1f, .1f, .8f}},
            {{-.5f, .5f, 0.5f}, {.1f, .1f, .8f}},
            {{.5f, -.5f, 0.5f}, {.1f, .1f, .8f}},
        
            // tail face (green)
            {{-.5f, -.5f, -0.5f}, {.1f, .8f, .1f}},
            {{.5f, .5f, -0.5f}, {.1f, .8f, .1f}},
            {{-.5f, .5f, -0.5f}, {.1f, .8f, .1f}},
            {{.5f, -.5f, -0.5f}, {.1f, .8f, .1f}},
        };
        for (auto& v : modelBuilder.vertices) {
            v.position += offset;
        }
 
        modelBuilder.indices = {
            0,  1,  2,  0,  3,  1,  4,  5,  6,  4,  7,  5,  8,  9,  10, 8,  11, 9,
            12, 13, 14, 12, 15, 13, 16, 17, 18, 16, 19, 17, 20, 21, 22, 20, 23, 21
        };
        
        return std::make_unique<Model>(device, modelBuilder);
    }

    void Mari::loadGameObjects() {
        //std::shared_ptr<Model> model = createCubeModel(device, {0.0f, 0.0f, 0.0f});
        //std::shared_ptr<Model> model = Model::createModelFromFile(device, "../../../../_Models/CornellBox/CornellBox-Original.obj");

        std::shared_ptr<Model> model = Model::createModelFromFile(device, "../../../../_Models/DOA/marie_rose_twinkle_rose/marie_rose_twinkle_rose_standing1.obj");
        auto gameObject = GameObject::createGameObject();
        gameObject.model = model;
        gameObject.transform.translation = {0.0f, -0.01f, 0.0f};
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
}