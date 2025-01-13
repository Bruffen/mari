#pragma once

#include "device.hpp"
#include "buffer.hpp"
#include "scene/material.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include <memory>
#include <vector>

namespace mari {
    class Mesh {
        public:
            struct Vertex {
                glm::vec3 position{};
                glm::vec4 color{};
                glm::vec3 normal{};
                glm::vec2 uv{};
                
                static std::vector<VkVertexInputBindingDescription> getBindingDescriptions();
                static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();
            
                bool operator==(const Vertex &other) const {
                    return  
                        position == other.position && 
                        color    == other.color && 
                        normal   == other.normal && 
                        uv       == other.uv;
                }
            };

            struct Builder {
                std::vector<Vertex> vertices{};
                std::vector<uint32_t> indices{};

                void loadModel(const std::string &filepath);
            };

            Mesh(Device &device);
            Mesh(Device &device, const Builder &builder);
            ~Mesh();
            Mesh(const Mesh &) = delete;
            Mesh &operator=(const Mesh &) = delete;

            static std::unique_ptr<Mesh> createModelFromFile(Device &device, const std::string &filepath);
            static std::unique_ptr<Mesh> createCubeModel(Device& device, glm::vec3 offset);

            void bind(VkCommandBuffer commandBuffer);
            void draw(VkCommandBuffer commandBuffer);

            /**/ // TODO public members for now for ray tracing testing
            void createVertexBuffers(const std::vector<Vertex> &vertices);
            void createIndexBuffers(const std::vector<uint32_t> &indices);

            std::unique_ptr<Buffer> vertexBuffer;
            uint32_t vertexCount;

            bool hasIndexBuffer = false;
            std::unique_ptr<Buffer> indexBuffer;
            uint32_t indexCount;
            /**/

            std::string name;
            std::vector<Primitive> primitives;
        private:


            Device &device;
    };
}