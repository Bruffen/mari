#pragma once

#include "device.hpp"
#include "vk_helper.hpp"

#include <cassert>

namespace mari {
    class Buffer {
        public:
            Buffer(
                Device& device,
                VkDeviceSize instanceSize,
                uint32_t instanceCount,
                VkBufferUsageFlags usageFlags,
                VkMemoryPropertyFlags memoryPropertyFlags,
                VkDeviceSize minOffsetAlignment = 1);
            ~Buffer();
            
            Buffer(const Buffer&) = delete;
            Buffer& operator=(const Buffer&) = delete;

            
            VkResult                map(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
            void                    unmap();
            
            void                    writeToBuffer(void* data, VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
            void                    stageToBuffer(void* data);
            VkResult                flush(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
            void                    update(VkDeviceSize offset, VkDeviceSize size, const void* data);
            VkDescriptorBufferInfo  descriptorInfo(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0) const;
            VkResult                invalidate(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
            
            void                    writeToIndex(void* data, int index);
            VkResult                flushIndex(int index);
            VkDescriptorBufferInfo  descriptorInfoForIndex(int index);
            VkResult                invalidateIndex(int index);
            void*                   getMappedMemory();
            
            VkBuffer                handle()                    const { return buffer; }
            VkDeviceMemory          deviceMemory()              const { return memory; }
            uint64_t                deviceAddress()             const { 
                assert(usageFlags & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT && "Buffer needs VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT flag"); 
                return address; 
            }
            uint32_t                getInstanceCount()          const { return instanceCount; }
            VkDeviceSize            getInstanceSize()           const { return instanceSize; }
            VkDeviceSize            getAlignmentSize()          const { return instanceSize; }
            VkBufferUsageFlags      getUsageFlags()             const { return usageFlags; }
            VkMemoryPropertyFlags   getMemoryPropertyFlags()    const { return memoryPropertyFlags; }
            VkDeviceSize            getBufferSize()             const { return bufferSize; }

        private:
            static VkDeviceSize     getAlignment(VkDeviceSize instanceSize, VkDeviceSize minOffsetAlignment);
            uint64_t                getBufferDeviceAddress();

            
            Device&                 device;
            void*                   mapped = nullptr;
            VkBuffer                buffer = VK_NULL_HANDLE;
            VkDeviceMemory          memory = VK_NULL_HANDLE;
            uint64_t                address;
            
            VkDeviceSize            bufferSize;
            uint32_t                instanceCount;
            VkDeviceSize            instanceSize;
            VkDeviceSize            alignmentSize;
            VkBufferUsageFlags      usageFlags;
            VkMemoryPropertyFlags   memoryPropertyFlags;
    };
}