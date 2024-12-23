#pragma once

#include "image.hpp"

#include <stdexcept>

namespace mari {
    Image::Image(Device &device, uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags flags)
     : device{device}, width{width}, height{height}, format{format} {
        createImage();
        createImageView();
    }

    Image::~Image() {
        cleanup();
    }

    void Image::createImage() {
        VkImageCreateInfo imageCreateInfo{};
        imageCreateInfo.sType           = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageCreateInfo.imageType       = VK_IMAGE_TYPE_2D;
        imageCreateInfo.format          = VK_FORMAT_B8G8R8A8_UNORM;
        imageCreateInfo.extent.width    = width;
        imageCreateInfo.extent.height   = height;
        imageCreateInfo.extent.depth    = 1;
        imageCreateInfo.mipLevels       = 1;
        imageCreateInfo.arrayLayers     = 1;
        imageCreateInfo.samples         = VK_SAMPLE_COUNT_1_BIT;
        imageCreateInfo.tiling          = VK_IMAGE_TILING_OPTIMAL;
        imageCreateInfo.usage           = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
        imageCreateInfo.initialLayout   = VK_IMAGE_LAYOUT_UNDEFINED;
        device.createImageWithInfo(imageCreateInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, handle, memory);

        // TODO study barriers and try to incorporate it into single time command method
        VkCommandBuffer cmdBuffer = device.beginSingleTimeCommands();
        VkImageMemoryBarrier imgBarrier{};
        imgBarrier.sType                = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        imgBarrier.srcAccessMask        = {};
        imgBarrier.dstAccessMask        = {};
        imgBarrier.oldLayout            = VK_IMAGE_LAYOUT_UNDEFINED;
        imgBarrier.newLayout            = VK_IMAGE_LAYOUT_GENERAL;
        imgBarrier.srcQueueFamilyIndex  = VK_QUEUE_FAMILY_IGNORED;
        imgBarrier.dstQueueFamilyIndex  = VK_QUEUE_FAMILY_IGNORED;
        imgBarrier.image                = handle;
        imgBarrier.subresourceRange     = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

        vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &imgBarrier);
        device.endSingleTimeCommands(cmdBuffer);
    }

    void Image::createImageView() {
        VkImageViewCreateInfo imageViewCreateInfo{};
        imageViewCreateInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewCreateInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        imageViewCreateInfo.format                          = VK_FORMAT_B8G8R8A8_UNORM;
        imageViewCreateInfo.subresourceRange                = {};
        imageViewCreateInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        imageViewCreateInfo.subresourceRange.baseMipLevel   = 0;
        imageViewCreateInfo.subresourceRange.levelCount     = 1;
        imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
        imageViewCreateInfo.subresourceRange.layerCount     = 1;
        imageViewCreateInfo.image                           = handle;
        if (vkCreateImageView(device.handle(), &imageViewCreateInfo, nullptr, &view)) {
            throw std::runtime_error("Failed to create image view");
        }
    }

    VkDescriptorImageInfo Image::descriptorInfo() {
        VkDescriptorImageInfo imageDescriptor{};
        imageDescriptor.imageView                   = view;
        imageDescriptor.imageLayout                 = VK_IMAGE_LAYOUT_GENERAL;
        return imageDescriptor;
    }

    void Image::resize(uint32_t width, uint32_t height) {
        cleanup();

        this->width  = width;
        this->height = height;

        createImage();
        createImageView();
    }

    void Image::cleanup() {
        if (view != VK_NULL_HANDLE) {
            vkDestroyImageView(device.handle(), view, nullptr);
        }
        if (memory != VK_NULL_HANDLE) {
            vkFreeMemory(device.handle(), memory, nullptr);
        }
        if (handle != VK_NULL_HANDLE) {
            vkDestroyImage(device.handle(), handle, nullptr);
        }
    }
}