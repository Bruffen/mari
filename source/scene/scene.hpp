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

            Transform transform;

            std::vector<std::shared_ptr<Image>>                          images;
            std::unordered_map<std::string, std::shared_ptr<GameObject>> nodes;
            std::vector<std::shared_ptr<GameObject>> topNodes;
            std::vector<std::shared_ptr<GameObject>> cameraObjects;
            std::unique_ptr<Buffer> materialDataBuffer;
        private:
            void                    updateWorldMatrix(GameObject& g, const glm::mat4 &worldMatrix);

            void                    loadSamplers(const std::vector<fastgltf::Sampler> &gltfSamplers);
            std::shared_ptr<Image>  loadImage(const std::string &folder, fastgltf::Asset& asset, fastgltf::Image& image);
            void                    loadImages(std::vector<fastgltf::Image> &gltfImages, fastgltf::Asset& asset, const std::string& folderPath);
            void                    loadMaterials(const std::vector<fastgltf::Material> &gltfMaterials, const std::vector<fastgltf::Texture> &gltfTextures);
            void                    loadMeshes(const fastgltf::Asset &gltf);
            void                    loadCameras(const std::vector<fastgltf::Camera> &gltfCameras);
            VkFilter                extractFilter(fastgltf::Filter filter);
            VkSamplerMipmapMode     extractMipmapMode(fastgltf::Filter filter);
            std::shared_ptr<Image>  extractImage(unsigned char* data, int width, int height, int channels);
            VkFormat                extractFormat(int channels); // TODO more complete implementation

            std::vector<std::shared_ptr<Mesh>>       meshes;
            std::vector<std::shared_ptr<Material>>   materials;
            std::vector<std::shared_ptr<Camera>>     cameras;
            
            std::vector<VkSampler>  samplers;

            //DescriptorPool descriptorPool;


            Device &device;
    };
}