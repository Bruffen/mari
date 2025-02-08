#pragma once

#include "components/camera.hpp"
#include "game_object.hpp"

#include <vulkan/vulkan.h>

namespace mari {

    #define MAX_LIGHTS 10

    struct PointLight {
        glm::vec4 position{};
        glm::vec4 color{};
    };

    struct RasterizationUbo {
        glm::mat4 projection{1.0f};
        glm::mat4 view{1.0f};
        glm::mat4 inverseView{1.0f};
        //glm::vec3 lightDirection = glm::normalize(glm::vec3(1.0f, -3.0f, -1.0f));
        glm::vec4 ambientLightColor{1.0f, 1.0f, 1.0f, 0.1f};
        PointLight pointLights[MAX_LIGHTS];
        int numLights;
    };

    struct RayTracingUbo {
        glm::mat4 viewInverse;
        glm::mat4 projInverse;
        int frameCount;
    };

    struct FrameInfo {
        int frameIndex;
        int frameCounter;
        float deltaTime;
        float elapsedTime;
        VkCommandBuffer commandBuffer;
        Camera &camera;
        VkDescriptorSet globalDescriptorSet;
        GameObject::Map &gameObjects; // TODO should point to scene now and go through game objects that way
    };
}