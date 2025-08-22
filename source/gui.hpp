#pragma once

#include "default_objects.hpp"
#include "frame_info.hpp"
#include "scene/scene.hpp"
#include "renderer.hpp"
#include "systems/ray_tracing_system.hpp"

#include <imgui.h>

namespace mari {
    class Gui {
        public:
            Gui(Device &device, Window &window, Renderer &renderer, RayTracingSystem &system);
            ~Gui();

            ImGuiIO* getIO() { return io; }

            void set(std::shared_ptr<Scene> scene);
            void prepare(FrameInfo &frameInfo);
            void render(VkCommandBuffer commandBuffer);
            void saveImage(const Image& image);
            void saveImageFromData(const void* data, const int width, const int height, const int comp, bool hdr = false);

            bool inputChanged = false;
            bool isActive = true;
        private:
            static void checkVkResult(VkResult err);
            void imGuiFramerate(float duration);
            void imGuiInspector(Node &g);
            void imGuiObject(Node &g);
            void imGuiTransform(Transform &transform);
            void imGuiMesh(const Mesh &mesh);
            void imGuiSubMesh(const PrimMesh &submesh);
            void imGuiMaterial(const Material &material);
            void imGuiImage(const Image &image);
            void imGuiCamera(Camera &camera);
            void imGuiRender(const FrameInfo &frameInfo);

            Device  &device;
            Window  &window;
            RayTracingSystem &system;
            ImGuiIO *io;
            std::shared_ptr<Scene> scene;

            uint32_t triangleCount = 0;

            int sceneObjectSelected = -1;

            VkDescriptorPool descriptorPool;
    };
}