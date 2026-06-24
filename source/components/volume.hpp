#pragma once

#include "image.hpp"
#include "components/transform.hpp"

#include <openvdb/openvdb.h>
#include <memory>

namespace mari {

    enum PhaseFunction {
        Isotropic = 0,
        Rayleigh = 1,
        HenyeyGreenstein = 2,
        MieApproximation = 3
    };

    class Volume {
        public:
            Volume(Device &device, std::string filepath);
            ~Volume();

            uint64_t getDensityDeviceAddress()      const { return nanoDensityBuffer->deviceAddress(); }
            uint64_t getTemperatureDeviceAddress()  const { return nanoTemperatureBuffer ? nanoTemperatureBuffer->deviceAddress() : 0; }
            bool     hasTemperature()               const { return temperature ? true : false; }

            void setTransform(const Transform &transform);

            glm::vec4 albedo = glm::vec4(1.0f);
            PhaseFunction phaseFunction = PhaseFunction::HenyeyGreenstein;
            float anisotropy_g = 0.0f;
            float particleSize = 0.1f; // in micrometers with range [0, 50]
            float sigma_a = 0.0f;
            float sigma_s = 1.0f;
            float temperatureMultiplier = 1.0f;
            float emissivenessMultiplier = 1.0f;
            float jitteringAmount = 0.01f;
            // TODO maybe create a struct shared by volume and material for participating media
        private:
            openvdb::GridBase::Ptr  density;
            openvdb::GridBase::Ptr  temperature;
            std::unique_ptr<Buffer> nanoDensityBuffer;
            std::unique_ptr<Buffer> nanoTemperatureBuffer;
    };
}