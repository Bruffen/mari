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
        uint32_t computeFamily;
        bool graphicsFamilyHasValue = false;
        bool presentFamilyHasValue = false;
        bool computeFamilyHasValue = false;
        bool isComplete() { return graphicsFamilyHasValue && presentFamilyHasValue && computeFamilyHasValue; }
    };

    class Device {
        public:
#ifdef MARI_DEBUG
        const bool enableValidationLayers = true;
        const bool enableShaderRelaxed = true;
#else
        const bool enableValidationLayers = false;
        const bool enableShaderRelaxed = false;
#endif

        Device(Window &window);
        ~Device();

        // Not copiable or movable
        Device(const Device &) = delete;
        Device &operator=(const Device &) = delete;
        Device(Device &&) = delete;
        Device &operator=(Device &&) = delete;

        VkDevice                    handle()                      const { return device_; }
        VkSurfaceKHR                surface()                     const { return surface_; }
        VkQueue                     graphicsQueue()               const { return graphicsQueue_; }
        VkQueue                     presentQueue()                const { return presentQueue_; }
        VkQueue                     computeQueue()                const { return computeQueue_; }
        VkInstance                  getInstance()                 const { return instance; }
        VkCommandPool               getCommandPool()              const { return commandPool; }
        VkPhysicalDevice            getPhysicalDevice()           const { return physicalDevice; }
        SwapchainSupportDetails     getSwapchainSupport()         const { return querySwapchainSupport(physicalDevice); }

        QueueFamilyIndices          findPhysicalQueueFamilies()   { return findQueueFamilies(physicalDevice); } // TODO do this once and save it as a member variable
        uint32_t                    findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;
        VkFormat                    findSupportedFormat(const std::vector<VkFormat> &candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

        VkCommandBuffer             beginSingleTimeCommands() const;
        void                        endSingleTimeCommands(VkCommandBuffer commandBuffer) const;

        void                        createBuffer(
                                        VkDeviceSize size, 
                                        VkBufferUsageFlags usage, 
                                        VkMemoryPropertyFlags properties, 
                                        VkBuffer &buffer, 
                                        VkDeviceMemory &bufferMemory) const;

        void                        createImageWithInfo(
                                        const VkImageCreateInfo &imageInfo,
                                        VkMemoryPropertyFlags properties,
                                        VkImage &image,
                                        VkDeviceMemory &imageMemory);
        void                        copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) const;
        void                        copyBufferToImage(VkBuffer buffer, VkImage image, VkExtent3D extent, uint32_t layerCount, VkImageLayout oldLayout);
        void                        copyImageToBuffer(VkImage image, VkExtent3D extent, VkImageLayout layout, VkBuffer buffer);
        void                        copyImageToImage(VkCommandBuffer commandBuffer, VkImage imageSrc, VkImage imageDst, VkExtent3D imageSize); // TODO transition layouts
        void                        blitImageToImage(VkCommandBuffer commandBuffer, VkImage imageSrc, VkExtent3D sizeSrc, VkImage imageDst, VkExtent3D sizeDst);

        VkPhysicalDeviceProperties properties;
        VkPhysicalDeviceRayTracingPipelinePropertiesKHR propertiesRT{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR};

    private:
        void                        createInstance();
        void                        setupDebugMessenger();
        void                        createSurface();
        void                        pickPhysicalDevice();
        void                        createLogicalDevice();
        void                        createCommandPool();

        bool                        isDeviceSuitable(VkPhysicalDevice device);
        std::vector<const char *>   getRequiredExtensions();
        bool                        checkValidationLayerSupport();
        QueueFamilyIndices          findQueueFamilies(VkPhysicalDevice device);
        void                        populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT &createInfo);
        void                        hasGflwRequiredInstanceExtensions();
        bool                        checkDeviceExtensionSupport(VkPhysicalDevice device);
        SwapchainSupportDetails     querySwapchainSupport(VkPhysicalDevice device) const;

        void                        addShaderDebugPrintf();
        void                        addRayTracingExtensions();

        VkInstance                  instance;
        VkDebugUtilsMessengerEXT    debugMessenger;
        VkPhysicalDevice            physicalDevice = VK_NULL_HANDLE;
        Window                      &window;
        VkCommandPool               commandPool;

        VkDevice                    device_;
        VkSurfaceKHR                surface_;
        VkQueue                     graphicsQueue_;
        VkQueue                     presentQueue_;
        VkQueue                     computeQueue_;

        const std::vector<const char *> validationLayers = {"VK_LAYER_KHRONOS_validation"/*, "VK_LAYER_LUNARG_api_dump"*/};
        std::vector<VkValidationFeatureEnableEXT> validationFeaturesEnable{};

        std::vector<const char *> deviceExtensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            VK_KHR_SHADER_RELAXED_EXTENDED_INSTRUCTION_EXTENSION_NAME
        };
    };
}