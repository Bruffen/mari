#pragma once

#include "image.hpp"

#include <memory>

namespace mari {
    class Volume {
        public:
            Volume(Device &device);

        private:
            std::unique_ptr<Image> image;
            glm::vec3 boundsMin;
            glm::vec3 boundsMax;
    };
}