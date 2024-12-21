#pragma once

#include "window.hpp"
#include "device.hpp"
#include "game_object.hpp"
#include "renderer.hpp"
#include "descriptors.hpp"

#include <memory>
#include <vector>

namespace mari {
    class MariRT {
        public:
            static constexpr int WIDTH = 800;
            static constexpr int HEIGHT = 600;

            MariRT();
            MariRT(const MariRT &) = delete;
            MariRT &operator=(const MariRT &) = delete;

            void run();
        private:
            void loadGameObjects();

            Window window{WIDTH, HEIGHT, "MariRT"};
            Device device{window};
            Renderer renderer{window, device};

            std::unique_ptr<DescriptorPool> globalPool{};
            GameObject::Map gameObjects;

            struct UniformData {
                glm::mat4 viewInverse;
                glm::mat4 projInverse;
            } uniformData;

            VkPipeline                              pipeline;
            VkPipelineLayout                        pipelineLayout;
            VkDescriptorPool                        descriptorPool;
            std::vector<VkDescriptorSet>            descriptorSets;
            VkDescriptorSetLayout                   descriptorSetLayout;
            
            struct StorageImage { // TODO move this out of here
                VkDeviceMemory memory;
                VkImage        image = VK_NULL_HANDLE;
                VkImageView    view;
                VkFormat       format;
                uint32_t       width;
                uint32_t       height;
            } storageImage;

            std::vector<std::unique_ptr<Buffer>>    uboBuffers;

            void initializeRayTracing();
            void createStorageImage();
            void createUniformBuffers();
    };
}