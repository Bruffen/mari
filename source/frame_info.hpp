#pragma once

#include "components/camera.hpp"
#include "scene/node.hpp"

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
        int maxDepth;
        float exposure;
        int tonemapper;
        int frameAccumulation;
        int russianRoulette;
        int nextEventEstimation;
        int samplesPerPixel;
    };

    struct InfiniteLightUbo {
        int environmentID;
        glm::vec2 environmentRotation;
        float marginalIntegral;
        glm::uvec2 functionSize;
        uint64_t marginalFunctionBufferAddress;
        uint64_t marginalCdfBufferAddress;
        uint64_t conditionalIntegralBufferAddress;
        uint64_t conditionalFunctionBufferAddress;
        uint64_t conditionalCdfBufferAddress;
    }; // TODO is it possible to fit all of this data contiguously within a single object and then pass a buffer device address of that single object with all we need?
       // TODO is it possible to pass a device adress to the equal area image here instead of using descriptors? probably also need the device address to the sampler?

    struct LightUbo {
        uint64_t lightsBufferAddress;
        uint64_t functionBufferAddress;
        uint64_t cdfBufferAddress;
        float    integral;
        int      size;
    };

    struct VolumeUbo {
        uint64_t volumeBufferBDA;
        float g;
        float sigma_a;
        float sigma_s;
    };

    struct FrameInfo {
        int frameIndex;
        int frameCounter;
        float deltaTime;
        float elapsedTime;
        VkCommandBuffer commandBuffer;
        Node &cameraObject;
        VkDescriptorSet globalDescriptorSet;
        std::unordered_map<std::string, std::shared_ptr<Node>> &nodes;
    };
}