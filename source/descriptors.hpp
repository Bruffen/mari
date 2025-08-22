#pragma once
 
#include "device.hpp"
 
// std
#include <memory>
#include <unordered_map>
#include <vector>
 
namespace mari {
    
    class DescriptorSetLayout {
        public:
            class Builder {
                public:
                    Builder(Device &device) : device{device} {}
    
                    Builder &addBinding(
                        uint32_t binding,
                        VkDescriptorType descriptorType,
                        VkShaderStageFlags stageFlags,
                        uint32_t count = 1,
                        VkDescriptorBindingFlags flags = 0);

                    DescriptorSetLayout build() const;
                    
                private:
                    Device &device;
                    std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings{};
                    std::vector<VkDescriptorBindingFlags> bindingFlags{};
            };
    
            DescriptorSetLayout(
                Device &device, std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings, VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlags);
            ~DescriptorSetLayout();
            DescriptorSetLayout(const DescriptorSetLayout &) = delete;
            DescriptorSetLayout &operator=(const DescriptorSetLayout &) = delete;
        
            VkDescriptorSetLayout handle() const { return descriptorSetLayout; }
    
        private:
            Device &device;
            VkDescriptorSetLayout descriptorSetLayout;
            std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings;
    
            friend class DescriptorWriter;
    };
 
    class DescriptorPool {
        public:
            class Builder {
                public:
                    Builder(Device &device) : device{device} {}
 
                    Builder &addPoolSize(VkDescriptorType descriptorType, uint32_t count);
                    Builder &setPoolFlags(VkDescriptorPoolCreateFlags flags);
                    Builder &setMaxSets(uint32_t count);
                    std::unique_ptr<DescriptorPool> build() const;
 
                private:
                    Device &device;
                    std::vector<VkDescriptorPoolSize> poolSizes{};
                    uint32_t maxSets = 1000;
                    VkDescriptorPoolCreateFlags poolFlags = 0;
            };
 
            DescriptorPool(
                Device &device,
                uint32_t maxSets,
                VkDescriptorPoolCreateFlags poolFlags,
                const std::vector<VkDescriptorPoolSize> &poolSizes);
            ~DescriptorPool();
            DescriptorPool(const DescriptorPool &) = delete;
            DescriptorPool &operator=(const DescriptorPool &) = delete;
            
            VkDescriptorPool handle() { return descriptorPool; }

            bool allocateDescriptors(const VkDescriptorSetLayout descriptorSetLayout, VkDescriptorSet &descriptor, const uint32_t descriptorCount) const;
            void freeDescriptors(std::vector<VkDescriptorSet> &descriptors) const;
            void resetPool();
 
        private:
            Device &device;
            VkDescriptorPool descriptorPool;
            
            friend class DescriptorWriter;
    };
 
    class DescriptorWriter {
        public:
            DescriptorWriter(DescriptorSetLayout &setLayout, DescriptorPool &pool);
    
            DescriptorWriter &writeBuffer(uint32_t binding, VkDescriptorBufferInfo bufferInfo);
            DescriptorWriter &writeImage(uint32_t binding, VkDescriptorImageInfo imageInfo);
            DescriptorWriter &writeImages(uint32_t binding, std::vector<VkDescriptorImageInfo> *imageInfos);
            DescriptorWriter &writeAccelerationStructure(uint32_t binding, VkWriteDescriptorSetAccelerationStructureKHR accelerationStructureDescriptor);
            
            bool build(VkDescriptorSet &set);
            void overwrite(VkDescriptorSet &set);
    
        private:
            DescriptorSetLayout &setLayout;
            DescriptorPool &pool;
            std::vector<VkWriteDescriptorSet> writes;
    };
}