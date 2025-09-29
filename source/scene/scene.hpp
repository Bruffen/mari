#pragma once

#include "device.hpp"
#include "default_objects.hpp"
#include "components/mesh.hpp"
#include "image.hpp"
#include "node.hpp"
#include "material.hpp"
#include "components/area_light.hpp"
#include "components/infinite_light.hpp"

#include <fastgltf/core.hpp>

#include <memory>
#include <unordered_map>
#include <string>

namespace mari {
    class Scene {
        public:
            Scene(Device &device, const std::string &path);
            ~Scene();

            void                    start();
            void                    update();
            std::shared_ptr<Image>  loadImage(const std::string &path, VkFormat format);
            std::shared_ptr<Image>  loadImage(const std::string &path, VkFormat format, const std::string &name);
            std::shared_ptr<Node>   getNode(const uint32_t id);
            void                    addNode(std::shared_ptr<Node> node);

            Transform transform;

            
            std::vector<std::shared_ptr<Image>>                             images;
            std::unordered_map<std::string, std::shared_ptr<Node>>          nodes;
            std::vector<std::shared_ptr<Node>>                              topNodes;
            std::vector<std::shared_ptr<Node>>                              cameraObjects;
            std::vector<std::shared_ptr<Node>>                              lightObjects;
            std::vector<std::shared_ptr<Material>>                          materials;
            std::unique_ptr<Buffer>                                         materialDataBuffer;
            std::shared_ptr<Node>                                           currentCamera;
            int                                                             environmentID = 0;
        private:
            void                    loadSamplers(const std::vector<fastgltf::Sampler> &gltfSamplers);
            std::shared_ptr<Image>  loadImage(const std::string &folder, fastgltf::Asset& asset, fastgltf::Image& image);
            void                    loadImages(std::vector<fastgltf::Image> &gltfImages, fastgltf::Asset& asset, const std::string& folderPath);
            void                    loadMaterials(const std::vector<fastgltf::Material> &gltfMaterials, const std::vector<fastgltf::Texture> &gltfTextures);
            void                    loadMeshes(const fastgltf::Asset &gltf);
            void                    loadCameras(const std::vector<fastgltf::Camera> &gltfCameras);
            VkFilter                extractFilter(fastgltf::Filter filter);
            VkSamplerMipmapMode     extractMipmapMode(fastgltf::Filter filter);
            std::shared_ptr<Image>  extractImage(void* data, VkFormat format, int width, int height);
            VkFormat                extractFormat(int channels); // TODO more complete implementation

            std::vector<std::shared_ptr<Mesh>>       meshes;
            std::vector<std::shared_ptr<Camera>>     cameras;
            
            std::vector<VkSampler>  samplers;

            //DescriptorPool descriptorPool; // TODO


            Device &device;
    };
}