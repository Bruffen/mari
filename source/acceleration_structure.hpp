#pragma once

#include "device.hpp"
#include "buffer.hpp"
#include "scene/node.hpp"

#include <memory>

namespace mari {
    class AccelerationStructure {
        public:
            AccelerationStructure(Device& device, VkAccelerationStructureTypeKHR type);
            ~AccelerationStructure();

            VkAccelerationStructureKHR                      handle;
            uint64_t                                        deviceAddress;
            uint32_t                                        geometryCount = 0;
            std::shared_ptr<Node>                           node; // Think this won't work with instancing since multiple nodes can have the same blas

            void                                            build(const VkAccelerationStructureGeometryKHR *pGeometries, 
                                                                  const uint32_t geometryCount, 
                                                                  const uint32_t* pMaxPrimitiveCounts,
                                                                  VkAccelerationStructureBuildRangeInfoKHR** ppBuildRangeInfos);
            void                                            update(const VkAccelerationStructureGeometryKHR *pGeometries,
                                                                   VkAccelerationStructureBuildRangeInfoKHR** ppBuildRangeInfos);
            VkWriteDescriptorSetAccelerationStructureKHR    descriptor();
        private:
            Device&                                         device;
            VkAccelerationStructureTypeKHR                  type;
            VkAccelerationStructureBuildGeometryInfoKHR     buildGeometryInfo{};
            std::unique_ptr<Buffer>                         accelerationStructureBuffer;
            std::unique_ptr<Buffer>                         accelerationStructureScratchBuffer;
    };
}