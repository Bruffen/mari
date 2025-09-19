#pragma once

#include "device.hpp"
#include "renderer.hpp"
#include "buffer.hpp"
#include "pipeline_layout.hpp"

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

            VkPipelineBindPoint             bindPoint() { return pipelineBindPoint; }
            
            void                            bind(VkCommandBuffer commandBuffer);
            static void                     defaultPipelineConfigInfo(PipelineConfigInfo &configInfo);
            static void                     enableAlphaBlending(PipelineConfigInfo &configInfo);
            void                            createGraphicsPipeline(
                                                const std::string &vertFilepath, 
                                                const std::string &fragFilepath, 
                                                const PipelineConfigInfo &configInfo);
            void                            createRayTracingPipeline(
                                                const PipelineLayout &pipelineLayout,
                                                const std::vector<std::string> &shadersRayGeneration,
                                                const std::vector<std::string> &shadersMiss,
                                                const std::vector<std::string> &shadersClosestHit,
                                                const std::vector<std::string> &shadersAnyHit,
                                                const std::vector<std::string> &shadersCallable);
            void                            createComputePipeline(const std::string &compFilepath, const PipelineLayout& pipelineLayout);
            VkStridedDeviceAddressRegionKHR raygenSBTEntry, missSBTEntry, hitSBTEntry, callableSBTEntry; // TODO private?
        private:
            VkPipelineShaderStageCreateInfo loadShader(const std::string &filepath, VkShaderStageFlagBits flag);
            std::vector<char>               readFile(const std::string &filepath);
            VkShaderModule                  createShaderModule(const std::vector<char> &code);
            void                            createShaderBindingTables(uint32_t countRaygen, uint32_t countMiss, uint32_t countHit, uint32_t countCallable);


            Device &device;
            VkPipeline handle;
            VkPipelineBindPoint pipelineBindPoint;
            std::vector<VkShaderModule> shaderModules;
            std::vector<VkRayTracingShaderGroupCreateInfoKHR> shaderGroups;
            std::unique_ptr<Buffer> raygenSBT, missSBT, hitSBT; // TODO group sbt with sbtentries in a struct?
    };
}