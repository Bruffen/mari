#pragma once

#include "window.hpp"

#include <string>
#include <vector>

namespace mari {
    struct SwapchainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    struct QueueFamilyIndices {
        uint32_t graphicsFamily;
        uint32_t presentFamily;
        bool graphicsFamilyHasValue = false;
        bool presentFamilyHasValue = false;
        bool isComplete() { return graphicsFamilyHasValue && presentFamilyHasValue; }
    };

    class Device {
        public:
        #ifdef NDEBUG
        const bool enableValidationLayers = false;
        #else
        const bool enableValidationLayers = true;
        #endif

        Device(Window &window);
        ~Device();

        // Not copiable or movable
        Device(const Device &) = delete;
        Device &operator=(const Device &) = delete;
        Device(Device &&) = delete;
        Device &operator=(Device &&) = delete;

        VkDevice                    handle()                      { return device_; }
        VkSurfaceKHR                surface()                     { return surface_; }
        VkQueue                     graphicsQueue()               { return graphicsQueue_; }
        VkQueue                     presentQueue()                { return presentQueue_; }
        VkCommandPool               getCommandPool()              { return commandPool; }
        VkPhysicalDevice            getPhysicalDevice()           { return physicalDevice; }
        SwapchainSupportDetails     getSwapchainSupport()         { return querySwapchainSupport(physicalDevice); }

        QueueFamilyIndices          findPhysicalQueueFamilies()   { return findQueueFamilies(physicalDevice); }
        uint32_t                    findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
        VkFormat                    findSupportedFormat(const std::vector<VkFormat> &candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

        void createBuffer( /* TODO should this be in device? */
            VkDeviceSize size,
            VkBufferUsageFlags usage,
            VkMemoryPropertyFlags properties,
            VkBuffer &buffer,
            VkDeviceMemory &bufferMemory);

        VkCommandBuffer beginSingleTimeCommands();
        void endSingleTimeCommands(VkCommandBuffer commandBuffer);

        void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
        void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, uint32_t layerCount);

        void createImageWithInfo(
            const VkImageCreateInfo &imageInfo,
            VkMemoryPropertyFlags properties,
            VkImage &image,
            VkDeviceMemory &imageMemory);

        VkPhysicalDeviceProperties properties;
        VkPhysicalDeviceRayTracingPipelinePropertiesKHR propertiesRT{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR};


    private:
        void createInstance();
        void setupDebugMessenger();
        void createSurface();
        void pickPhysicalDevice();
        void createLogicalDevice();
        void createCommandPool();

        bool isDeviceSuitable(VkPhysicalDevice device);
        std::vector<const char *> getRequiredExtensions();
        bool checkValidationLayerSupport();
        QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
        void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT &createInfo);
        void hasGflwRequiredInstanceExtensions();
        bool checkDeviceExtensionSupport(VkPhysicalDevice device);
        SwapchainSupportDetails querySwapchainSupport(VkPhysicalDevice device);

        void addRayTracingExtensions();

        VkInstance                  instance;
        VkDebugUtilsMessengerEXT    debugMessenger;
        VkPhysicalDevice            physicalDevice = VK_NULL_HANDLE;
        Window                      &window;
        VkCommandPool               commandPool;

        VkDevice                    device_;
        VkSurfaceKHR                surface_;
        VkQueue                     graphicsQueue_;
        VkQueue                     presentQueue_;

        const std::vector<const char *> validationLayers = {"VK_LAYER_KHRONOS_validation"};
        std::vector<const char *> deviceExtensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME
        };
    };
}