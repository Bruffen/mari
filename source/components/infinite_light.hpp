#pragma once

#include "light.hpp"
#include "../image.hpp"
#include "../sampling.hpp"

#include <memory>

namespace mari {
    class InfiniteAreaLight : public Light {
        public:
            InfiniteAreaLight(Device &device, std::shared_ptr<Image> image, uint32_t textureSize = 4096);
            
            std::shared_ptr<Image> equalAreaImage;
            PiecewiseConstant2D sampler;
            uint32_t textureSize;
        private:
            void equalAreaTransformation(std::shared_ptr<Image> image);
    };
}