#pragma once

#include "camera.hpp"
#include "device.hpp"
#include "pipeline.hpp"
#include "game_object.hpp"
#include "frame_info.hpp"

#include <memory>
#include <vector>

namespace mari {
    class SimpleRenderSystem {
        public:
            SimpleRenderSystem(Device &device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout);
            ~SimpleRenderSystem();

            void renderGameObjects(FrameInfo &frameInfo, std::vector<GameObject> &gameObjects) ;
        private:
            void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
            void createPipeline(VkRenderPass renderPass);

            Device &device;
            std::unique_ptr<Pipeline> pipeline; 
            VkPipelineLayout pipelineLayout;
    };
}