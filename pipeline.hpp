#pragma once

#include "device.hpp"

#include <string>
#include <vector>

namespace mari {
    struct PipelineConfigInfo {
        PipelineConfigInfo(const PipelineConfigInfo&) = delete;
        PipelineConfigInfo() = default;
        PipelineConfigInfo& operator=(const PipelineConfigInfo&) = delete;

        VkViewport viewport;
        VkRect2D scissor;
        VkPipelineViewportStateCreateInfo viewportInfo;
        VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo;
        VkPipelineRasterizationStateCreateInfo rasterizationInfo;
        VkPipelineMultisampleStateCreateInfo multisampleInfo;
        VkPipelineColorBlendAttachmentState colorBlendAttachment;
        VkPipelineColorBlendStateCreateInfo colorBlendInfo;
        VkPipelineDepthStencilStateCreateInfo depthStencilInfo;
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
            void operator=(const Pipeline&) = delete;

            void bind(VkCommandBuffer commandBuffer);
            static void defaultPipelineConfigInfo(PipelineConfigInfo &configInfo, uint32_t width, uint32_t height);

        private:
            static std::vector<char> readFile(const std::string &filepath);

            void createGraphicsPipeline(
                const std::string &vertFilepath, 
                const std::string &fragFilepath, 
                const PipelineConfigInfo &configInfo);

            void createShaderModule(const std::vector<char> &code, VkShaderModule *shaderModule);

            Device &device;
            VkPipeline graphicsPipeline;
            VkShaderModule vertShaderModule;
            VkShaderModule fragShaderModule;
    };
}