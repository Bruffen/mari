#pragma once

#include "window.hpp"

namespace mari {
    class Renderer {
        public:
            static constexpr int WIDTH = 1920;
            static constexpr int HEIGHT = 1080;

            void run();
        private:
            Window window{WIDTH, HEIGHT, "Mari"};
    };
}