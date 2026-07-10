#pragma once

#include "gui.hpp"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define __STDC_LIB_EXT1__
#include "stb_image_write.h"

#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <vulkan/vk_enum_string_helper.h> 

#include <iomanip>
#include <ctime>
#include <sstream>

namespace mari {
    Gui::Gui(Device &device, Window &window, Renderer &renderer, RayTracingSystem &system) : device{device}, window{window}, system{system} {
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
#ifdef MARI_DEBUG
            ImGui::Text("========================");
            ImGui::Text("DEBUG BUILD");
            ImGui::Text("========================");
#endif
            imGuiFramerate(frameInfo.deltaTime);
            ImGui::Text("Samples: %i", frameInfo.frameCounter + 1);
            ImGui::Text("Triangle count: %i", triangleCount);

            ImGui::BeginChild("TabChild", ImVec2(0, 0), ImGuiChildFlags_None);
            if (ImGui::BeginTabBar("##tabs", ImGuiTabBarFlags_None)) {
                bool a = true;
                bool *a_ptr = &a;

                // Scene tab
                if (ImGui::BeginTabItem("Scene")) {

                    ImGui::BeginChild("SceneChild", ImVec2(0, 200), ImGuiChildFlags_None);
                    for (std::shared_ptr<Node> &g : scene->topNodes) {                            
                        imGuiObject(*g);
                    }
                    ImGui::EndChild();
                    
                    if (ImGui::BeginTabBar("##tabsInspector", ImGuiTabBarFlags_None)) {
                        if (ImGui::BeginTabItem("Inspector")) {
                            ImGui::BeginChild("InspectChild", ImVec2(0, -10), ImGuiChildFlags_None);
                            if (sceneObjectSelected > 0) {
                                const std::shared_ptr<Node> g = scene->getNode(sceneObjectSelected);
                                if (g != nullptr) {
                                    imGuiInspector(*g);
                                }
                            }
                            ImGui::EndChild();
                            ImGui::EndTabItem();
                        }
                        ImGui::EndTabBar();
                    }

                    ImGui::EndTabItem();
                }

                // Textures tab
                if (ImGui::BeginTabItem("Textures")) {
                    for (auto &image : scene->images) {
                        if (ImGui::CollapsingHeader(image->name.c_str(), a_ptr)) {
                            imGuiImage(*image);
                        }
                    }
                    ImGui::EndTabItem();
                }
                
                // Materials tab
                if (ImGui::BeginTabItem("Materials")) {
                    for (auto &material : scene->materials) {
                        if (ImGui::CollapsingHeader((std::to_string(material->index) + ". " + material->name).c_str(), a_ptr)) {
                            imGuiMaterial(*material);
                        }
                    }
                    ImGui::EndTabItem();
                }

                // Render tab
                imGuiRender(frameInfo);

                ImGui::EndTabBar();
            }
            ImGui::EndChild();
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

    void Gui::imGuiInspector(Node &g) {
        ImGui::Text(("ID: " + std::to_string(g.getId())).c_str());
        if (imGuiTransform(g.transform) && g.volume) {
            if (g.transform.scale.x == 0.0) g.transform.scale.x = 0.00001f;
            if (g.transform.scale.y == 0.0) g.transform.scale.y = 0.00001f;
            if (g.transform.scale.z == 0.0) g.transform.scale.z = 0.00001f;
            g.setVolumeTransform();
        }
        if (g.mesh) {
            imGuiMesh(*g.mesh);
        }
        if (g.camera) {
            imGuiCamera(*g.camera);
        }
        if (g.volume) {
            imGuiVolume(*g.volume);
        }
    }

    void Gui::imGuiObject(Node &g) {
        ImGuiTreeNodeFlags flag = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnDoubleClick; //ImGuiTreeNodeFlags_OpenOnArrow;
        if (g.children.empty()) flag |= ImGuiTreeNodeFlags_Leaf;
        if (g.getId() == sceneObjectSelected) flag |= ImGuiTreeNodeFlags_Selected; 
        
        if (ImGui::TreeNodeEx(g.name.c_str(), flag)) {
            if (ImGui::IsItemClicked()) { // TODO Only works if item is open
                sceneObjectSelected = g.getId();
            }

            for (const std::shared_ptr<Node> &child : g.children) {
                imGuiObject(*child);
            }

            ImGui::TreePop();
        }
    }

    bool Gui::imGuiTransform(Transform &transform) {
        // TODO if (static) {} else {
        glm::vec3 p = transform.position;
        glm::vec3 r = transform.rotation;
        glm::vec3 s = transform.scale;
        glm::vec3 rd = glm::degrees(transform.rotation);

        ImGui::Text("Transform");
        ImGui::Indent(20.0f);
        ImGui::Text("Position:");
        ImGui::SameLine();
        ImGui::DragFloat3("##pos", &transform.position.x, 0.005f);

        ImGui::Text("Rotation:");
        ImGui::SameLine();
        ImGui::DragFloat3("##rot", &rd.x, 0.1f, 0.0f, 360.0f, "%.1f", ImGuiSliderFlags_WrapAround);

        ImGui::Text("Scale:   ");
        ImGui::SameLine();
        ImGui::DragFloat3("##sca", &transform.scale.x, 0.005f);

        transform.rotation = glm::radians(rd);
        bool transformChanged = false;
        if (p != transform.position || r != transform.rotation || s != transform.scale) {
            inputChanged = true;
            transformChanged = true;
        }

        ImGui::Indent(-20.0f);
        return transformChanged;
    }

    void Gui::imGuiMesh(const Mesh &mesh) {
        ImGui::Text("Mesh");
        ImGui::Indent(20.0f);

        ImGui::Text((mesh.name).c_str());
        ImGui::Text("%i vertices", mesh.vertexCount);
        if (mesh.hasIndexBuffer) {
            ImGui::Text("%i indices", mesh.indexCount);
        }
        ImGui::Text("%i triangles", mesh.hasIndexBuffer ? mesh.indexCount / 3 : mesh.vertexCount / 3);
        ImGui::Indent(-20.0f);

        std::vector<std::string> materials;
        for (const PrimMesh &s : mesh.primMeshes) {
            // avoid showing the same material multiple times
            if (std::find(materials.begin(), materials.end(), s.material->name) == materials.end()) {
                materials.push_back(s.material->name);
                imGuiSubMesh(s);
            }
        }
    }

    void Gui::imGuiSubMesh(const PrimMesh &submesh) { // TODO dropdown list of materials
        ImGui::Text("Material");
        ImGui::Indent(20.0f);
        ImGui::Text(submesh.material->name.c_str());

        static int itemSelected = 0;

        if (ImGui::BeginCombo("##materialcombo", scene->materials[submesh.material->index]->name.c_str(), 0)) {
            for (int n = 0; n < scene->materials.size(); n++) {
                const bool isSelected = (itemSelected == n);
                if (ImGui::Selectable(scene->materials[n]->name.c_str(), isSelected))
                    itemSelected = n;

                if (isSelected)
                    ImGui::SetItemDefaultFocus();

                // TODO currently material is indexed directly with its device address in the material buffer
                // have an intermediary material index buffer that points to the materials in the material buffer
            }
            ImGui::EndCombo();
        }

        imGuiMaterial(*submesh.material);
        ImGui::Indent(-20.0f);
    }

    void Gui::imGuiMedium(Medium &medium) {
        ImGuiSliderFlags silderFlags = ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_ClampOnInput | ImGuiSliderFlags_ClampZeroRange;
        ImGui::ColorEdit3("Albedo", (float*)&medium.albedo, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
        ImGui::SliderFloat("Absorption", (float*)&medium.absorption, 0.0f, 100.0f, "%.3f", ImGuiSliderFlags_None);
        ImGui::SliderFloat("Scattering", (float*)&medium.scattering, 0.0f, 100.0f, "%.3f", ImGuiSliderFlags_None);
        const char* phaseFunctions[] = { "Isotropic", "Rayleigh", "HenyeyGreenstein", "Mie Approximation" };
        const char* selectedPhaseFunction = phaseFunctions[medium.phaseFunction.type];

        // TODO create a function for phase functions and use it in both imGuiVolume() and imGuiMaterial()
        if (ImGui::BeginCombo("Phase Function", selectedPhaseFunction, 0)) {
            for (int n = 0; n < IM_ARRAYSIZE(phaseFunctions); n++) {
                const bool isSelected = (medium.phaseFunction.type == n);
                if (ImGui::Selectable(phaseFunctions[n], isSelected)) {
                    medium.phaseFunction.type = static_cast<PhaseFunctionType>(n);
                }

                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        if (medium.phaseFunction.type == PhaseFunctionType::HenyeyGreenstein) {
            ImGui::SliderFloat("Anisotropy g", (float*)&medium.phaseFunction.anisotropy, -0.999f, 0.999f, "%.3f", silderFlags);
        }
        if (medium.phaseFunction.type == PhaseFunctionType::MieApproximation) {
            ImGui::SliderFloat("Particle size", (float*)&medium.phaseFunction.particleSize, 0.0f, 50.0f, "%.3f", silderFlags);
        }
    }

    void Gui::imGuiMaterial(Material &material) {
        ImGuiSliderFlags silderFlags = ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_ClampOnInput | ImGuiSliderFlags_ClampZeroRange;

        MaterialConstants mc = material.data.constants;
        bool a = true;
        bool *a_ptr = &a;

        ImGui::PushID(material.name.c_str());
        ImGui::Indent(10.0f);
        if (ImGui::CollapsingHeader("Base", a_ptr)) {
            ImGui::Indent(10.0f);
            ImGui::ColorEdit4("Albedo", (float*)&material.data.constants.albedo, ImGuiColorEditFlags_Float);
            imGuiImage(*material.textures.albedo);
            ImGui::SliderFloat("Metallic", (float*)&material.data.constants.metallic, 0.0f, 1.0f, "%.3f", silderFlags);
            ImGui::SliderFloat("Roughness", (float*)&material.data.constants.roughness, 0.0f, 1.0f, "%.3f", silderFlags);
            imGuiImage(*material.textures.metallicRoughness);
            ImGui::ColorEdit4("Emission", (float*)&material.data.constants.emission, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
            ImGui::Indent(-10.0f);
        }
        if (ImGui::CollapsingHeader("Dielectric", a_ptr)) {
            ImGui::Indent(10.0f);
            ImGui::SliderFloat("Thickness", (float*)&material.data.constants.thickness, 0.0f, 1.0f, "%1.0f", silderFlags);
            ImGui::SliderFloat("Refraction Index", (float*)&material.data.constants.ior, 1.0f, 3.0f, "%.3f", silderFlags);
            imGuiMedium(material.data.constants.medium);
            ImGui::Indent(-10.0f);
        }
        ImGui::Indent(-10.0f);
        ImGui::PopID();


        if (mc != material.data.constants) {
            inputChanged = true;
            scene->materialDataBuffer->update(material.index * sizeof(MaterialData), sizeof(MaterialConstants), &material.data.constants);
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
        ImGui::Text("Camera");
        ImGui::Indent(20.0f);
        float fov = camera.fov;
        float fovDegrees = glm::degrees(fov);

        ImGui::Text("Field of View:");
        ImGui::SameLine();
        ImGui::DragFloat("##fov", &fovDegrees, 0.1f, glm::degrees(Camera::MIN_FOV), glm::degrees(Camera::MAX_FOV), "%.1f", ImGuiSliderFlags_ClampOnInput);

        camera.fov = glm::radians(fovDegrees);
        if (fov != camera.fov) {
            inputChanged = true;
        }
        ImGui::Indent(-20.0f);
    }

    void Gui::imGuiVolume(Volume &volume) {
        ImGuiSliderFlags silderFlags = ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_ClampOnInput | ImGuiSliderFlags_ClampZeroRange;

        Medium m = volume.medium;
        float temperature = volume.temperatureMultiplier;
        float emissiveness = volume.emissivenessMultiplier;
        float jittering = volume.jitteringAmount;

        ImGui::PushID("##volume");
        ImGui::Indent(20.0f);

        imGuiMedium(volume.medium);
        if (volume.hasTemperature()) {
            ImGui::DragFloat("Temperature multiplier", (float*)&volume.temperatureMultiplier, 0.01f, 0.0f, 10000.0f);
            ImGui::DragFloat("Emissiveness multiplier", (float*)&volume.emissivenessMultiplier, 0.01f, 0.0f, 1000.0f);
        }
        ImGui::DragFloat("Jittering amount", (float*)&volume.jitteringAmount, 0.01f, 0.0f, 10000.0f);


        ImGui::Indent(-20.0f);
        ImGui::PopID();

        if (m            != volume.medium                   ||
            temperature  != volume.temperatureMultiplier    ||
            emissiveness != volume.emissivenessMultiplier   ||
            jittering    != volume.jitteringAmount
        ) {
            inputChanged = true;
        }
    }

    void Gui::imGuiRender(const FrameInfo &frameInfo) {
        if (ImGui::BeginTabItem("Render")) {
            // Window size and resize
            VkExtent2D windowSize = window.getExtent();
            int inputWindowSize[2] = {static_cast<int>(windowSize.width), static_cast<int>(windowSize.height)};

            ImGui::InputInt2("Screen resolution", inputWindowSize);

            if (inputWindowSize[0] != windowSize.width || inputWindowSize[1] != windowSize.height) {
                if (!ImGui::IsItemActive())
                window.resizeWindow(inputWindowSize[0], inputWindowSize[1]);
            }

            // Path tracing parameters
            int oldMaxDepth = system.maxDepth;
            ImGui::SliderInt("Depth", &system.maxDepth, 1, 100, "%i", ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_ClampOnInput | ImGuiSliderFlags_ClampZeroRange);
            if (oldMaxDepth != system.maxDepth) inputChanged = true; 

            ImGui::SliderInt("Samples per pixel", &system.samplesPerPixel, 1, 20, "%i", ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_ClampOnInput | ImGuiSliderFlags_ClampZeroRange);

            bool oldFrameAccumulation = system.frameAccumulation;
            ImGui::Checkbox("Frame Accumulation", &system.frameAccumulation);
            if (oldFrameAccumulation != system.frameAccumulation) inputChanged = true;
            
            bool oldRussianRoulette = system.russianRoulette;
            ImGui::Checkbox("Russian Roulette", &system.russianRoulette);
            if (oldRussianRoulette != system.russianRoulette) inputChanged = true;

            bool oldNextEventEstimation = system.nextEventEstimation;
            ImGui::Checkbox("Next Event Estimation", &system.nextEventEstimation);
            if (oldNextEventEstimation != system.nextEventEstimation) inputChanged = true;

            // Tonemapping
            ImGui::DragFloat("Exposure", &system.exposure, 0.01f, 0.0f, 10000.0f, "%.2f", ImGuiSliderFlags_ClampOnInput);
            const char* tonemappers[] = { "None", "ACES", "AgX" };
            const char* selectedTonemapper = tonemappers[system.tonemapper];
    
            if (ImGui::BeginCombo("Tonemapper", selectedTonemapper, 0)) {
                for (int n = 0; n < IM_ARRAYSIZE(tonemappers); n++) {
                    const bool isSelected = (system.tonemapper == n);
                    if (ImGui::Selectable(tonemappers[n], isSelected)) {
                        system.tonemapper = n;
                    }
    
                    if (isSelected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            // Camera
            std::vector<char*> cameraNames;
            for (const auto& camera : scene->cameraObjects) {
                cameraNames.push_back(&camera->name[0]);
            }
    
            const char* selectedCamera = scene->currentCamera->name.c_str();
            if (ImGui::BeginCombo("Camera", selectedCamera, 0)) {
                for (int n = 0; n < cameraNames.size(); n++) {
                    const bool isSelected = (scene->currentCamera->name == cameraNames[n]);
                    if (ImGui::Selectable(cameraNames[n], isSelected)) {
                        for (const auto& camera : scene->cameraObjects) {
                            if (camera->name == cameraNames[n]) {
                                scene->currentCamera = camera;
                                inputChanged = true;
                                break;
                            }
                        }
                    }
    
                    if (isSelected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            // Environments
            std::vector<char*> environmentNames;
            for (const auto& environment : scene->lightObjects) {
                environmentNames.push_back(&environment->name[0]);
            }

            const int offset = static_cast<int>(scene->images.size());
            const char* selectedEnvironment = scene->lightObjects.at(scene->environmentID - offset)->name.c_str();
            if (ImGui::BeginCombo("Environment", selectedEnvironment, 0)) {
                for (int n = 0; n < scene->lightObjects.size(); n++) {
                    const bool isSelected = (scene->environmentID - offset == n);
                    if (ImGui::Selectable(environmentNames[n], isSelected)) {
                        for (const auto& environment : scene->lightObjects) {
                            if (environment->name == environmentNames[n]) {
                                scene->environmentID = n + offset;
                                inputChanged = true;
                                break;
                            }
                        }
                    }
    
                    if (isSelected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }

            ImGui::Text("Rotation:");
            ImGui::SameLine();
            glm::vec2 oldEnvRotation = system.environmentRotation;
            glm::vec2 newEnvRotation = glm::degrees(system.environmentRotation);
            ImGui::DragFloat2("##envrot", &newEnvRotation.x, 0.1f, 0.0f, 360.0f, "%.1f", ImGuiSliderFlags_WrapAround);
            system.environmentRotation = glm::radians(newEnvRotation);
            if (system.environmentRotation != oldEnvRotation) inputChanged = true;
            
            if (ImGui::Button("Save render")) {
                saveImage(*system.presentImage);
            }

            ImGui::EndTabItem();
        }
    }

    void Gui::saveImage(const Image& image) {
        const int width  = static_cast<int>(image.size.width);
        const int height = static_cast<int>(image.size.height);
        const int comp   = 4;

        Buffer imageBuffer {
            device,
            sizeof(float) * comp * width * height, 
            1, 
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
        };

        device.copyImageToBuffer(image.handle, image.size, image.layout, imageBuffer.handle());

        saveImageFromData((void*)imageBuffer.getMappedMemory(), width, height, comp);
    }

    void Gui::saveImageFromData(const void* data, const int width, const int height, const int comp, bool hdr) {
        auto t = std::time(nullptr);
        std::tm tm{};
        localtime_s(&tm, &t);

        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y-%m-%d %H-%M-%S");
        std::string filename = oss.str();
        
        if (hdr) {
            stbi_write_hdr((filename + ".hdr").c_str(), width, height, comp, (float*)data);
        }
        else {
            stbi_write_bmp((filename + ".bmp").c_str(), width, height, comp, (void*)data);
            //stbi_write_png((filename + ".png").c_str(), width, height, comp, (void*)data, width * comp);
        }
    }
}