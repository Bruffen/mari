#pragma once

#include "window.hpp"
#include "device.hpp"
#include "game_object.hpp"
#include "renderer.hpp"
#include "descriptors.hpp"

#include <memory>
#include <vector>

namespace mari {
    class Mari {
        public:
            static constexpr int WIDTH = 800;
            static constexpr int HEIGHT = 600;

            Mari();
            ~Mari();
            Mari(const Mari &) = delete;
            Mari &operator=(const Mari &) = delete;

            void run();
        private:
            void loadGameObjects();

            Window window{WIDTH, HEIGHT, "Mari"};
            Device device{window};
            Renderer renderer{window, device};

            std::unique_ptr<DescriptorPool> globalPool{};
            std::vector<GameObject> gameObjects;
    };
}