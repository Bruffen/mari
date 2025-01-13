#pragma once

#include "device.hpp"
#include "default_objects.hpp"
#include "mesh.hpp"
#include "image.hpp"
#include "node.hpp"
#include "material.hpp"

#include <fastgltf/core.hpp>

#include <memory>
#include <unordered_map>
#include <string>

namespace mari {
    class Scene {
        public:
            Scene(Device &device, const DefaultObjects &defaultObjects, const std::string &path);
            ~Scene();
            std::unordered_map<std::string, std::shared_ptr<Mesh>>      meshes; // TODO move to private
        private:
            VkFilter                extractFilter(fastgltf::Filter filter);
            VkSamplerMipmapMode     extractMipmapMode(fastgltf::Filter filter);
            std::shared_ptr<Image>  loadImage(fastgltf::Asset& asset, fastgltf::Image& image);

            std::unordered_map<std::string, Node>                       nodes;
            std::unordered_map<std::string, std::shared_ptr<Image>>     images;
            std::unordered_map<std::string, std::shared_ptr<Material>>  materials;

            std::vector<Node>       topNodes;
            std::vector<VkSampler>  samplers;
            std::unique_ptr<Buffer> materialDataBuffer;

            //DescriptorAllocatorGrowable descriptorPool;


            Device &device;
    };
}