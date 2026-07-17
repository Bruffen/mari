#pragma once

#include "volume.hpp"
#define NANOVDB_USE_OPENVDB
#include <nanovdb/tools/CreateNanoGrid.h>

namespace mari {
    bool isOpenvdbInitialized = false; // TODO static member

    Volume::Volume(Device &device, std::string filepath) {
        medium.absorption = 0.0f;
        medium.scattering = 1.0f;
        medium.heterogeneous = true;

        if (!isOpenvdbInitialized) {
            openvdb::initialize(); 
            isOpenvdbInitialized = true;
        } 
        openvdb::io::File file(filepath);
        file.open();
        openvdb::GridPtrVecPtr gridsPtr = file.getGrids();
        
        //for (const openvdb::GridBase::Ptr grid : *gridsPtr.get()) {
        //    grid->print();
        //}
        
        density = file.readGrid("density");
        if (file.hasGrid("temperature")) {
            temperature = file.readGrid("temperature");
        }

        file.close();
        
        const openvdb::FloatGrid::Ptr densityGridData = openvdb::gridPtrCast<openvdb::FloatGrid>(density);

        // Find the highest density value
        auto densityIter = densityGridData->beginValueAll();
        float maxDensity = 0.0f;
        do {
            float current = densityIter.getValue();
            if (current > maxDensity)
                maxDensity = current;
        } while (densityIter.next());

        // Adjust sigma values to correspond to density range
        medium.absorption *= maxDensity;
        medium.scattering *= maxDensity;

        // Normalize the density if needed
        if (maxDensity != 1.0f && maxDensity > 0.0f) {
            densityIter = densityGridData->beginValueAll();
            do {
                densityIter.setValue(densityIter.getValue() / maxDensity);
            } while (densityIter.next());
        }

        nanovdb::GridHandle<nanovdb::HostBuffer> densityGridHandle = nanovdb::tools::createNanoGrid(*densityGridData);

        uint64_t byteSize = densityGridHandle.bufferSize();
        uint64_t elementStride = sizeof(float); // because it's a FloatGrid
        uint64_t elementCount = byteSize / elementStride; 

        nanoDensityBuffer = std::make_unique<Buffer>(
            device, 
            elementStride, 
            static_cast<uint32_t>(elementCount), 
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, 
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );

        nanoDensityBuffer->stageToBuffer(densityGridHandle.buffer().data());

        if (temperature) {
            const openvdb::FloatGrid::Ptr temperatureGridData = openvdb::gridPtrCast<openvdb::FloatGrid>(temperature);
            nanovdb::GridHandle<nanovdb::HostBuffer> temperatureGridHandle = nanovdb::tools::createNanoGrid(*temperatureGridData);
            
            byteSize = temperatureGridHandle.bufferSize();
            elementStride = sizeof(float);
            elementCount = byteSize / elementStride; 
            
            nanoTemperatureBuffer = std::make_unique<Buffer>(
                device, 
                elementStride, 
                static_cast<uint32_t>(elementCount), 
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, 
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
            );

            nanoTemperatureBuffer->stageToBuffer(temperatureGridHandle.buffer().data());
        }
    }

    // This converts from openvdb to nanovdb, creates a staging buffer 
    // and copies the whole nanovdb buffer to the device!!!
    // Very inefficient!!!
    // TODO Store the nanovdb grid instead, figure out how to modify its transform
    // and also where and how to copy only the transform to the device
    void Volume::setTransform(const Transform &transform) {
        auto vdbMat4 = openvdb::v12_1::math::Mat4d();
        vdbMat4.setIdentity();

        //vdbMat4.setToRotation();
        vdbMat4.postScale(openvdb::v12_1::math::Vec3d(transform.scale.x, -transform.scale.y, transform.scale.z));
        vdbMat4.setTranslation(openvdb::v12_1::math::Vec3d(transform.position.x, transform.position.y, transform.position.z));

        density->setTransform(openvdb::v12_1::math::Transform::createLinearTransform(vdbMat4));
        const openvdb::FloatGrid::Ptr densityGridData = openvdb::gridPtrCast<openvdb::FloatGrid>(density);
        nanovdb::GridHandle<nanovdb::HostBuffer> densityGridHandle = nanovdb::tools::createNanoGrid(*densityGridData);
        nanoDensityBuffer->stageToBuffer(densityGridHandle.buffer().data());

        if (temperature) {
            temperature->setTransform(openvdb::v12_1::math::Transform::createLinearTransform(vdbMat4));
            const openvdb::FloatGrid::Ptr temperatureGridData = openvdb::gridPtrCast<openvdb::FloatGrid>(temperature);
            nanovdb::GridHandle<nanovdb::HostBuffer> temperatureGridHandle = nanovdb::tools::createNanoGrid(*temperatureGridData);
            nanoTemperatureBuffer->stageToBuffer(temperatureGridHandle.buffer().data());
        }
    }
    
    Volume::~Volume() {
        if (isOpenvdbInitialized) {
            openvdb::uninitialize();
            isOpenvdbInitialized = false;
        }
    }
}