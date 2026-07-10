#pragma once

#include "device.hpp"
#include "buffer.hpp"
#include "components/transform.hpp"
#include "scene/participating_media.hpp"

#include <openvdb/openvdb.h>
#include <memory>

namespace mari {

    class Volume {
        public:
            Volume(Device &device, std::string filepath);
            ~Volume();

            uint64_t getDensityDeviceAddress()      const { return nanoDensityBuffer->deviceAddress(); }
            uint64_t getTemperatureDeviceAddress()  const { return nanoTemperatureBuffer ? nanoTemperatureBuffer->deviceAddress() : 0; }
            bool     hasTemperature()               const { return temperature ? true : false; }

            void setTransform(const Transform &transform);

            Medium medium;
            float temperatureMultiplier     = 1.0f;
            float emissivenessMultiplier    = 1.0f;
            float jitteringAmount           = 0.01f;
        private:
            openvdb::GridBase::Ptr  density;
            openvdb::GridBase::Ptr  temperature;
            std::unique_ptr<Buffer> nanoDensityBuffer;
            std::unique_ptr<Buffer> nanoTemperatureBuffer;
    };
}