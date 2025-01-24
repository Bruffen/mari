#pragma once

#include "gui.hpp"

#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <vulkan/vk_enum_string_helper.h> 

namespace mari {
    Gui::Gui(Device &device, Window &window, Renderer &renderer) : device{device}, window{window} {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        imGui = ImGui::GetIO();
        imGui.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        imGui.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
        imGui.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        imGui.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
        ImGui::StyleColorsDark();

        std::vector<VkDescriptorPoolSize> poolSizes = {{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 }};
        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.flags              = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolInfo.maxSets            = 1000; // TODO calculate based on scene->images->size() + imgui
        poolInfo.poolSizeCount      = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes         = poolSizes.data();
        vkCreateDescriptorPool(device.handle(), &poolInfo, nullptr, &descriptorPool);

        ImGui_ImplGlfw_InitForVulkan(window.getGLFWwindow(), true);
        ImGui_ImplVulkan_InitInfo initInfo{};
        initInfo.Instance           = device.getInstance();
        initInfo.PhysicalDevice     = device.getPhysicalDevice();
        initInfo.Device             = device.handle();
        initInfo.QueueFamily        = device.findPhysicalQueueFamilies().presentFamily;
        initInfo.Queue              = device.presentQueue();
        initInfo.PipelineCache      = nullptr;
        initInfo.DescriptorPool     = descriptorPool;
        initInfo.RenderPass         = renderer.getSwapchainRenderPass();
        initInfo.Subpass            = 0;
        initInfo.MinImageCount      = static_cast<uint32_t>(renderer.getSwapchain().imageCount());
        initInfo.ImageCount         = static_cast<uint32_t>(renderer.getSwapchain().imageCount());
        initInfo.MSAASamples        = VK_SAMPLE_COUNT_1_BIT;
        initInfo.Allocator          = nullptr;
        initInfo.CheckVkResultFn    = checkVkResult;
        ImGui_ImplVulkan_Init(&initInfo);

        std::shared_ptr<Image> s = DefaultObjects::getImageWhite();
        DefaultObjects::getImageWhite()->descriptorGui = ImGui_ImplVulkan_AddTexture(DefaultObjects::getSamplerNearest(), DefaultObjects::getImageWhite()->view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        DefaultObjects::getImageBlack()->descriptorGui = ImGui_ImplVulkan_AddTexture(DefaultObjects::getSamplerNearest(), DefaultObjects::getImageBlack()->view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        DefaultObjects::getImageError()->descriptorGui = ImGui_ImplVulkan_AddTexture(DefaultObjects::getSamplerNearest(), DefaultObjects::getImageError()->view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    Gui::~Gui() {
        for (auto &image : scene->images) {
            ImGui_ImplVulkan_RemoveTexture(image->descriptorGui);
        }
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        vkDestroyDescriptorPool(device.handle(), descriptorPool, nullptr);
        ImGui::DestroyContext();
    }

    void Gui::checkVkResult(VkResult err) {
        if (err == 0)
            return;
        fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
        if (err < 0)
            abort();
    }

    void Gui::set(std::shared_ptr<Scene> scene) {
        this->scene = scene;
        for (auto &image : this->scene->images) {
            image->descriptorGui = ImGui_ImplVulkan_AddTexture(DefaultObjects::getSamplerNearest(), image->view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }
    }

    void Gui::prepare(FrameInfo &frameInfo) {
        assert(scene  && "No scene was set in GUI! Call Gui::set() once and before preparing the frame.");

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        {
            ImGui::Begin("Mari");
            ImGui::Text("Frame duration: %.3f ms (%.1f FPS)", frameInfo.deltaTime * 1000.0f, 1.0f / frameInfo.deltaTime);

            if (ImGui::BeginTabBar("##tabs", ImGuiTabBarFlags_None)) {
                bool a = true;
                bool *a_ptr = &a;
                if (ImGui::BeginTabItem("Scene")) {
                    for (const std::shared_ptr<GameObject> &g : scene->topNodes) {
                        imGuiObject(*g);
                    }
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Textures")) {
                    for (auto &image : scene->images) {
                        if (ImGui::CollapsingHeader(image->name.c_str(), a_ptr)) {
                            ImGui::Text("%s", string_VkFormat(image->format));
                            ImGui::Text("%ix%i", image->size.width, image->size.height);
                            float ratio = 240.0f / glm::max(image->size.height, image->size.width);
                            ImGui::Image((ImTextureID)image->descriptorGui, ImVec2(static_cast<float>(image->size.width) * ratio, static_cast<float>(image->size.height) * ratio));
                        }
                    }
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
            ImGui::End();
        }

        ImGui::Render();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }

    void Gui::render(VkCommandBuffer commandBuffer) {
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
    }


    void Gui::imGuiObject(const GameObject &g) {
        if (ImGui::TreeNode(g.name.c_str(), g.name.c_str())) {
            imGuiTransform(g.transform);
            if (g.mesh) {
                imGuiMesh(*g.mesh);
            }
            for (const std::shared_ptr<GameObject> &child : g.children) {
                imGuiObject(*child);
            }
            ImGui::TreePop();
        }
    }

    void Gui::imGuiTransform(const Transform &transform) {
        ImGui::Text("Position: x: %.2f, y: %.2f, z: %.2f", transform.position.x, transform.position.y, transform.position.z);
        ImGui::Text("Rotation: x: %.2f°, y: %.2f°, z: %.2f°", glm::degrees(transform.rotation.x), glm::degrees(transform.rotation.y), glm::degrees(transform.rotation.z));
        ImGui::Text("Scale:    x: %.2f, y: %.2f, z: %.2f", transform.scale.x, transform.scale.y, transform.scale.z);
    }

    void Gui::imGuiMesh(const Mesh &mesh) {
        if (ImGui::TreeNode(mesh.name.c_str(), ("Mesh: " + mesh.name).c_str())) {
            ImGui::Text("%i vertices", mesh.vertexCount);
            if (mesh.hasIndexBuffer) {
                ImGui::Text("%i indices", mesh.indexCount);
            }
            ImGui::Text("%i triangles", mesh.hasIndexBuffer ? mesh.indexCount / 3 : mesh.vertexCount / 3);

            std::vector<std::string> materials;
            for (const SubMesh &s : mesh.submeshes) {
                // avoid showing the same material multiple times
                if (std::find(materials.begin(), materials.end(), s.material->name) == materials.end()) {
                    materials.push_back(s.material->name);
                    imGuiSubMesh(s);
                }
            }
            ImGui::TreePop();
        }
    }

    void Gui::imGuiSubMesh(const SubMesh &submesh) {
        //bool a = true;
        //bool *a_ptr = &a;
        //if (ImGui::CollapsingHeader(submesh.material->name.c_str(), a_ptr)) {
            imGuiMaterial(*submesh.material);
        //}
    }

    void Gui::imGuiMaterial(const Material &material) {
        if (ImGui::TreeNode(material.name.c_str(), ("Material: " + material.name).c_str())) {
            ImGui::Text("Color: R: %.2f, G: %.2f, B: %.2f, A: %.2f", material.constants.color.r, material.constants.color.g, material.constants.color.b, material.constants.color.a);
            ImGui::Text("Metallic: %.3f", material.constants.metallic);
            ImGui::Text("Roughness: %.3f", material.constants.roughness);
            imGuiImage(*material.resources.colorImage);
            imGuiImage(*material.resources.metallicRoughnessImage);
            ImGui::TreePop();
        }
    }

    void Gui::imGuiImage(const Image &image) {
        if (&image == nullptr) return;
        ImGui::Text(image.name.c_str());
        ImGui::Text("%s", string_VkFormat(image.format));
        ImGui::Text("%ix%i", image.size.width, image.size.height);
        float ratio = 128.0f / glm::max(image.size.height, image.size.width);
        ImGui::Image((ImTextureID)image.descriptorGui, ImVec2(static_cast<float>(image.size.width) * ratio, static_cast<float>(image.size.height) * ratio));    
    }
}