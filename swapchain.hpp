#pragma once

#include "device.hpp"

#include <vulkan/vulkan.h>

#include <memory>
#include <string>
#include <vector>

namespace mari
{

    class Swapchain
    {
        public:
            static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

            Swapchain(Device &deviceRef, VkExtent2D windowExtent);
            Swapchain(Device &deviceRef, VkExtent2D windowExtent, std::shared_ptr<Swapchain> previous);

            ~Swapchain();

            Swapchain(const Swapchain &) = delete;
            Swapchain &operator=(const Swapchain &) = delete;

            VkFramebuffer getFrameBuffer(int index) { return SwapchainFramebuffers[index]; }
            VkRenderPass getRenderPass() { return renderPass; }
            VkImageView getImageView(int index) { return SwapchainImageViews[index]; }
            size_t imageCount() { return SwapchainImages.size(); }
            VkFormat getSwapchainImageFormat() { return SwapchainImageFormat; }
            VkExtent2D getSwapchainExtent() { return SwapchainExtent; }
            uint32_t width() { return SwapchainExtent.width; }
            uint32_t height() { return SwapchainExtent.height; }

            float extentAspectRatio()
            {
                return static_cast<float>(SwapchainExtent.width) / static_cast<float>(SwapchainExtent.height);
            }
            VkFormat findDepthFormat();

            VkResult acquireNextImage(uint32_t *imageIndex);
            VkResult submitCommandBuffers(const VkCommandBuffer *buffers, uint32_t *imageIndex);

        private:
            void initialize();
            void createSwapchain();
            void createImageViews();
            void createDepthResources();
            void createRenderPass();
            void createFramebuffers();
            void createSyncObjects();

            // Helper functions
            VkSurfaceFormatKHR chooseSwapSurfaceFormat(
                const std::vector<VkSurfaceFormatKHR> &availableFormats);
            VkPresentModeKHR chooseSwapPresentMode(
                const std::vector<VkPresentModeKHR> &availablePresentModes);
            VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities);

            VkFormat SwapchainImageFormat;
            VkExtent2D SwapchainExtent;

            std::vector<VkFramebuffer> SwapchainFramebuffers;
            VkRenderPass renderPass;

            std::vector<VkImage> depthImages;
            std::vector<VkDeviceMemory> depthImageMemorys;
            std::vector<VkImageView> depthImageViews;
            std::vector<VkImage> SwapchainImages;
            std::vector<VkImageView> SwapchainImageViews;

            Device &device;
            VkExtent2D windowExtent;

            VkSwapchainKHR swapchain;
            std::shared_ptr<Swapchain> oldSwapchain;

            std::vector<VkSemaphore> imageAvailableSemaphores;
            std::vector<VkSemaphore> renderFinishedSemaphores;
            std::vector<VkFence> inFlightFences;
            std::vector<VkFence> imagesInFlight;
            size_t currentFrame = 0;
    };
}