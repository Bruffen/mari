#pragma once

#include "window.hpp"
#include "device.hpp"
#include "game_object.hpp"
#include "renderer.hpp"
#include "default_objects.hpp"
#include "descriptors.hpp"
#include "scene/scene.hpp"

#include <memory>
#include <vector>

namespace mari {
    class Mari {
        public:
            static constexpr int WIDTH = 800;
            static constexpr int HEIGHT = 600;

            Mari();
            Mari(const Mari &) = delete;
            Mari &operator=(const Mari &) = delete;

            void run();
            
            const DefaultObjects &getDefaultObjects() const { return defaultObjects; }
        private:
            void loadGameObjects();

            Window          window{WIDTH, HEIGHT, "Mari"};
            Device          device{window};
            Renderer        renderer{window, device};
            DefaultObjects  defaultObjects{device};

            std::shared_ptr<Scene> scene; // TODO
            GameObject::Map gameObjects;
            std::unique_ptr<DescriptorPool> globalPool{};
    };
}