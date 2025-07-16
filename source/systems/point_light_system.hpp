#pragma once

#include "device.hpp"
#include "pipeline.hpp"
#include "scene/node.hpp"
#include "frame_info.hpp"

#include <memory>
#include <vector>

namespace mari {
    class PointLightSystem {
        public:
            PointLightSystem(Device &device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout);
            ~PointLightSystem();

            void update(FrameInfo &frameInfo, RasterizationUbo &ubo);
            void render(FrameInfo &frameInfo);
        private:
            void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
            void createPipeline(VkRenderPass renderPass);

            Device &device;
            std::unique_ptr<Pipeline> pipeline; 
            VkPipelineLayout pipelineLayout;
    };
}