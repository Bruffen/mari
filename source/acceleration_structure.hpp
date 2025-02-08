#pragma once

#include "device.hpp"
#include "buffer.hpp"

#include <memory>

namespace mari {
    class AccelerationStructure {
        public:
            AccelerationStructure(Device& device, VkAccelerationStructureTypeKHR type);
            ~AccelerationStructure();

            VkAccelerationStructureKHR                      handle;
            uint64_t                                        deviceAddress;
            std::unique_ptr<Buffer>                         buffer;
            uint32_t                                        geometryCount = 0;
            VkTransformMatrixKHR                            transformMatrix{1.0};

            void                                            build(const VkAccelerationStructureGeometryKHR *pGeometries, 
                                                                  const uint32_t geometryCount, 
                                                                  const uint32_t* pMaxPrimitiveCounts,
                                                                  VkAccelerationStructureBuildRangeInfoKHR** ppBuildRangeInfos);
            VkWriteDescriptorSetAccelerationStructureKHR    descriptor();
        private:
            Device&                                         device;
            VkAccelerationStructureTypeKHR                  type;
    };
}