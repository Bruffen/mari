#pragma once

#include "descriptors.hpp"
#include "light.hpp"
#include "pipeline.hpp"
#include "default_objects.hpp"

#include <span>
#include <stdexcept>

namespace mari {
    Light::Light(Device &device) : device{device} {
        
    }

    InfiniteAreaLight::InfiniteAreaLight(Device &device, std::shared_ptr<Image> image) : Light(device) {
        assert(image && "Need to pass a valid Image into InfiniteAreaLight");
        assert(image->format == VK_FORMAT_R32G32B32A32_SFLOAT && "Environment image for infinite area light needs to be rgba32f." );

        
        VkExtent3D extent{ textureSize, textureSize, 1 };
        equalAreaImage = std::make_shared<Image>(
            device, 
            extent, 
            image->format, 
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 
            VK_IMAGE_LAYOUT_GENERAL
        );
        equalAreaImage->name = image->name;
        equalAreaImage->sampler = DefaultObjects::getSamplerLinearClampEdge();

        equalAreaTransformation(image);

        auto imageBuffer = std::make_shared<Buffer>(
            device,
            sizeof(float) * 4 * textureSize * textureSize, 
            1, 
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
        );

        device.copyImageToBuffer(equalAreaImage->handle, extent, equalAreaImage->layout, imageBuffer->handle());

        vkDeviceWaitIdle(device.handle()); // TODO synchronization

        // Get the image data in host memory
        std::span<glm::vec4> data((glm::vec4*)imageBuffer->getMappedMemory(), textureSize * textureSize);

        // Convert rgb values to single floats
        std::vector<float> values{};
        values.reserve(data.size());
        for (const glm::vec4& d : data) {
            values.push_back(glm::length(glm::vec3(d.r, d.g, d.b)));
        }

        // Create PiecewiseConstant
        sampler = PiecewiseConstant2D(device, values, textureSize, textureSize);
    }

    void InfiniteAreaLight::equalAreaTransformation(std::shared_ptr<Image> image) {
        std::unique_ptr<DescriptorPool> pool = DescriptorPool::Builder(device)
            .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER      , 1)
            .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE               , 1)
            .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER              , 1)
            .build();

        DescriptorSetLayout descriptorSetLayout = DescriptorSetLayout::Builder(device)
            .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER    , VK_SHADER_STAGE_COMPUTE_BIT)
            .addBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE             , VK_SHADER_STAGE_COMPUTE_BIT)
            .addBinding(2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER            , VK_SHADER_STAGE_COMPUTE_BIT)
            .build();
            
        std::vector<VkDescriptorSetLayout> descriptorSetLayouts{descriptorSetLayout.handle()};
        PipelineLayout pipelineLayout{device, &descriptorSetLayouts};

        Pipeline pipeline{device};
        pipeline.createComputePipeline("../../shaders/spv/infinite_area_light.comp.spv", pipelineLayout);

        Buffer textureSizeUniform = Buffer{device, sizeof(glm::uvec2), 1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT};
        textureSizeUniform.map();
        glm::uvec2 textureSize2D{textureSize, textureSize};
        textureSizeUniform.writeToBuffer(&textureSize2D);

        VkDescriptorSet descriptorSet;
        DescriptorWriter(descriptorSetLayout, *pool)
            .writeImage( 0, image->descriptorInfo())
            .writeImage( 1, equalAreaImage->descriptorInfo())
            .writeBuffer(2, textureSizeUniform.descriptorInfo())
            .build(descriptorSet);

        VkCommandBuffer commandBuffer = device.beginSingleTimeCommands();
        pipeline.bind(commandBuffer);
        vkCmdBindDescriptorSets(commandBuffer, pipeline.bindPoint(), pipelineLayout.handle(), 0, 1, &descriptorSet, 0, nullptr);
        vkCmdDispatch(commandBuffer, equalAreaImage->size.width / 16, equalAreaImage->size.height / 16, 1);
        vkEndCommandBuffer(commandBuffer);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        if (vkQueueSubmit(device.computeQueue(), 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
            throw std::runtime_error("Failed to submit compute command buffer!");
        }
        
        vkDeviceWaitIdle(device.handle()); // TODO actual synchronization in VkSubmitInfo
    }
}