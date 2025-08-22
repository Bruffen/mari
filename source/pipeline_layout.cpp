#pragma once

#include "pipeline_layout.hpp"

#include <stdexcept>

namespace mari {
    PipelineLayout::PipelineLayout(Device& device, const std::vector<VkDescriptorSetLayout>* descriptorSetLayouts) 
    : PipelineLayout(device, descriptorSetLayouts, nullptr) { }

    PipelineLayout::PipelineLayout(Device& device, const std::vector<VkDescriptorSetLayout>* descriptorSetLayouts, const std::vector<VkPushConstantRange>* pushConstantRanges) : device{device} {
        VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
        pipelineLayoutCreateInfo.sType          = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        // Descriptor Set
        pipelineLayoutCreateInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts->size());
        pipelineLayoutCreateInfo.pSetLayouts    = descriptorSetLayouts->data();
        // Push Constant
        if (pushConstantRanges) {
            pipelineLayoutCreateInfo.pushConstantRangeCount = static_cast<uint32_t>(pushConstantRanges->size());
            pipelineLayoutCreateInfo.pPushConstantRanges = pushConstantRanges->data();
        }

        if (vkCreatePipelineLayout(device.handle(), &pipelineLayoutCreateInfo, nullptr, &layout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create pipeline layout");
        }
    }

    PipelineLayout::~PipelineLayout() {
        vkDestroyPipelineLayout(device.handle(), layout, nullptr);
    }
}