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
            static constexpr int MAX_FRAMES_IN_FLIGHT = 3; // TODO rt

            Swapchain(Device &deviceRef, VkExtent2D windowExtent);
            Swapchain(Device &deviceRef, VkExtent2D windowExtent, std::shared_ptr<Swapchain> previous);

            ~Swapchain();

            Swapchain(const Swapchain &) = delete;
            Swapchain &operator=(const Swapchain &) = delete;

            VkSwapchainKHR              handle()                        const { return swapchain; }
            VkFramebuffer               getFrameBuffer(int index)       const { return swapchainFramebuffers[index]; }
            VkRenderPass                getRenderPass()                 const { return renderPass; }
            VkImageView                 getImageView(int index)         const { return swapchainImageViews[index]; }
            std::vector<VkImage>&       getImages()                           { return swapchainImages; }
            size_t                      imageCount()                    const { return swapchainImages.size(); }
            VkFormat                    getSwapchainImageFormat()       const { return swapchainImageFormat; }
            VkExtent2D                  getSwapchainExtent()            const { return swapchainExtent; }
            uint32_t                    width()                         const { return swapchainExtent.width; }
            uint32_t                    height()                        const { return swapchainExtent.height; }

            float extentAspectRatio() {
                return static_cast<float>(swapchainExtent.width) / static_cast<float>(swapchainExtent.height);
            }
            VkFormat findDepthFormat();

            VkResult acquireNextImage(uint32_t *imageIndex);
            VkResult submitCommandBuffers(const VkCommandBuffer *buffers, uint32_t *imageIndex);

            bool compareSwapFormats(const Swapchain &swapchain) const {
                return swapchain.swapchainDepthFormat == swapchainDepthFormat &&
                       swapchain.swapchainImageFormat == swapchainImageFormat;
            }

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

            VkFormat swapchainImageFormat;
            VkFormat swapchainDepthFormat;
            VkExtent2D swapchainExtent;

            std::vector<VkFramebuffer> swapchainFramebuffers;
            VkRenderPass renderPass;

            std::vector<VkImage> depthImages;
            std::vector<VkDeviceMemory> depthImageMemorys;
            std::vector<VkImageView> depthImageViews;
            std::vector<VkImage> swapchainImages;
            std::vector<VkImageView> swapchainImageViews;

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