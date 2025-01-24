#pragma once

#include "device.hpp"
#include "pipeline.hpp"
#include "scene/scene.hpp"
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
            RayTracingSystem(Device &device, Window &window, const Scene &scene, VkDescriptorSetLayout descriptorSetLayout);
            ~RayTracingSystem();

            void                                    render(FrameInfo &frameInfo, Swapchain &swapchain);
            // TODO get around having these be public
            AccelerationStructure                   tlas;
            std::unique_ptr<Image>                  accumImage;
            std::unique_ptr<Buffer>                 geometryAddressesBuffer;
        private:
            void                                    buildScene(const Scene &scene);
            void                                    buildBLAS(const Mesh &mesh, VkTransformMatrixKHR transformMatrix);
            void                                    buildTLAS(VkTransformMatrixKHR transformMatrix);
            void                                    createPipelineLayout(VkDescriptorSetLayout descriptorSetLayout);
            void                                    createPipeline();
            void                                    createImages(uint32_t width, uint32_t height);

            Device                                  &device;
            std::vector<AccelerationStructure>      blases;
            std::unique_ptr<Pipeline>               pipeline; 
            VkPipelineLayout                        pipelineLayout;

            std::unique_ptr<Image>                  presentImage;

            std::vector<GeometryAdresses>           geometryAddresses;
    };
}