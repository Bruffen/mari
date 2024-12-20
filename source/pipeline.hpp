#pragma once

#include "device.hpp"

#include <string>
#include <vector>

namespace mari {
    struct PipelineConfigInfo {
        PipelineConfigInfo() = default;
        PipelineConfigInfo(const PipelineConfigInfo&) = delete;
        PipelineConfigInfo& operator=(const PipelineConfigInfo&) = delete;

        std::vector<VkVertexInputBindingDescription> bindingDescriptions{};
        std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};
        VkPipelineViewportStateCreateInfo viewportInfo;
        VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo;
        VkPipelineRasterizationStateCreateInfo rasterizationInfo;
        VkPipelineMultisampleStateCreateInfo multisampleInfo;
        VkPipelineColorBlendAttachmentState colorBlendAttachment;
        VkPipelineColorBlendStateCreateInfo colorBlendInfo;
        VkPipelineDepthStencilStateCreateInfo depthStencilInfo;
        std::vector<VkDynamicState> dynamicStateEnables;
        VkPipelineDynamicStateCreateInfo dynamicStateInfo;
        VkPipelineLayout pipelineLayout = nullptr;
        VkRenderPass renderPass = nullptr;
        uint32_t subpass = 0;
    };

    class Pipeline {
        public:
            Pipeline(
                Device &device, 
                const std::string &vertFilepath, 
                const std::string &fragFilepath, 
                const PipelineConfigInfo &configInfo
            );

            ~Pipeline();

            Pipeline(const Pipeline&) = delete;
            Pipeline &operator=(const Pipeline&) = delete;

            void bind(VkCommandBuffer commandBuffer);
            static void defaultPipelineConfigInfo(PipelineConfigInfo &configInfo);
            static void enableAlphaBlending(PipelineConfigInfo &configInfo);

            // TODO rt move to private and remove device from args
            //static void loadShader(const std::string &filepath, VkShaderStageFlagBits flag, VkPipelineShaderStageCreateInfo *stage);
            static std::vector<char> readFile(const std::string &filepath);
            static VkShaderModule createShaderModule(Device &device, const std::vector<char> &code);
        private:

            // TODO rt change name to rasterization pipeline
            void createGraphicsPipeline(
                const std::string &vertFilepath, 
                const std::string &fragFilepath, 
                const PipelineConfigInfo &configInfo);


            Device &device;
            VkPipeline graphicsPipeline;
            VkShaderModule vertShaderModule; // TODO rt
            VkShaderModule fragShaderModule; // TODO rt
    };
}