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
            Scene(Device &VkDevice);
            ~Scene();

            void                                    start();
            void                                    update();
            void                                    load(const std::string &path);
            std::shared_ptr<Node>                   getNode(const uint32_t id);
            void                                    addNode(std::shared_ptr<Node> node);

            static std::shared_ptr<Image>           loadImage(Device &device, const std::string &path, VkFormat format);
            static std::shared_ptr<Image>           loadImage(Device &device, const std::string &path, VkFormat format, const std::string &name);

            Transform transform;

            std::vector<std::shared_ptr<Image>>     images;
            std::unordered_map<std::string, std::shared_ptr<Node>> nodes;
            std::vector<std::shared_ptr<Node>>      topNodes;
            std::vector<std::shared_ptr<Node>>      cameraObjects;
            std::vector<std::shared_ptr<Node>>      lightObjects; // currently this is actually just infinite area lights
            std::shared_ptr<Node>                   volumeObject;
            std::vector<std::shared_ptr<Material>>  materials;
            std::unique_ptr<Buffer>                 materialDataBuffer;
            std::shared_ptr<Node>                   currentCamera;
            int                                     environmentID = 0;
        private:
            void                                    loadSamplers(const std::vector<fastgltf::Sampler> &gltfSamplers);
            std::shared_ptr<Image>                  loadImage(const std::string &folder, fastgltf::Asset& asset, fastgltf::Image& image);
            void                                    loadImages(std::vector<fastgltf::Image> &gltfImages, fastgltf::Asset& asset, const std::string& folderPath);
            void                                    loadMaterials(const std::vector<fastgltf::Material> &gltfMaterials, const std::vector<fastgltf::Texture> &gltfTextures);
            void                                    loadMeshes(const fastgltf::Asset &gltf);
            void                                    loadCameras(const std::vector<fastgltf::Camera> &gltfCameras);
            static VkFilter                         extractFilter(fastgltf::Filter filter);
            static VkSamplerMipmapMode              extractMipmapMode(fastgltf::Filter filter);
            static std::shared_ptr<Image>           extractImage(Device &device, void* data, VkFormat format, int width, int height);
            static VkFormat                         extractFormat(int channels); // TODO more complete implementation

            std::vector<std::shared_ptr<Mesh>>      meshes;
            std::vector<std::shared_ptr<Camera>>    cameras;
            
            std::vector<VkSampler>  samplers;

            //DescriptorPool descriptorPool; // TODO

            Device &device;
    };
}