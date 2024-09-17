#pragma once

#include "window.hpp"
#include "device.hpp"
#include "pipeline.hpp"
#include "swapchain.hpp"
#include "model.hpp"

#include <memory>
#include <vector>

namespace mari {
    class Renderer {
        public:
            static constexpr int WIDTH = 800;
            static constexpr int HEIGHT = 600;

            Renderer();
            ~Renderer();
            Renderer(const Renderer &) = delete;
            Renderer &operator=(const Renderer &) = delete;

            void run();
            void loadModels();
        private:
            void createPipelineLayout();
            void createPipeline();
            void createCommandBuffers();
            void freeCommandBuffers();
            void drawFrame();
            void recreateSwapchain();
            void recordCommandBuffer(int imageIndex);

            Window window{WIDTH, HEIGHT, "Mari"};
            Device device{window};
            std::unique_ptr<Swapchain> swapchain;
            std::unique_ptr<Pipeline> pipeline;
            VkPipelineLayout pipelineLayout;
            std::vector<VkCommandBuffer> commandBuffers;
            std::unique_ptr<Model> model;
    };
}