#include "renderer.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include <array>
#include <cassert>
#include <stdexcept>

namespace mari {
    struct SimplePushConstantData {
        glm::vec2 offset;
        alignas(16) glm::vec3 color;
    };

    Renderer::Renderer() {
        loadModels();
        createPipelineLayout();
        recreateSwapchain();
        createCommandBuffers();
    }

    Renderer::~Renderer() {
        vkDestroyPipelineLayout(device.device(), pipelineLayout, nullptr);
    }

    void Renderer::run() {
        while (!window.shouldClose()) {
            glfwPollEvents();
            drawFrame();
        }

        vkDeviceWaitIdle(device.device());
    };

    void Renderer::loadModels() {
        std::vector<Model::Vertex> vertices {
            {{ 0.0f, -0.5f}, {1.0, 0.0, 0.0}},
            {{ 0.5f,  0.5f}, {0.0, 1.0, 0.0}},
            {{-0.5f,  0.5f}, {0.0, 0.0, 1.0}}
        };

        model = std::make_unique<Model>(device, vertices);
    }

    void Renderer::createPipelineLayout() {
        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(SimplePushConstantData);


        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 0;
        pipelineLayoutInfo.pSetLayouts = nullptr;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

        if (vkCreatePipelineLayout(device.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create pipeline layout");
        }
    }

    void Renderer::createPipeline() {
        assert(swapchain != nullptr && "Cannot create pipeline before swap chain");
        assert(pipelineLayout != nullptr && "Cannot create pipeline before pipeline layout");

        PipelineConfigInfo pipelineConfig{};
        Pipeline::defaultPipelineConfigInfo(pipelineConfig);
        pipelineConfig.renderPass = swapchain->getRenderPass();
        pipelineConfig.pipelineLayout = pipelineLayout;
        pipeline = std::make_unique<Pipeline>(device, "../../shaders/simple.vert.spv", "../../shaders/simple.frag.spv", pipelineConfig);
    }

    void Renderer::recreateSwapchain() {
        auto extent = window.getExtent();
        while (extent.width == 0 || extent.height == 0) {
            extent = window.getExtent();
            glfwWaitEvents();
        }

        vkDeviceWaitIdle(device.device());
        if (swapchain == nullptr) {
            swapchain = std::make_unique<Swapchain>(device, extent);
        } else {
            swapchain = std::make_unique<Swapchain>(device, extent, std::move(swapchain));
            if (swapchain->imageCount() != commandBuffers.size()) {
                freeCommandBuffers();
                createCommandBuffers();
            }
        }

        // if render pass compatible do nothing else
        createPipeline();
    }

    void Renderer::createCommandBuffers() {
        commandBuffers.resize(swapchain->imageCount());

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = device.getCommandPool();
        allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

        if (vkAllocateCommandBuffers(device.device(), &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate command buffers");
        }
    }

    void Renderer::freeCommandBuffers() {
        vkFreeCommandBuffers(
            device.device(), 
            device.getCommandPool(), 
            static_cast<uint32_t>(commandBuffers.size()), 
            commandBuffers.data()
        );
        commandBuffers.clear();
    }

    void Renderer::recordCommandBuffer(int imageIndex) {
        static int frame = 0;
        frame = (frame + 1) % 1000;

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if (vkBeginCommandBuffer(commandBuffers[imageIndex], &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("Failed to begin recording command buffer");
        }

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = swapchain->getRenderPass();
        renderPassInfo.framebuffer = swapchain->getFrameBuffer(imageIndex);

        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = swapchain->getSwapchainExtent();

        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = {0.01f, 0.01f, 0.01f, 1.0f};
        clearValues[1].depthStencil = {1.0f, 0};
        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(commandBuffers[imageIndex], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(swapchain->getSwapchainExtent().width);
        viewport.height = static_cast<float>(swapchain->getSwapchainExtent().height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        VkRect2D scissor{{0, 0}, swapchain->getSwapchainExtent()};
        vkCmdSetViewport(commandBuffers[imageIndex], 0, 1, &viewport);
        vkCmdSetScissor(commandBuffers[imageIndex], 0, 1, &scissor);

        pipeline->bind(commandBuffers[imageIndex]);
        model->bind(commandBuffers[imageIndex]);

        for (int j = 0; j < 4; j++) {
            SimplePushConstantData push{};
            push.offset = {-0.5f + frame * 0.002f, -0.4f + j * 0.25f};
            push.color = {0.0f, 0.0f, 0.2f + 0.2f * j};

            vkCmdPushConstants(
                commandBuffers[imageIndex], 
                pipelineLayout, 
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 
                0, 
                sizeof(SimplePushConstantData),
                &push
            );
            model->draw(commandBuffers[imageIndex]);
        }


        vkCmdEndRenderPass(commandBuffers[imageIndex]);
        if (vkEndCommandBuffer(commandBuffers[imageIndex]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to record command buffer");
        }
    }

    void Renderer::drawFrame() {
        uint32_t imageIndex;
        auto result = swapchain->acquireNextImage(&imageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            recreateSwapchain();
            return;
        }

        if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("Failed to acquire swapchain image");
        }

        recordCommandBuffer(imageIndex);
        result = swapchain->submitCommandBuffers(&commandBuffers[imageIndex], &imageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || window.wasWindowResized()) {
            window.resetWindowsResizedFlag();
            recreateSwapchain();
            return;
        }

        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to present swapchain image");
        }
    }
}