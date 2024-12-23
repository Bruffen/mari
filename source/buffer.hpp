#pragma once

#include "device.hpp"
#include "vk_helper.hpp"

namespace mari {
        // Holds data for a scratch buffer used as a temporary storage during acceleration structure builds
        // TODO make scratch buffers a funcionality of buffer class
    struct ScratchBuffer {
        uint64_t       deviceAddress;
        VkBuffer       handle;
        VkDeviceMemory memory;

        static ScratchBuffer createScratchBuffer(Device &device, VkDeviceSize size) {
            ScratchBuffer scratchBuffer{};

            VkBufferCreateInfo bufferCreateInfo = {};
            bufferCreateInfo.sType              = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferCreateInfo.size               = size;
            bufferCreateInfo.usage              = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
            vkCreateBuffer(device.handle(), &bufferCreateInfo, nullptr, &scratchBuffer.handle);

            VkMemoryRequirements memoryRequirements = {};
            vkGetBufferMemoryRequirements(device.handle(), scratchBuffer.handle, &memoryRequirements);

            VkMemoryAllocateFlagsInfo memoryAllocateFlagsInfo = {};
            memoryAllocateFlagsInfo.sType                     = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
            memoryAllocateFlagsInfo.flags                     = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR;

            VkMemoryAllocateInfo memoryAllocateInfo = {};
            memoryAllocateInfo.sType                = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            memoryAllocateInfo.pNext                = &memoryAllocateFlagsInfo;
            memoryAllocateInfo.allocationSize       = memoryRequirements.size;
            memoryAllocateInfo.memoryTypeIndex      = device.findMemoryType(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            vkAllocateMemory(device.handle(), &memoryAllocateInfo, nullptr, &scratchBuffer.memory);
            vkBindBufferMemory(device.handle(), scratchBuffer.handle, scratchBuffer.memory, 0);

            VkBufferDeviceAddressInfoKHR bufferDeviceAddressInfo{};
            bufferDeviceAddressInfo.sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
            bufferDeviceAddressInfo.buffer = scratchBuffer.handle;
            scratchBuffer.deviceAddress    = vkGetBufferDeviceAddressKHR(device.handle(), &bufferDeviceAddressInfo);

            return scratchBuffer;
        }

        static void deleteScratchBuffer(Device &device, ScratchBuffer &scratchBuffer) {
            if (scratchBuffer.memory != VK_NULL_HANDLE) {
                vkFreeMemory(device.handle(), scratchBuffer.memory, nullptr);
            }
            if (scratchBuffer.handle != VK_NULL_HANDLE) {
                vkDestroyBuffer(device.handle(), scratchBuffer.handle, nullptr);
            }
        }
    };
     
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
        VkResult                flush(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
        VkDescriptorBufferInfo  descriptorInfo(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
        VkResult                invalidate(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
        
        void                    writeToIndex(void* data, int index);
        VkResult                flushIndex(int index);
        VkDescriptorBufferInfo  descriptorInfoForIndex(int index);
        VkResult                invalidateIndex(int index);
        void*                   getMappedMemory();
        
        VkBuffer                handle()                    const { return buffer; }
        VkDeviceMemory          deviceMemory()              const { return memory; }
        uint32_t                getInstanceCount()          const { return instanceCount; }
        VkDeviceSize            getInstanceSize()           const { return instanceSize; }
        VkDeviceSize            getAlignmentSize()          const { return instanceSize; }
        VkBufferUsageFlags      getUsageFlags()             const { return usageFlags; }
        VkMemoryPropertyFlags   getMemoryPropertyFlags()    const { return memoryPropertyFlags; }
        VkDeviceSize            getBufferSize()             const { return bufferSize; }
    
        uint64_t                deviceAddress();

    private:
        static VkDeviceSize     getAlignment(VkDeviceSize instanceSize, VkDeviceSize minOffsetAlignment);
        
        Device&                 device;
        void*                   mapped = nullptr;
        VkBuffer                buffer = VK_NULL_HANDLE;
        VkDeviceMemory          memory = VK_NULL_HANDLE;
        
        VkDeviceSize            bufferSize;
        uint32_t                instanceCount;
        VkDeviceSize            instanceSize;
        VkDeviceSize            alignmentSize;
        VkBufferUsageFlags      usageFlags;
        VkMemoryPropertyFlags   memoryPropertyFlags;
    };
}