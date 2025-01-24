#pragma once

#include "image.hpp"
#include "default_objects.hpp"

#include <memory>
#include <stdexcept>
#include <iostream>
#include <vulkan/utility/vk_format_utils.h>

namespace mari {
    Image::Image(Device &device, VkExtent3D size, VkFormat format, VkImageUsageFlags flags, VkImageLayout layout, void *data)
     : device{device}, size{size}, format{format}, flags{flags}, layout{layout}, sampler{sampler} {
        auto formatInfo = vkuGetFormatInfo(format);
        channels = formatInfo.component_count;
        texelBytes = formatInfo.block_size;
        createImage();
        createImageView();

        if (data) {
            writeFromData(data);
        }
    }
    
    Image::Image(Device &device, VkExtent3D size, VkFormat format, VkImageUsageFlags flags, VkImageLayout layout)
     : Image(device, size, format, flags, layout, nullptr) {
    }
    
    Image::~Image() {
        cleanup();
    }

    void Image::createImage() {
        VkImageCreateInfo imageCreateInfo{};
        imageCreateInfo.sType           = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageCreateInfo.imageType       = VK_IMAGE_TYPE_2D;
        imageCreateInfo.format          = format;
        imageCreateInfo.extent          = size;
        imageCreateInfo.mipLevels       = 1;
        imageCreateInfo.arrayLayers     = 1;
        imageCreateInfo.samples         = VK_SAMPLE_COUNT_1_BIT;
        imageCreateInfo.tiling          = VK_IMAGE_TILING_OPTIMAL;
        imageCreateInfo.usage           = flags;
        imageCreateInfo.initialLayout   = VK_IMAGE_LAYOUT_UNDEFINED;
        device.createImageWithInfo(imageCreateInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, handle, memory);

        // TODO study barriers and try to incorporate it into single time command method
        VkCommandBuffer cmdBuffer = device.beginSingleTimeCommands();
        VkImageMemoryBarrier imgBarrier{};
        imgBarrier.sType                = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        imgBarrier.srcAccessMask        = {};
        imgBarrier.dstAccessMask        = {};
        imgBarrier.oldLayout            = VK_IMAGE_LAYOUT_UNDEFINED;
        imgBarrier.newLayout            = layout;
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
        imageViewCreateInfo.format                          = format;
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
        descriptor.imageView   = view;
        descriptor.imageLayout = layout;
        descriptor.sampler     = sampler ? sampler : DefaultObjects::getSamplerNearest(); // TODO not every image needs a sampler
        return descriptor;
    }

    void Image::resize(uint32_t width, uint32_t height) {
        cleanup();

        this->size = VkExtent3D{width, height, 1};

        createImage();
        createImageView();
    }

    void Image::writeFromData(void *data) {
        size_t data_size = size.width * size.height * size.depth * texelBytes;
        Buffer stagingBuffer = Buffer(
            device, 
            data_size, 
            1, 
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );

        stagingBuffer.map();
        stagingBuffer.writeToBuffer(data);

        writeFromBuffer(stagingBuffer);
    }

    void Image::writeFromBuffer(Buffer &buffer) {
        device.copyBufferToImage(buffer.handle(), handle, size, 1, layout);
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