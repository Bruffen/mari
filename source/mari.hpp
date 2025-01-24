#pragma once

#include "window.hpp"
#include "device.hpp"
#include "game_object.hpp"
#include "renderer.hpp"
#include "default_objects.hpp"
#include "frame_info.hpp"
#include "descriptors.hpp"
#include "scene/scene.hpp"
#include "gui.hpp"

#include <memory>
#include <vector>

namespace mari {
    class Mari {
        public:
            static constexpr int WIDTH = 1280;
            static constexpr int HEIGHT = 720;

            Mari();
            ~Mari();
            Mari(const Mari &) = delete;
            Mari &operator=(const Mari &) = delete;

            void run();
        private:
            void loadGameObjects();

            Window          window{WIDTH, HEIGHT, "Mari"};
            Device          device{window};
            Renderer        renderer{window, device};

            std::shared_ptr<Scene> scene; // TODO
            GameObject::Map gameObjects;
            std::unique_ptr<DescriptorPool> globalPool;
            std::unique_ptr<Gui> gui;
    };
}