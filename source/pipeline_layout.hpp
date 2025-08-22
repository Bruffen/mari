#pragma once

#include "device.hpp"

namespace mari {
    class PipelineLayout {
        public:
            PipelineLayout(Device& device, const std::vector<VkDescriptorSetLayout>* descriptorSetLayouts);
            PipelineLayout(Device& device, const std::vector<VkDescriptorSetLayout>* descriptorSetLayouts, const std::vector<VkPushConstantRange>* pushConstantRanges);

            ~PipelineLayout();

            const VkPipelineLayout& handle() const { return layout; };
        private:
            Device &device;
            VkPipelineLayout layout{};
    };
}