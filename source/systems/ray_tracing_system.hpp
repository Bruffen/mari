#pragma once

#include "device.hpp"
#include "pipeline.hpp"
#include "game_object.hpp"
#include "frame_info.hpp"

namespace mari {
    
    // Holds data for a scratch buffer used as a temporary storage during acceleration structure builds
    struct ScratchBuffer {
        uint64_t       deviceAddress;
        VkBuffer       handle;
        VkDeviceMemory memory;
    };

    struct AccelerationStructure {
        VkAccelerationStructureKHR          handle;
        uint64_t                            deviceAddress;
        std::unique_ptr<Buffer>             buffer;
    };



    class RayTracingSystem {
        public:
            RayTracingSystem(Device &device, GameObject::Map &gameObjects, VkDescriptorSetLayout descriptorSetLayout);
            ~RayTracingSystem();

            void                                    render(FrameInfo &frameInfo);
            // TODO get around having tlas be public
            AccelerationStructure                   tlas; 
            // TODO make this private after improving
            void                                    buildCommandBuffers(Renderer &renderer, std::vector<VkDescriptorSet> &descriptorSets, VkImage &image, uint32_t width, uint32_t height);
        private:
            void                                    buildScene(const GameObject::Map &scene);
            void                                    buildBLAS(const GameObject &object);
            void                                    buildTLAS();
            void                                    createPipelineLayout(VkDescriptorSetLayout descriptorSetLayout);
            void                                    createPipeline();

            ScratchBuffer                           createScratchBuffer(VkDeviceSize size);
            void                                    deleteScratchBuffer(ScratchBuffer &scratchBuffer);

            Device                                  &device;
            std::vector<AccelerationStructure>      blases;
            std::unique_ptr<Pipeline>               pipeline; 
            VkPipelineLayout                        pipelineLayout;

            
    };
}