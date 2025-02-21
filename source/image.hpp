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

            void                    resize(uint32_t width, uint32_t height);
            void                    writeFromData(void *data);
            void                    writeFromBuffer(Buffer &buffer);
            VkDescriptorImageInfo   descriptorInfo();

            std::string             name = "";
            VkExtent3D              size;
            VkDeviceMemory          memory;
            VkImage                 handle = VK_NULL_HANDLE;
            VkImageView             view;
            VkImageLayout           layout;
            VkFormat                format;
            VkImageUsageFlags       flags;
            VkDescriptorSet         descriptorGui; // TODO can the same one be used for both? Probably not, use a map instead in gui class
            VkSampler               sampler; // TODO not every image needs a sampler, maybe make a texture class that holds both an image and sampler

        private:
            void                    createImage();
            void                    createImageView();
            void                    cleanup();

            Device                  &device;
            uint32_t                channels;
            uint32_t                texelBytes;
            VkDescriptorImageInfo   descriptor;
    };
}