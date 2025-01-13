#pragma once

#include "device.hpp"
#include "buffer.hpp"

#include <vulkan\vulkan.h>

namespace mari {

    class Image {
        public:
            Image(Device &device, VkExtent3D size, VkFormat format, VkImageUsageFlags flags, VkImageLayout layout, void *data);
            Image(Device &device, VkExtent3D size, VkFormat format, VkImageUsageFlags flags, VkImageLayout layout);
            ~Image();

            VkDescriptorImageInfo   descriptorInfo();
            void                    resize(uint32_t width, uint32_t height);
            void                    writeFromData(void *data);
            void                    writeFromBuffer(Buffer &buffer);

            VkExtent3D              size;
            VkDeviceMemory          memory;
            VkImage                 handle = VK_NULL_HANDLE;
            VkImageView             view;
            VkImageLayout           layout;
            VkFormat                format;
            VkImageUsageFlags       flags;

        private:
            void                    createImage();
            void                    createImageView();
            void                    cleanup();

            Device                  &device;
    };
}