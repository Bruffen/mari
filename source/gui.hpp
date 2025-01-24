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

            ImGuiIO& getIO() { return imGui; }

            void set(std::shared_ptr<Scene> scene);
            void prepare(FrameInfo &frameInfo);
            void render(VkCommandBuffer commandBuffer);
        private:
            static void checkVkResult(VkResult err);
            void imGuiObject(const GameObject &g);
            void imGuiTransform(const Transform &transform);
            void imGuiMesh(const Mesh &mesh);
            void imGuiSubMesh(const SubMesh &submesh);
            void imGuiMaterial(const Material &material);
            void imGuiImage(const Image &image);

            Device  &device;
            Window  &window;
            ImGuiIO imGui;
            std::shared_ptr<Scene> scene;

            VkDescriptorPool descriptorPool;
    };
}