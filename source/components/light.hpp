#pragma once

#include "../image.hpp"
#include "../sampling.hpp"

#include <glm/glm.hpp>
#include <memory>

namespace mari {

    enum LightType {
        POINT,
        DIRECTIONAL,
        AREA,
        MESH
    };

    struct LightUbo {
        int environmentID;
        float marginalIntegral;
        uint64_t marginalFunctionBufferAddress;
        uint64_t marginalCdfBufferAddress;
        uint64_t conditionalIntegralBufferAddress;
        uint64_t conditionalFunctionBufferAddress;
        uint64_t conditionalCdfBufferAddress;
    }; // TODO is it possible to fit all of this data contiguously within a single object and then pass a buffer device address of that single object with all we need?

    class Light {
        public:
            Light(Device &device);

            virtual ~Light() {};

        protected:
            Device &device;
            glm::vec3 color;
    };

    class InfiniteAreaLight : public Light {
        public:
            InfiniteAreaLight(Device &device, std::shared_ptr<Image> image);
        
            std::shared_ptr<Image> equalAreaImage;
            std::shared_ptr<Buffer> imageBuffer;
            PiecewiseConstant2D sampler;
        private:
            static const uint32_t textureSize = 4096;
            void equalAreaTransformation(std::shared_ptr<Image> image);
    };
}