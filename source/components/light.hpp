#pragma once

#include <glm/glm.hpp>

namespace mari {

    enum LightType {
        POINT,
        DIRECTIONAL,
        AREA,
        MESH
    };

    class Light {
        public:
            Light();

        private:
            glm::vec3 color;
    };
}