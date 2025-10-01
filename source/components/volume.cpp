#pragma once

#include "volume.hpp"
#define NANOVDB_USE_OPENVDB
#include <openvdb/openvdb.h>
#include <nanovdb/tools/CreateNanoGrid.h>

namespace mari {
    Volume::Volume(Device &device, std::string filepath) {
        openvdb::initialize(); // TODO initialize and unitialize globally
        openvdb::io::File file(filepath);
        file.open();
        openvdb::GridPtrVecPtr gridsPtr = file.getGrids();
        
        for (const openvdb::GridBase::Ptr grid : *gridsPtr.get()) {
            grid->print();
        }
        
        const openvdb::GridBase::Ptr grid = file.readGrid("density");
        file.close();
        
        const openvdb::FloatGrid::Ptr gridData = openvdb::gridPtrCast<openvdb::FloatGrid>(grid);
        nanovdb::GridHandle<nanovdb::HostBuffer> gridHandle = nanovdb::tools::createNanoGrid(*gridData);

        uint64_t byteSize = gridHandle.bufferSize();
        uint64_t elementStride = sizeof(float); // because it's a FloatGrid
        uint64_t elementCount = byteSize / elementStride; 

        nanoBuffer = std::make_unique<Buffer>(
            device, 
            elementStride, 
            static_cast<uint32_t>(elementCount), 
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, 
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );

        nanoBuffer->stageToBuffer(gridHandle.buffer().data());
        openvdb::uninitialize();
    }
}