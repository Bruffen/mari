#pragma once

#include "gui.hpp"

#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <vulkan/vk_enum_string_helper.h> 

namespace mari {
    Gui::Gui(Device &device, Window &window, Renderer &renderer) : device{device}, window{window} {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        io = &ImGui::GetIO();
        io->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io->ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
        io->ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        //io->ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
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
        DefaultObjects::getImageWhite()->descriptorGui = ImGui_ImplVulkan_AddTexture(
            DefaultObjects::getSamplerNearest(), 
            DefaultObjects::getImageWhite()->view, 
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        );
        DefaultObjects::getImageBlack()->descriptorGui = ImGui_ImplVulkan_AddTexture(
            DefaultObjects::getSamplerNearest(), 
            DefaultObjects::getImageBlack()->view, 
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        );
        DefaultObjects::getImageError()->descriptorGui = ImGui_ImplVulkan_AddTexture(
            DefaultObjects::getSamplerNearest(), 
            DefaultObjects::getImageError()->view, 
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        );
    }

    Gui::~Gui() {
        if (scene) {
            for (auto &image : scene->images) {
                ImGui_ImplVulkan_RemoveTexture(image->descriptorGui); // TODO will give errors if the same texture shows up more than once in scene->images
            }
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

        for (auto &[k, n] : scene->nodes) {
            if (n->mesh) {
                if(n->mesh->hasIndexBuffer) {
                    triangleCount += n->mesh->indexCount / 3;
                }
                else {
                    triangleCount += n->mesh->vertexCount / 3;
                }
            }
        }
    }

    void Gui::prepare(FrameInfo &frameInfo) {
        assert(scene  && "No scene was set in GUI! Call Gui::set() once and before preparing the frame.");

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        //ImGui::ShowDemoWindow();
        {
            ImGui::Begin("Mari");

            imGuiFramerate(frameInfo.deltaTime);
            ImGui::Text("Samples: %i", frameInfo.frameCounter);
            ImGui::Text("Triangle count: %i", triangleCount);

            if (ImGui::BeginTabBar("##tabs", ImGuiTabBarFlags_None)) {
                bool a = true;
                bool *a_ptr = &a;
                if (ImGui::BeginTabItem("Scene")) {
                    if (ImGui::TreeNode("Scene")) {
                        imGuiTransform(scene->transform);
                        for (std::shared_ptr<GameObject> &g : scene->topNodes) {
                            imGuiObject(*g);
                        }
                        ImGui::TreePop();
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

    void Gui::imGuiFramerate(float duration) {
        const  int     size = 50;
        static float   values[size] = {};
        static int     values_offset = 0;
        static double  refresh_time = 0.0;
        if (refresh_time == 0.0)
            refresh_time = ImGui::GetTime();
        while (refresh_time < ImGui::GetTime()) { // Create data at fixed 60 Hz rate for the demo
            values[values_offset] = duration * 1000.0f;
            values_offset = (values_offset + 1) % size;
            refresh_time += 1.0f / 60.0f;
        }

        float average = 0.0f;
        for (float v : values) {
            average += v;
        }
        average /= static_cast<float>(size);

        ImGui::Text("Frame duration: %.3f ms (%.1f FPS)", average, 1000.0f / average);
        ImGui::PlotLines("##frameplot", values, IM_ARRAYSIZE(values), values_offset, 0, 0.0f);
    }

    void Gui::imGuiObject(GameObject &g) {
        if (ImGui::TreeNode(g.name.c_str(), g.name.c_str())) {
            imGuiTransform(g.transform);
            if (g.mesh) {
                imGuiMesh(*g.mesh);
            }
            if (g.camera) {
                imGuiCamera(*g.camera);
            }
            for (const std::shared_ptr<GameObject> &child : g.children) {
                imGuiObject(*child);
            }
            ImGui::TreePop();
        }
    }

    void Gui::imGuiTransform(Transform &transform) {
        // TODO if (static) {} else {
        glm::vec3 p = transform.position;
        glm::vec3 r = transform.rotation;
        glm::vec3 s = transform.scale;
        glm::vec3 rd = glm::degrees(transform.rotation);

        ImGui::Text("Position:");
        ImGui::SameLine();
        ImGui::DragFloat3("##pos", &transform.position.x, 0.005f);

        ImGui::Text("Rotation:");
        ImGui::SameLine();
        ImGui::DragFloat3("##rot", &rd.x, 0.1f, 0.0f, 360.0f, "%.1f", ImGuiSliderFlags_WrapAround); // TODO degrees

        ImGui::Text("Scale:   ");
        ImGui::SameLine();
        ImGui::DragFloat3("##sca", &transform.scale.x, 0.005f);

        transform.rotation = glm::radians(rd);

        if (p != transform.position || r != transform.rotation || s != transform.scale) {
            inputChanged = true;
        }
    }

    void Gui::imGuiMesh(const Mesh &mesh) {
        if (ImGui::TreeNode(mesh.name.c_str(), ("Mesh: " + mesh.name).c_str())) {
            ImGui::Text("%i vertices", mesh.vertexCount);
            if (mesh.hasIndexBuffer) {
                ImGui::Text("%i indices", mesh.indexCount);
            }
            ImGui::Text("%i triangles", mesh.hasIndexBuffer ? mesh.indexCount / 3 : mesh.vertexCount / 3);

            std::vector<std::string> materials;
            for (const PrimMesh &s : mesh.primMeshes) {
                // avoid showing the same material multiple times
                if (std::find(materials.begin(), materials.end(), s.material->name) == materials.end()) {
                    materials.push_back(s.material->name);
                    imGuiSubMesh(s);
                }
            }
            ImGui::TreePop();
        }
    }

    void Gui::imGuiSubMesh(const PrimMesh &submesh) { // TODO dropdown list of materials
        //bool a = true;
        //bool *a_ptr = &a;
        //if (ImGui::CollapsingHeader(submesh.material->name.c_str(), a_ptr)) {
            imGuiMaterial(*submesh.material);
        //}
    }

    void Gui::imGuiMaterial(const Material &material) {
        if (ImGui::TreeNode(material.name.c_str(), ("Material: " + material.name).c_str())) {
            ImGuiSliderFlags silderFlags = ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_ClampOnInput | ImGuiSliderFlags_ClampZeroRange;

            MaterialConstants mc = material.data.constants;

            ImGui::ColorEdit4("Albedo", (float*)&material.data.constants.albedo, ImGuiColorEditFlags_Float);
            imGuiImage(*material.textures.albedo);
            ImGui::SliderFloat("Metallic", (float*)&material.data.constants.metallic, 0.0f, 1.0f, "%.3f", silderFlags);
            ImGui::SliderFloat("Roughness", (float*)&material.data.constants.roughness, 0.0f, 1.0f, "%.3f", silderFlags);
            imGuiImage(*material.textures.metallicRoughness);
            ImGui::ColorEdit4("Emission", (float*)&material.data.constants.emission, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);

            ImGui::TreePop();

            if (mc.albedo != material.data.constants.albedo || 
                mc.metallic != material.data.constants.metallic || 
                mc.roughness != material.data.constants.roughness ||
                mc.emission != material.data.constants.emission) {
                inputChanged = true;
                scene->materialDataBuffer->update(material.index * sizeof(MaterialData), sizeof(MaterialConstants), &material.data.constants);
            }
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

    void Gui::imGuiCamera(Camera &camera) {
        if (ImGui::TreeNode("##cam", "Camera")) {
            float fov = camera.fov;
            float fovDegrees = glm::degrees(fov);

            ImGui::Text("Field of View:");
            ImGui::SameLine();
            ImGui::DragFloat("##fov", &fovDegrees, 0.1f, glm::degrees(Camera::MIN_FOV), glm::degrees(Camera::MAX_FOV), "%.1f", ImGuiSliderFlags_ClampOnInput);

            camera.fov = glm::radians(fovDegrees);
            if (fov != camera.fov) {
                inputChanged = true;
            }
            ImGui::TreePop();
        }
    }
}