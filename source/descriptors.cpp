#include "descriptors.hpp"
 
// std
#include <cassert>
#include <stdexcept>
 
namespace mari {
 
    // *************** Descriptor Set Layout Builder *********************
 
    DescriptorSetLayout::Builder &DescriptorSetLayout::Builder::addBinding(
    uint32_t binding,
    VkDescriptorType descriptorType,
    VkShaderStageFlags stageFlags,
    uint32_t count,
    VkDescriptorBindingFlags flags
    ) {
        assert(bindings.count(binding) == 0 && "Binding already in use");
        VkDescriptorSetLayoutBinding layoutBinding{};
        layoutBinding.binding = binding;
        layoutBinding.descriptorType = descriptorType;
        layoutBinding.descriptorCount = count;
        layoutBinding.stageFlags = stageFlags;
        bindings[binding] = layoutBinding;
        bindingFlags.push_back(flags);

        return *this;
    }
 
    std::unique_ptr<DescriptorSetLayout> DescriptorSetLayout::Builder::build() const {
        VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo{};
        bindingFlagsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
        bindingFlagsInfo.bindingCount = static_cast<uint32_t>(bindingFlags.size());
        bindingFlagsInfo.pBindingFlags = bindingFlags.data();
        return std::make_unique<DescriptorSetLayout>(device, bindings, bindingFlagsInfo);
    }
 
    // *************** Descriptor Set Layout *********************
 
    DescriptorSetLayout::DescriptorSetLayout(
    Device &device, std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings, VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlags)
    : device{device}, bindings{bindings} {
        std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings{};
        for (auto kv : bindings) {
            setLayoutBindings.push_back(kv.second);
        }
 
        VkDescriptorSetLayoutCreateInfo descriptorSetLayoutInfo{};
        descriptorSetLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptorSetLayoutInfo.bindingCount = static_cast<uint32_t>(setLayoutBindings.size());
        descriptorSetLayoutInfo.pBindings = setLayoutBindings.data();
        descriptorSetLayoutInfo.pNext = &bindingFlags;
 
        if (vkCreateDescriptorSetLayout(
                device.handle(),
                &descriptorSetLayoutInfo,
                nullptr,
                &descriptorSetLayout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create descriptor set layout!");
        }
    }
 
    DescriptorSetLayout::~DescriptorSetLayout() {
        vkDestroyDescriptorSetLayout(device.handle(), descriptorSetLayout, nullptr);
    }
    
    // *************** Descriptor Pool Builder *********************
    
    DescriptorPool::Builder &DescriptorPool::Builder::addPoolSize(
    VkDescriptorType descriptorType, uint32_t count) {
        poolSizes.push_back({descriptorType, count});
        return *this;
    }
 
    DescriptorPool::Builder &DescriptorPool::Builder::setPoolFlags(
    VkDescriptorPoolCreateFlags flags) {
        poolFlags = flags;
        return *this;
    }
    DescriptorPool::Builder &DescriptorPool::Builder::setMaxSets(uint32_t count) {
        maxSets = count;
        return *this;
    }
 
    std::unique_ptr<DescriptorPool> DescriptorPool::Builder::build() const {
        return std::make_unique<DescriptorPool>(device, maxSets, poolFlags, poolSizes);
    }
    
    // *************** Descriptor Pool *********************
    
    DescriptorPool::DescriptorPool(Device &device, uint32_t maxSets, VkDescriptorPoolCreateFlags poolFlags, const std::vector<VkDescriptorPoolSize> &poolSizes)
    : device{device} {
        VkDescriptorPoolCreateInfo descriptorPoolInfo{};
        descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        descriptorPoolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        descriptorPoolInfo.pPoolSizes = poolSizes.data();
        descriptorPoolInfo.maxSets = maxSets;
        descriptorPoolInfo.flags = poolFlags;
    
        if (vkCreateDescriptorPool(device.handle(), &descriptorPoolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor pool!");
        }
    }
 
    DescriptorPool::~DescriptorPool() {
        vkDestroyDescriptorPool(device.handle(), descriptorPool, nullptr);
    }
 
    bool DescriptorPool::allocateDescriptors(
    const VkDescriptorSetLayout descriptorSetLayout, VkDescriptorSet &descriptor, const uint32_t descriptorCount) const {
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorPool;
        allocInfo.pSetLayouts = &descriptorSetLayout;
        allocInfo.descriptorSetCount = 1;

        VkDescriptorSetVariableDescriptorCountAllocateInfo variableDescriptorCountAllocInfo{};
        if (descriptorCount > 1) {
            uint32_t variableDescCounts[] = { descriptorCount };
            variableDescriptorCountAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO_EXT;
            variableDescriptorCountAllocInfo.descriptorSetCount = 1;
            variableDescriptorCountAllocInfo.pDescriptorCounts = variableDescCounts;
            
            allocInfo.pNext = &variableDescriptorCountAllocInfo;
        }
    
        // Might want to create a "DescriptorPoolManager" class that handles this case, and builds
        // a new pool whenever an old pool fills up. But this is beyond our current scope
        if (vkAllocateDescriptorSets(device.handle(), &allocInfo, &descriptor) != VK_SUCCESS) {
            return false;
        }
        return true;
    }
 
    void DescriptorPool::freeDescriptors(std::vector<VkDescriptorSet> &descriptors) const {
        vkFreeDescriptorSets(
            device.handle(),
            descriptorPool,
            static_cast<uint32_t>(descriptors.size()),
            descriptors.data()
        );
    }
 
    void DescriptorPool::resetPool() {
        vkResetDescriptorPool(device.handle(), descriptorPool, 0);
    }
 
    // *************** Descriptor Writer *********************
 
    DescriptorWriter::DescriptorWriter(DescriptorSetLayout &setLayout, DescriptorPool &pool)
        : setLayout{setLayout}, pool{pool} {}
 
    DescriptorWriter &DescriptorWriter::writeBuffer(
        uint32_t binding, VkDescriptorBufferInfo bufferInfo) {
        assert(setLayout.bindings.count(binding) == 1 && "Layout does not contain specified binding");
    
        auto &bindingDescription = setLayout.bindings[binding];
        
        assert(
            bindingDescription.descriptorCount == 1 &&
            "Binding single descriptor info, but binding expects multiple");
    
        VkWriteDescriptorSet write{};
        write.sType             = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.descriptorType    = bindingDescription.descriptorType;
        write.dstBinding        = binding;
        write.pBufferInfo       = &bufferInfo;
        write.descriptorCount   = 1;
        
        writes.push_back(write);
        return *this;
    }
 
    DescriptorWriter &DescriptorWriter::writeImage(uint32_t binding, VkDescriptorImageInfo imageInfo) {
        assert(setLayout.bindings.count(binding) == 1 && "Layout does not contain specified binding");
        
        auto &bindingDescription = setLayout.bindings[binding];
        
        assert(
            bindingDescription.descriptorCount == 1 &&
            "Binding single descriptor info, but binding expects multiple. Use writeImages() instead."
        );
        
        VkWriteDescriptorSet write{};
        write.sType             = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.descriptorType    = bindingDescription.descriptorType;
        write.dstBinding        = binding;
        write.pImageInfo        = &imageInfo;
        write.descriptorCount   = 1;
        
        writes.push_back(write);
        return *this;
    }

        DescriptorWriter &DescriptorWriter::writeImages(uint32_t binding, std::vector<VkDescriptorImageInfo> *imageInfos) {
        assert(setLayout.bindings.count(binding) == 1 && "Layout does not contain specified binding");
        
        auto &bindingDescription = setLayout.bindings[binding];

        assert(
            bindingDescription.descriptorCount == static_cast<uint32_t>(imageInfos->size()) &&
            "Binding single descriptor info, but binding expects multiple. Use writeImages() instead."
        );
        
        VkWriteDescriptorSet write{};
        write.sType             = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.descriptorType    = bindingDescription.descriptorType;
        write.descriptorCount   = bindingDescription.descriptorCount;
        write.dstBinding        = binding;
        write.pImageInfo        = imageInfos->data();
        
        writes.push_back(write);
        return *this;
    }

    DescriptorWriter &DescriptorWriter::writeAccelerationStructure(uint32_t binding, VkWriteDescriptorSetAccelerationStructureKHR accelerationStructureDescriptor) {
        assert(setLayout.bindings.count(binding) == 1 && "Layout does not contain specified binding");
        
        auto &bindingDescription = setLayout.bindings[binding];
        
        assert(
            bindingDescription.descriptorCount == 1 &&
            "Binding single descriptor info, but binding expects multiple"
        );
        
        VkWriteDescriptorSet write{};
        write.sType             = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.descriptorType    = bindingDescription.descriptorType;
        write.dstBinding        = binding;
        write.descriptorCount   = 1;
        write.pNext             = &accelerationStructureDescriptor;
        
        writes.push_back(write);
        return *this;
    }

 
    bool DescriptorWriter::build(VkDescriptorSet &set) {
        uint32_t maxDescriptorCount = 1;
        for (auto &write : writes) {
            if (write.descriptorCount > maxDescriptorCount) {
                maxDescriptorCount = write.descriptorCount;
            }
        }


        bool success = pool.allocateDescriptors(setLayout.handle(), set, maxDescriptorCount);
        if (!success) {
            return false;
        }
        overwrite(set);
        return true;
    }
 
    void DescriptorWriter::overwrite(VkDescriptorSet &set) {
        for (auto &write : writes) {
            write.dstSet = set;
        }
        vkUpdateDescriptorSets(pool.device.handle(), static_cast<uint32_t>(writes.size()), writes.data(), 0, VK_NULL_HANDLE);
    }
}
 