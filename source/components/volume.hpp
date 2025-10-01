#pragma once

#include "image.hpp"

#include <memory>

namespace mari {
    class Volume {
        public:
            Volume(Device &device, std::string filepath);

            uint64_t getDeviceAddress() const { return nanoBuffer->deviceAddress(); }

            float g = 0.0;
            float sigma_a = 0.0;
            float sigma_s = 1.0;
            // TODO maybe create a struct shared by volume and material for participating media
        private:
            std::unique_ptr<Buffer> nanoBuffer;
    };
}