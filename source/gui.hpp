#pragma once

#include "default_objects.hpp"
#include "frame_info.hpp"
#include "scene/scene.hpp"
#include "renderer.hpp"

#include <imgui.h>

namespace mari {
    class Gui {
        public:
            Gui(Device &device, Window &window, Renderer &renderer);
            ~Gui();

            ImGuiIO* getIO() { return io; }

            void set(std::shared_ptr<Scene> scene);
            void prepare(FrameInfo &frameInfo);
            void render(VkCommandBuffer commandBuffer);

            bool inputChanged = false;
        private:
            static void checkVkResult(VkResult err);
            void imGuiFramerate(float duration);
            void imGuiObject(GameObject &g);
            void imGuiTransform(Transform &transform);
            void imGuiMesh(const Mesh &mesh);
            void imGuiSubMesh(const PrimMesh &submesh);
            void imGuiMaterial(const Material &material);
            void imGuiImage(const Image &image);
            void imGuiCamera(Camera &camera);

            Device  &device;
            Window  &window;
            ImGuiIO *io;
            std::shared_ptr<Scene> scene;

            uint32_t triangleCount = 0;

            VkDescriptorPool descriptorPool;
    };
}