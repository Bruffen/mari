#pragma once

#include "device.hpp"

#include <vulkan\vulkan.h>

namespace mari {

    class Image {
        public:
            Image(Device &device, uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags flags);
            ~Image();

            VkDescriptorImageInfo   descriptorInfo();
            void                    resize(uint32_t width, uint32_t height);

            VkDeviceMemory          memory;
            VkImage                 handle = VK_NULL_HANDLE;
            VkImageView             view;
            VkFormat                format;
            uint32_t                width;
            uint32_t                height;

        private:
            void                    createImage();
            void                    createImageView();
            void                    cleanup();

            Device                  &device;
    };
}