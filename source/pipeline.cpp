#include "pipeline.hpp"
#include "components/mesh.hpp"
#include "vk_helper.hpp"

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <cassert>

namespace mari {
    Pipeline::Pipeline(Device &device) : device{device} {}

    Pipeline::~Pipeline() {
        for (VkShaderModule shader : shaderModules) {
            vkDestroyShaderModule(device.handle(), shader, nullptr);
        }
        vkDestroyPipeline(device.handle(), handle, nullptr);
    }

    std::vector<char> Pipeline::readFile(const std::string &filepath) {
        std::ifstream file{filepath, std::ios::ate | std::ios::binary};

        if (!file.is_open()) {
            throw std::runtime_error("Failed to open file: " + filepath);
        }

        size_t size =  static_cast<size_t>(file.tellg());
        std::vector<char> buffer(size);

        file.seekg(0);
        file.read(buffer.data(), size);
        file.close();

        return buffer;
    }

    VkShaderModule Pipeline::createShaderModule(const std::vector<char> &code) {
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

        VkShaderModule shaderModule;
        if (vkCreateShaderModule(device.handle(), &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create shader module.");
        }

        return shaderModule;
    }

    VkPipelineShaderStageCreateInfo Pipeline::loadShader(const std::string &filepath, VkShaderStageFlagBits stage) {
        VkShaderModule shaderModule = Pipeline::createShaderModule(Pipeline::readFile(filepath));

        VkPipelineShaderStageCreateInfo shaderStage{};
        shaderStage.sType                   = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStage.stage                   = stage;
        shaderStage.module                  = shaderModule;
        shaderStage.pName                   = "main";
        shaderStage.flags                   = 0;
        shaderStage.pNext                   = nullptr;
        shaderStage.pSpecializationInfo     = nullptr;
        assert(shaderStage.module != VK_NULL_HANDLE);

        shaderModules.push_back(std::move(shaderModule));
        return shaderStage;
    }

    void Pipeline::createGraphicsPipeline(const std::string &vertFilepath, const std::string &fragFilepath, const PipelineConfigInfo &configInfo) {
        assert(configInfo.pipelineLayout != VK_NULL_HANDLE && "Cannot create graphics pipeline: No pipelineLayout provided in configInfo");
        assert(configInfo.renderPass     != VK_NULL_HANDLE && "Cannot create graphics pipeline: No renderPass provided in configInfo");

        pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

        VkPipelineShaderStageCreateInfo shaderStages[] = {
            loadShader(vertFilepath, VK_SHADER_STAGE_VERTEX_BIT),
            loadShader(fragFilepath, VK_SHADER_STAGE_FRAGMENT_BIT)
        };

        auto &bindingDescriptions = configInfo.bindingDescriptions;
        auto &attributeDescriptions = configInfo.attributeDescriptions;
        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(bindingDescriptions.size());
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
        vertexInputInfo.pVertexBindingDescriptions = bindingDescriptions.data();
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &configInfo.inputAssemblyInfo;
        pipelineInfo.pViewportState = &configInfo.viewportInfo;
        pipelineInfo.pRasterizationState = &configInfo.rasterizationInfo;
        pipelineInfo.pMultisampleState = &configInfo.multisampleInfo;
        pipelineInfo.pColorBlendState = &configInfo.colorBlendInfo;
        pipelineInfo.pDepthStencilState = &configInfo.depthStencilInfo;
        pipelineInfo.pDynamicState = &configInfo.dynamicStateInfo;

        pipelineInfo.layout = configInfo.pipelineLayout;
        pipelineInfo.renderPass = configInfo.renderPass;
        pipelineInfo.subpass = configInfo.subpass;

        pipelineInfo.basePipelineIndex = -1;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

        if (vkCreateGraphicsPipelines(device.handle(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &handle) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create graphics pipeline");
        }
    }

    void Pipeline::createRayTracingPipeline(VkPipelineLayout &pipelineLayout) {
        assert(pipelineLayout != VK_NULL_HANDLE && "Cannot create ray tracing pipeline: Null PipelineLayout");

        pipelineBindPoint = VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR;

        std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
        // TODO this function should be generic, therefore shader information should be passed here from ray_tracing_system
        {
            shaderStages.push_back(loadShader("../../shaders/raygen.rgen.spv", VK_SHADER_STAGE_RAYGEN_BIT_KHR));
            VkRayTracingShaderGroupCreateInfoKHR raygenGroupCreateInfo{};
            raygenGroupCreateInfo.sType                     = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
            raygenGroupCreateInfo.type                      = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
            raygenGroupCreateInfo.generalShader             = static_cast<uint32_t>(shaderStages.size() - 1);
            raygenGroupCreateInfo.closestHitShader          = VK_SHADER_UNUSED_KHR;
            raygenGroupCreateInfo.anyHitShader              = VK_SHADER_UNUSED_KHR;
            raygenGroupCreateInfo.intersectionShader        = VK_SHADER_UNUSED_KHR;
            shaderGroups.push_back(raygenGroupCreateInfo);
        }

        {
            shaderStages.push_back(loadShader("../../shaders/miss.rmiss.spv", VK_SHADER_STAGE_MISS_BIT_KHR));
            VkRayTracingShaderGroupCreateInfoKHR missGroupCreateInfo{};
            missGroupCreateInfo.sType                       = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
            missGroupCreateInfo.type                        = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
            missGroupCreateInfo.generalShader               = static_cast<uint32_t>(shaderStages.size() - 1);
            missGroupCreateInfo.closestHitShader            = VK_SHADER_UNUSED_KHR;
            missGroupCreateInfo.anyHitShader                = VK_SHADER_UNUSED_KHR;
            missGroupCreateInfo.intersectionShader          = VK_SHADER_UNUSED_KHR;
            shaderGroups.push_back(missGroupCreateInfo);
        }

        {
            shaderStages.push_back(loadShader("../../shaders/closesthit.rchit.spv", VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR));
            VkRayTracingShaderGroupCreateInfoKHR chitGroupCreateInfo{};
            chitGroupCreateInfo.sType                       = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
            chitGroupCreateInfo.type                        = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
            chitGroupCreateInfo.generalShader               = VK_SHADER_UNUSED_KHR;
            chitGroupCreateInfo.closestHitShader            = static_cast<uint32_t>(shaderStages.size() - 1);
            chitGroupCreateInfo.anyHitShader                = VK_SHADER_UNUSED_KHR;
            chitGroupCreateInfo.intersectionShader          = VK_SHADER_UNUSED_KHR;
            shaderGroups.push_back(chitGroupCreateInfo);
        }

        VkRayTracingPipelineCreateInfoKHR rayTracingPipelineCreateInfo{};
        rayTracingPipelineCreateInfo.sType                  = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
        rayTracingPipelineCreateInfo.stageCount             = static_cast<uint32_t>(shaderStages.size());
        rayTracingPipelineCreateInfo.pStages                = shaderStages.data();
        rayTracingPipelineCreateInfo.groupCount             = static_cast<uint32_t>(shaderGroups.size());
        rayTracingPipelineCreateInfo.pGroups                = shaderGroups.data();
        rayTracingPipelineCreateInfo.layout                 = pipelineLayout;
        rayTracingPipelineCreateInfo.maxPipelineRayRecursionDepth = device.propertiesRT.maxRayRecursionDepth;
        std::cout << "Maximum recursion depth of: " << rayTracingPipelineCreateInfo.maxPipelineRayRecursionDepth << std::endl;
        if (vkCreateRayTracingPipelinesKHR(device.handle(), VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &rayTracingPipelineCreateInfo, nullptr, &handle)) {
            throw std::runtime_error("Failed to create ray tracing pipeline");
        }

        createShaderBindingTables();
    }

    void Pipeline::bind(VkCommandBuffer commandBuffer) {
        vkCmdBindPipeline(commandBuffer, pipelineBindPoint, handle);
    }

    void Pipeline::defaultPipelineConfigInfo(PipelineConfigInfo &configInfo) {
        configInfo.inputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        configInfo.inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        configInfo.inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;

        configInfo.viewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        configInfo.viewportInfo.viewportCount = 1;
        configInfo.viewportInfo.pViewports = nullptr;
        configInfo.viewportInfo.scissorCount = 1;
        configInfo.viewportInfo.pScissors = nullptr;

        configInfo.rasterizationInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        configInfo.rasterizationInfo.depthClampEnable = VK_FALSE;
        configInfo.rasterizationInfo.rasterizerDiscardEnable = VK_FALSE;
        configInfo.rasterizationInfo.polygonMode = VK_POLYGON_MODE_FILL;
        configInfo.rasterizationInfo.lineWidth = 1.0f;
        configInfo.rasterizationInfo.cullMode = VK_CULL_MODE_NONE;
        configInfo.rasterizationInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
        configInfo.rasterizationInfo.depthBiasEnable = VK_FALSE;
        configInfo.rasterizationInfo.depthBiasConstantFactor = 0.0f;  // Optional
        configInfo.rasterizationInfo.depthBiasClamp = 0.0f;           // Optional
        configInfo.rasterizationInfo.depthBiasSlopeFactor = 0.0f;     // Optional

        configInfo.multisampleInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        configInfo.multisampleInfo.sampleShadingEnable = VK_FALSE;
        configInfo.multisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        configInfo.multisampleInfo.minSampleShading = 1.0f;           // Optional
        configInfo.multisampleInfo.pSampleMask = nullptr;             // Optional
        configInfo.multisampleInfo.alphaToCoverageEnable = VK_FALSE;  // Optional
        configInfo.multisampleInfo.alphaToOneEnable = VK_FALSE;       // Optional

        configInfo.colorBlendAttachment.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
            VK_COLOR_COMPONENT_A_BIT;
        configInfo.colorBlendAttachment.blendEnable = VK_FALSE;
        configInfo.colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;   // Optional
        configInfo.colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;  // Optional
        configInfo.colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;              // Optional
        configInfo.colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;   // Optional
        configInfo.colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;  // Optional
        configInfo.colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;              // Optional

        configInfo.colorBlendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        configInfo.colorBlendInfo.logicOpEnable = VK_FALSE;
        configInfo.colorBlendInfo.logicOp = VK_LOGIC_OP_COPY;  // Optional
        configInfo.colorBlendInfo.attachmentCount = 1;
        configInfo.colorBlendInfo.pAttachments = &configInfo.colorBlendAttachment;
        configInfo.colorBlendInfo.blendConstants[0] = 0.0f;  // Optional
        configInfo.colorBlendInfo.blendConstants[1] = 0.0f;  // Optional
        configInfo.colorBlendInfo.blendConstants[2] = 0.0f;  // Optional
        configInfo.colorBlendInfo.blendConstants[3] = 0.0f;  // Optional

        configInfo.depthStencilInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        configInfo.depthStencilInfo.depthTestEnable = VK_TRUE;
        configInfo.depthStencilInfo.depthWriteEnable = VK_TRUE;
        configInfo.depthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS;
        configInfo.depthStencilInfo.depthBoundsTestEnable = VK_FALSE;
        configInfo.depthStencilInfo.minDepthBounds = 0.0f;  // Optional
        configInfo.depthStencilInfo.maxDepthBounds = 1.0f;  // Optional
        configInfo.depthStencilInfo.stencilTestEnable = VK_FALSE;
        configInfo.depthStencilInfo.front = {};  // Optional
        configInfo.depthStencilInfo.back = {};   // Optional

        configInfo.dynamicStateEnables = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        configInfo.dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        configInfo.dynamicStateInfo.pDynamicStates = configInfo.dynamicStateEnables.data();
        configInfo.dynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(configInfo.dynamicStateEnables.size());
        configInfo.dynamicStateInfo.flags = 0;

        configInfo.bindingDescriptions = Mesh::Vertex::getBindingDescriptions();
        configInfo.attributeDescriptions = Mesh::Vertex::getAttributeDescriptions();
    }

    void Pipeline::enableAlphaBlending(PipelineConfigInfo &configInfo) {
        configInfo.colorBlendAttachment.blendEnable = VK_TRUE;
        configInfo.colorBlendAttachment.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
            VK_COLOR_COMPONENT_A_BIT;
        configInfo.colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        configInfo.colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        configInfo.colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        configInfo.colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        configInfo.colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        configInfo.colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    }

    inline uint32_t alignedSize(uint32_t value, uint32_t alignment) {
        return (value + alignment - 1) & ~(alignment - 1);
    }

    void Pipeline::createShaderBindingTables() {
        const uint32_t              handleSize          = device.propertiesRT.shaderGroupHandleSize;
        const uint32_t              handleSizeAligned   = alignedSize(device.propertiesRT.shaderGroupHandleSize, device.propertiesRT.shaderGroupHandleAlignment);
        const uint32_t              handleAlignment     = device.propertiesRT.shaderGroupHandleAlignment;
        const uint32_t              groupCount          = static_cast<uint32_t>(shaderGroups.size());
        const uint32_t              sbtSize             = groupCount * handleSizeAligned;
        const VkBufferUsageFlags    sbtBufferUsageFlags = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        // TODO vma

        raygenSBT = std::make_unique<Buffer>(device, handleSize, 1, sbtBufferUsageFlags, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        missSBT   = std::make_unique<Buffer>(device, handleSize, 1, sbtBufferUsageFlags, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        hitSBT    = std::make_unique<Buffer>(device, handleSize, 1, sbtBufferUsageFlags, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

        std::vector<uint8_t> shaderHandleStorage(sbtSize);
        if (vkGetRayTracingShaderGroupHandlesKHR(device.handle(), handle, 0, groupCount, sbtSize, shaderHandleStorage.data())) {
            throw std::runtime_error("Failed to get ray tracing shader group handles");
        }

        uint8_t *data = static_cast<uint8_t *>(raygenSBT->getMappedMemory());
        memcpy(data, shaderHandleStorage.data(), handleSize);
        data = static_cast<uint8_t *>(missSBT->getMappedMemory());
        memcpy(data, shaderHandleStorage.data() + handleSizeAligned, handleSize);
        data = static_cast<uint8_t *>(hitSBT->getMappedMemory());
        memcpy(data, shaderHandleStorage.data() + handleSizeAligned * 2, handleSize);

        raygenSBT->unmap();
        missSBT->unmap();
        hitSBT->unmap();

        // Create sbt entries for the pipeline
        raygenSBTEntry.deviceAddress    = raygenSBT->deviceAddress();
        raygenSBTEntry.size             = handleSizeAligned;
        raygenSBTEntry.stride           = handleSizeAligned;

        missSBTEntry.deviceAddress      = missSBT->deviceAddress();
        missSBTEntry.size               = handleSizeAligned;
        missSBTEntry.stride             = handleSizeAligned;

        hitSBTEntry.deviceAddress       = hitSBT->deviceAddress();
        hitSBTEntry.size                = handleSizeAligned;
        hitSBTEntry.stride              = handleSizeAligned;

        callableSBTEntry.deviceAddress  = VkDeviceAddress(0);
        callableSBTEntry.size           = handleSizeAligned;
        callableSBTEntry.stride         = handleSizeAligned;
    }
}