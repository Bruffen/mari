#pragma once

#include <rgb2spec/rgb2spec.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/euler_angles.hpp>

namespace mari {
    class Spectral {
        public:
            static Spectral& instance();
            static bool isOn() { return isSpectral; }

            glm::vec3 rgbToSpectral(glm::vec3 rgb);
        private:
            Spectral();
            ~Spectral();

            static const bool isSpectral = false;
            RGB2Spec* rgb2Spec = nullptr;
    };
}