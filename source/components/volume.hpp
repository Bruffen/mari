#pragma once

#include "image.hpp"
#include "components/transform.hpp"

#include <openvdb/openvdb.h>
#include <memory>

namespace mari {

    enum PhaseFunction {
        Isotropic = 0,
        HenyeyGreenstein = 1,
        Draine = 2
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
            float g = 0.0f;
            float a = 0.0f;
            float sigma_a = 0.0f;
            float sigma_s = 1.0f;
            float temperature_multiplier = 1.0f;
            float emissiveness_multiplier = 1.0f;
            float jittering_amount = 0.4f;
            // TODO maybe create a struct shared by volume and material for participating media
        private:
            openvdb::GridBase::Ptr  density;
            openvdb::GridBase::Ptr  temperature;
            std::unique_ptr<Buffer> nanoDensityBuffer;
            std::unique_ptr<Buffer> nanoTemperatureBuffer;
    };
}