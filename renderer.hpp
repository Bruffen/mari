#pragma once

#include "window.hpp"
#include "device.hpp"
#include "swapchain.hpp"

#include <memory>
#include <vector>
#include <cassert>

namespace mari {
    class Renderer {
        public:
            Renderer(Window& window, Device& device);
            ~Renderer();
            Renderer(const Renderer &) = delete;
            Renderer &operator=(const Renderer &) = delete;

            VkRenderPass getSwapchainRenderPass() const { return swapchain->getRenderPass(); }
            float getAspectRatio() const { return swapchain->extentAspectRatio(); }
            bool isFrameInProgress() const { return isFrameStarted; }

            VkCommandBuffer getCurrentCommandBuffer() const {
                assert(isFrameStarted && "Cannot get command buffer when frame is not in progress"); 
                return commandBuffers[currentFrameIndex]; 
            }

            int getFrameIndex() const {
                assert(isFrameStarted && "Cannot get frame index when frame is not in progress");
                return currentFrameIndex;
            }

            VkCommandBuffer beginFrame();
            void endFrame();
            void beginSwapchainRenderPass(VkCommandBuffer commandBuffer);
            void endSwapchainRenderPass(VkCommandBuffer commandBuffer);

        private:
            void createCommandBuffers();
            void freeCommandBuffers();
            void recreateSwapchain();

            Window& window;
            Device& device;
            std::unique_ptr<Swapchain> swapchain;
            std::vector<VkCommandBuffer> commandBuffers;

            uint32_t currentImageIndex;
            int currentFrameIndex{0};
            bool isFrameStarted{false};
    };
}