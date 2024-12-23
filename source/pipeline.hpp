#pragma once

#include "device.hpp"
#include "renderer.hpp"
#include "buffer.hpp"

#include <string>
#include <vector>

namespace mari {
    struct PipelineConfigInfo {
        PipelineConfigInfo() = default;
        PipelineConfigInfo(const PipelineConfigInfo&) = delete;
        PipelineConfigInfo& operator=(const PipelineConfigInfo&) = delete;

        std::vector<VkVertexInputBindingDescription>   bindingDescriptions{};
        std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};
        VkPipelineViewportStateCreateInfo              viewportInfo;
        VkPipelineInputAssemblyStateCreateInfo         inputAssemblyInfo;
        VkPipelineRasterizationStateCreateInfo         rasterizationInfo;
        VkPipelineMultisampleStateCreateInfo           multisampleInfo;
        VkPipelineColorBlendAttachmentState            colorBlendAttachment;
        VkPipelineColorBlendStateCreateInfo            colorBlendInfo;
        VkPipelineDepthStencilStateCreateInfo          depthStencilInfo;
        std::vector<VkDynamicState>                    dynamicStateEnables;
        VkPipelineDynamicStateCreateInfo               dynamicStateInfo;
        VkPipelineLayout                               pipelineLayout = nullptr;
        VkRenderPass                                   renderPass = nullptr;
        uint32_t                                       subpass = 0;
    };

    class Pipeline {
        public:
            Pipeline(Device &device);

            ~Pipeline();

            Pipeline(const Pipeline&) = delete;
            Pipeline &operator=(const Pipeline&) = delete;

            void                            bind(VkCommandBuffer commandBuffer);
            static void                     defaultPipelineConfigInfo(PipelineConfigInfo &configInfo);
            static void                     enableAlphaBlending(PipelineConfigInfo &configInfo);
            void                            createGraphicsPipeline(
                                                const std::string &vertFilepath, 
                                                const std::string &fragFilepath, 
                                                const PipelineConfigInfo &configInfo);
            void                            createRayTracingPipeline(VkPipelineLayout &pipelineLayout);
            VkStridedDeviceAddressRegionKHR raygenSBTEntry, missSBTEntry, hitSBTEntry, callableSBTEntry; // TODO private?
        private:
            VkPipelineShaderStageCreateInfo loadShader(const std::string &filepath, VkShaderStageFlagBits flag);
            std::vector<char>               readFile(const std::string &filepath);
            VkShaderModule                  createShaderModule(const std::vector<char> &code);
            void                            createShaderBindingTables();


            Device &device;
            VkPipeline handle;
            VkPipelineBindPoint pipelineBindPoint;
            std::vector<VkShaderModule> shaderModules;
            std::vector<VkRayTracingShaderGroupCreateInfoKHR> shaderGroups; // TODO not sure if it's best in here. look into it
            std::unique_ptr<Buffer> raygenSBT, missSBT, hitSBT; // TODO group sbt with sbtentries in a struct?
    };
}