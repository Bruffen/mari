#pragma once

#include "window.hpp"
#include "device.hpp"
#include "game_object.hpp"
#include "renderer.hpp"
#include "descriptors.hpp"

#include <memory>
#include <vector>

namespace mari {
    // Holds data for a scratch buffer used as a temporary storage during acceleration structure builds
    struct ScratchBuffer
    {
        uint64_t       device_address;
        VkBuffer       handle;
        VkDeviceMemory memory;
    };

    class MariRT {
        public:
            static constexpr int WIDTH = 800;
            static constexpr int HEIGHT = 600;

            MariRT();
            ~MariRT();
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

            // TODO rt improve
            VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProperties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR};

            struct AccelerationStructure {
                VkAccelerationStructureKHR          handle;
                uint64_t                            device_address;
                std::unique_ptr<Buffer>             buffer;
            };

            struct StorageImage {
                VkDeviceMemory memory;
                VkImage        image = VK_NULL_HANDLE;
                VkImageView    view;
                VkFormat       format;
                uint32_t       width;
                uint32_t       height;
            } storageImage;

            struct UniformData {
                glm::mat4 viewInverse;
                glm::mat4 projInverse;
            } uniformData;

            AccelerationStructure                   bottomLevelAS;
            AccelerationStructure                   topLevelAS;

            VkPipeline                              pipeline;
            VkPipelineLayout                        pipelineLayout;
            VkDescriptorPool                        descriptorPool;
            std::vector<VkDescriptorSet>            descriptorSets;
            VkDescriptorSetLayout                   descriptorSetLayout;
            std::vector<VkShaderModule>             shaderModules;
            std::vector<VkRayTracingShaderGroupCreateInfoKHR> shaderGroups{};
            std::unique_ptr<Buffer>                 raygenSBT, missSBT, hitSBT;
            std::vector<std::unique_ptr<Buffer>>    uboBuffers;

            void initializeRayTracing();
            void createStorageImage();
            void createUniformBuffers();
            void buildBottomLevelAccelerationStructure();
            void buildTopLevelAccelerationStructure();
            void createRayTracingPipeline();
            void createShaderBindingTables();
            void createDescriptorSets();
            void buildCommandBuffers();

            ScratchBuffer createScratchBuffer(VkDeviceSize size);
            void deleteScratchBuffer(ScratchBuffer &scratch_buffer);

            VkPipelineShaderStageCreateInfo loadShader(const std::string &filepath, VkShaderStageFlagBits stage);
    };
}