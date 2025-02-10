/*
 * Encapsulates a vulkan buffer
 *
 * Initially based off VulkanBuffer by Sascha Willems -
 * https://github.com/SaschaWillems/Vulkan/blob/master/base/VulkanBuffer.h
 */
 
#include "buffer.hpp"
#include "vk_helper.hpp"
 
// std
#include <cstring>
 
namespace mari {

    Buffer::Buffer(
            Device &device,
            VkDeviceSize instanceSize,
            uint32_t instanceCount,
            VkBufferUsageFlags usageFlags,
            VkMemoryPropertyFlags memoryPropertyFlags,
            VkDeviceSize minOffsetAlignment)
            : device{device},
            instanceSize{instanceSize},
            instanceCount{instanceCount},
            usageFlags{usageFlags},
            memoryPropertyFlags{memoryPropertyFlags} 
    {
        alignmentSize = getAlignment(instanceSize, minOffsetAlignment);
        bufferSize = alignmentSize * instanceCount;
        device.createBuffer(bufferSize, usageFlags, memoryPropertyFlags, buffer, memory);
        if (usageFlags & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
            address = getBufferDeviceAddress();
        }
    }
 
    Buffer::~Buffer() {
        unmap();
        vkDestroyBuffer(device.handle(), buffer, nullptr);
        vkFreeMemory(device.handle(), memory, nullptr);
    }

    void* Buffer::getMappedMemory() {
        if (!mapped) {
            map();
        }

        assert(mapped && "Memory is not mapped");
        return mapped;
    }
 
    /**
     * Map a memory range of this buffer. If successful, mapped points to the specified buffer range.
     *
     * @param size (Optional) Size of the memory range to map. Pass VK_WHOLE_SIZE to map the complete
     * buffer range.
     * @param offset (Optional) Byte offset from beginning
     *
     * @return VkResult of the buffer mapping call
     */
    VkResult Buffer::map(VkDeviceSize size, VkDeviceSize offset) {
        assert(buffer && memory && "Called map on buffer before create");
        return vkMapMemory(device.handle(), memory, offset, size, 0, &mapped);
    }
    
    /**
     * Unmap a mapped memory range
     *
     * @note Does not return a result as vkUnmapMemory can't fail
     */
    void Buffer::unmap() {
        if (mapped) {
            vkUnmapMemory(device.handle(), memory);
            mapped = nullptr;
        }
    }
 
    /**
     * Copies the specified data to the mapped buffer. Default value writes whole buffer range
     *
     * @param data Pointer to the data to copy
     * @param size (Optional) Size of the data to copy. Pass VK_WHOLE_SIZE to flush the complete buffer
     * range.
     * @param offset (Optional) Byte offset from beginning of mapped region
     *
     */
    void Buffer::writeToBuffer(void *data, VkDeviceSize size, VkDeviceSize offset) {
        assert(mapped && "Cannot copy to unmapped buffer");
        
        if (size == VK_WHOLE_SIZE) {
            memcpy(mapped, data, bufferSize);
        } else {
            char *memOffset = (char *)mapped;
            memOffset += offset;
            memcpy(memOffset, data, size);
        }
    }

    /**
     * Copies the specified data to a buffer only in device memory. 
     * A staging buffer in host memory is used as a intermediary to perform the copy.
     * 
     * @param data Pointer to the data to copy
     */
    void Buffer::stageToBuffer(void *data) {
        assert(usageFlags & VK_BUFFER_USAGE_TRANSFER_DST_BIT && 
            "Device buffer needs usage flag VK_BUFFER_USAGE_TRANSFER_DST_BIT to be copied to from host memory.");
        assert(usageFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT || usageFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT && 
            "Buffer is visible from host. Use writeToBuffer() instead.");
        assert(memoryPropertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT &&
            "Buffer is not in device memory. Needs VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT in memory property flags.");
        
        Buffer stagingBuffer{
            device,
            instanceSize,
            instanceCount,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        };

        stagingBuffer.map();
        stagingBuffer.writeToBuffer(data);
        device.copyBuffer(stagingBuffer.handle(), this->handle(), bufferSize);
    }
    
    /**
     * Flush a memory range of the buffer to make it visible to the device
     *
     * @note Only required for non-coherent memory
     *
     * @param size (Optional) Size of the memory range to flush. Pass VK_WHOLE_SIZE to flush the
     * complete buffer range.
     * @param offset (Optional) Byte offset from beginning
     *
     * @return VkResult of the flush call
     */
    VkResult Buffer::flush(VkDeviceSize size, VkDeviceSize offset) {
        VkMappedMemoryRange mappedRange = {};
        mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        mappedRange.memory = memory;
        mappedRange.offset = offset;
        mappedRange.size = size;
        return vkFlushMappedMemoryRanges(device.handle(), 1, &mappedRange);
    }

    /**
     * Update a small memory range of the buffer in device memory
     *
     * @note Only necessary for device only memory
     *
     * @param offset Byte offset from beginning.
     * @param size Size of the memory range to update. Must be less than or equal to 65536 bytes
     * @param data Pointer to the data to copy.
     */
    void Buffer::update(VkDeviceSize offset, VkDeviceSize size, const void* data) {
        assert(usageFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT && "Use memcpy for host visible buffers");
        assert(size <= 65536 && "Buffer update size must be less than or equal to 65536 bytes");
        assert(offset + size < bufferSize && "Buffer update goes outside of memory range");

        VkCommandBuffer cmd = device.beginSingleTimeCommands();
        vkCmdUpdateBuffer(cmd, buffer, offset, size, data);
        device.endSingleTimeCommands(cmd);
    }

    
    /**
     * Invalidate a memory range of the buffer to make it visible to the host
     *
     * @note Only required for non-coherent memory
     *
     * @param size (Optional) Size of the memory range to invalidate. Pass VK_WHOLE_SIZE to invalidate
     * the complete buffer range.
     * @param offset (Optional) Byte offset from beginning
     *
     * @return VkResult of the invalidate call
     */
    VkResult Buffer::invalidate(VkDeviceSize size, VkDeviceSize offset) {
        VkMappedMemoryRange mappedRange = {};
        mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        mappedRange.memory = memory;
        mappedRange.offset = offset;
        mappedRange.size = size;
        return vkInvalidateMappedMemoryRanges(device.handle(), 1, &mappedRange);
    }
    
    /**
     * Create a buffer info descriptor
     *
     * @param size (Optional) Size of the memory range of the descriptor
     * @param offset (Optional) Byte offset from beginning
     *
     * @return VkDescriptorBufferInfo of specified offset and range
     */
    VkDescriptorBufferInfo Buffer::descriptorInfo(VkDeviceSize size, VkDeviceSize offset) {
        return VkDescriptorBufferInfo{
            buffer,
            offset,
            size,
        };
    }
    
    /**
     * Copies "instanceSize" bytes of data to the mapped buffer at an offset of index * alignmentSize
     *
     * @param data Pointer to the data to copy
     * @param index Used in offset calculation
     *
     */
    void Buffer::writeToIndex(void *data, int index) {
        writeToBuffer(data, instanceSize, index * alignmentSize);
    }
 
    /**
     *  Flush the memory range at index * alignmentSize of the buffer to make it visible to the device
     *
     * @param index Used in offset calculation
     *
     */
    VkResult Buffer::flushIndex(int index) { return flush(alignmentSize, index * alignmentSize); }
        
    /**
     * Create a buffer info descriptor
     *
     * @param index Specifies the region given by index * alignmentSize
     *
     * @return VkDescriptorBufferInfo for instance at index
     */
    VkDescriptorBufferInfo Buffer::descriptorInfoForIndex(int index) {
        return descriptorInfo(alignmentSize, index * alignmentSize);
    }
 
    /**
     * Invalidate a memory range of the buffer to make it visible to the host
     *
     * @note Only required for non-coherent memory
     *
     * @param index Specifies the region to invalidate: index * alignmentSize
     *
     * @return VkResult of the invalidate call
     */
    VkResult Buffer::invalidateIndex(int index) {
        return invalidate(alignmentSize, index * alignmentSize);
    }
     
    /**
     * Returns the minimum instance size required to be compatible with devices minOffsetAlignment
     *
     * @param instanceSize The size of an instance
     * @param minOffsetAlignment The minimum required alignment, in bytes, for the offset member (eg
     * minUniformBufferOffsetAlignment)
     *
     * @return VkResult of the buffer mapping call
     */
    VkDeviceSize Buffer::getAlignment(VkDeviceSize instanceSize, VkDeviceSize minOffsetAlignment) {
        if (minOffsetAlignment > 0) {
            return (instanceSize + minOffsetAlignment - 1) & ~(minOffsetAlignment - 1);
        }
        return instanceSize;
    }

    uint64_t Buffer::getBufferDeviceAddress() {
        assert(buffer && "Buffer is null while trying to get its device address.");

        VkBufferDeviceAddressInfoKHR bufferDeviceAddressInfo{};
        bufferDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        bufferDeviceAddressInfo.buffer = buffer;
        return vkGetBufferDeviceAddressKHR(device.handle(), &bufferDeviceAddressInfo);
    }
}
 