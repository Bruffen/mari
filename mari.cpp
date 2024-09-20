#include "mari.hpp"

#include "simple_render_system.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include <array>
#include <cassert>
#include <stdexcept>

namespace mari {
    Mari::Mari() {
        loadGameObjects();
    }

    Mari::~Mari() {

    }

    void Mari::run() { 
        SimpleRenderSystem simpleRenderSystem{device, renderer.getSwapchainRenderPass()};

        while (!window.shouldClose()) {
            glfwPollEvents();
            
            if (auto commandBuffer = renderer.beginFrame()) {
                renderer.beginSwapchainRenderPass(commandBuffer);
                simpleRenderSystem.renderGameObjects(commandBuffer, gameObjects);
                renderer.endSwapchainRenderPass(commandBuffer);
                renderer.endFrame();
            }
        }

        vkDeviceWaitIdle(device.device());
    };

    void Mari::loadGameObjects() {
        std::vector<Model::Vertex> vertices {
            {{ 0.0f, -0.5f}, {1.0, 0.0, 0.0}},
            {{ 0.5f,  0.5f}, {0.0, 1.0, 0.0}},
            {{-0.5f,  0.5f}, {0.0, 0.0, 1.0}}
        };

        auto model = std::make_shared<Model>(device, vertices);

        auto triangle = GameObject::createGameObject();
        triangle.model = model;
        triangle.color = { 0.1f, 0.8f, 0.1f };
        triangle.transform2d.translation.x = 0.2f;
        triangle.transform2d.scale = {2.0f, 0.5f};
        triangle.transform2d.rotation = 0.25f * glm::two_pi<float>();

        gameObjects.push_back(std::move(triangle));
    }
}