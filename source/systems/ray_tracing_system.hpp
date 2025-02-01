#pragma once

#include "acceleration_structure.hpp"
#include "pipeline.hpp"
#include "scene/scene.hpp"
#include "frame_info.hpp"
#include "image.hpp"

namespace mari {
    class RayTracingSystem {
        public:
            RayTracingSystem(Device &device, Window &window, const Scene &scene, VkDescriptorSetLayout descriptorSetLayout);
            ~RayTracingSystem();

            void                                    render(FrameInfo &frameInfo, Swapchain &swapchain);
            // TODO get around having these be public
            std::unique_ptr<AccelerationStructure>  tlas;
            std::unique_ptr<Image>                  accumImage;
            std::unique_ptr<Buffer>                 geometryAddressesBuffer;
            std::unique_ptr<Buffer>                 blasDataBuffer;
        private:
            void                                    buildScene(const Scene &scene);
            void                                    buildBLAS(const Mesh &mesh, VkTransformMatrixKHR transformMatrix);
            void                                    buildTLAS(VkTransformMatrixKHR transformMatrix);
            void                                    createPipelineLayout(VkDescriptorSetLayout descriptorSetLayout);
            void                                    createPipeline();
            void                                    createImages(uint32_t width, uint32_t height);

            Device                                  &device;
            std::vector<std::unique_ptr<AccelerationStructure>> blases;
            std::unique_ptr<Pipeline>               pipeline; 
            VkPipelineLayout                        pipelineLayout;

            std::unique_ptr<Image>                  presentImage;

            std::vector<SubMeshAdresses>            geometryAddresses;
    };
}