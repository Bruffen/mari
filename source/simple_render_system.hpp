#pragma once

#include "camera.hpp"
#include "device.hpp"
#include "pipeline.hpp"
#include "game_object.hpp"

#include <memory>
#include <vector>

namespace mari {
    class SimpleRenderSystem {
        public:
            SimpleRenderSystem(Device &device, VkRenderPass renderPass);
            ~SimpleRenderSystem();

            void renderGameObjects(VkCommandBuffer commandBuffer, std::vector<GameObject> &gameObjects, const Camera &camera) ;
        private:
            void createPipelineLayout();
            void createPipeline(VkRenderPass renderPass);

            Device &device;
            std::unique_ptr<Pipeline> pipeline; 
            VkPipelineLayout pipelineLayout;
    };
}