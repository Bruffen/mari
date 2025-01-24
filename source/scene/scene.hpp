#pragma once

#include "device.hpp"
#include "default_objects.hpp"
#include "components/mesh.hpp"
#include "image.hpp"
#include "game_object.hpp"
#include "material.hpp"

#include <fastgltf/core.hpp>

#include <memory>
#include <unordered_map>
#include <string>

namespace mari {
    class Scene {
        public:
            Scene(Device &device, const std::string &path);
            ~Scene();

            void update();
            void updateWorldMatrix(GameObject& g, const glm::mat4 &worldMatrix); 

            Transform transform;

            std::unordered_map<std::string, std::shared_ptr<Mesh>>       meshes; // TODO move to private
            std::vector<std::shared_ptr<Image>>                          images;
            std::unordered_map<std::string, std::shared_ptr<GameObject>> nodes;
            std::vector<std::shared_ptr<GameObject>> topNodes;
            std::vector<std::shared_ptr<GameObject>> cameras;
        private:
            VkFilter                extractFilter(fastgltf::Filter filter);
            VkSamplerMipmapMode     extractMipmapMode(fastgltf::Filter filter);
            std::shared_ptr<Image>  extractImage(unsigned char* data, int width, int height, int channels);
            VkFormat                extractFormat(int channels); // TODO more complete implementation
            std::shared_ptr<Image>  loadImage(const std::string &folder, fastgltf::Asset& asset, fastgltf::Image& image);

            std::unordered_map<std::string, std::shared_ptr<Material>>   materials;

            std::vector<VkSampler>  samplers;
            std::unique_ptr<Buffer> materialDataBuffer;

            //DescriptorAllocatorGrowable descriptorPool;


            Device &device;
    };
}