#pragma once

#include "device.hpp"
#include "pipeline.hpp"
#include "game_object.hpp"
#include "frame_info.hpp"
#include "image.hpp"

namespace mari {

    struct AccelerationStructure {
        VkAccelerationStructureKHR          handle;
        uint64_t                            deviceAddress;
        std::unique_ptr<Buffer>             buffer;

        VkWriteDescriptorSetAccelerationStructureKHR descriptor() {
            VkWriteDescriptorSetAccelerationStructureKHR accelerationStructureSet{};
            accelerationStructureSet.sType                      = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
            accelerationStructureSet.accelerationStructureCount = 1;
            accelerationStructureSet.pAccelerationStructures    = &handle;
            return accelerationStructureSet;
        }
    };

    class RayTracingSystem {
        public:
            RayTracingSystem(Device &device, Window &window, GameObject::Map &gameObjects, VkDescriptorSetLayout descriptorSetLayout);
            ~RayTracingSystem();

            void                                    render(FrameInfo &frameInfo, Swapchain &swapchain);
            // TODO get around having tlas and accumImage be public
            AccelerationStructure                   tlas;
            std::unique_ptr<Image>                  accumImage;
        private:
            void                                    buildScene(const GameObject::Map &scene);
            void                                    buildBLAS(const Mesh &mesh, const TransformComponent &transform);
            void                                    buildTLAS();
            void                                    createPipelineLayout(VkDescriptorSetLayout descriptorSetLayout);
            void                                    createPipeline();
            void                                    createImages(uint32_t width, uint32_t height);

            Device                                  &device;
            std::vector<AccelerationStructure>      blases;
            std::unique_ptr<Pipeline>               pipeline; 
            VkPipelineLayout                        pipelineLayout;

            std::unique_ptr<Image>                  presentImage;
    };
}