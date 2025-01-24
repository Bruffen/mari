#pragma once

#include "mesh.hpp"
#include "utils.hpp"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#include <cassert>
#include <cstring>
#include <iostream>
#include <unordered_map>

namespace std {
    template <>
    struct hash<mari::Mesh::Vertex> {
        size_t operator() (mari::Mesh::Vertex const &vertex) const {
            size_t seed = 0;
            mari::hashCombine(seed, vertex.position, vertex.color, vertex.normal, vertex.uv);
            return seed;
        }
    };
}

namespace mari {
    Mesh::Mesh(Device &device, const Builder &builder) : device{device} {
        createVertexBuffers(builder.vertices);
        createIndexBuffers(builder.indices);
    }

    Mesh::Mesh(Device &device) : device{device} {
        
    }
   
    Mesh::~Mesh() {}

    std::unique_ptr<Mesh> Mesh::createModelFromFile(Device &device, const std::string &filepath) {
        Builder builder{};
        builder.loadModel(filepath);
        std::cout << "Vertex count: " << builder.vertices.size() << std::endl;
        std::cout << "Triangles count: " << builder.indices.size() / 3 << std::endl;
        return std::make_unique<Mesh>(device, builder);
    }


    void Mesh::createVertexBuffers(const std::vector<Vertex> &vertices) {
        vertexCount = static_cast<uint32_t>(vertices.size());
        assert(vertexCount >= 3 && "Vertex count must be at least 3");
        VkDeviceSize bufferSize = sizeof(vertices[0]) * vertexCount;

        uint32_t vertexSize = sizeof(vertices[0]);


        Buffer stagingBuffer{
            device,
            vertexSize,
            vertexCount,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        };

        stagingBuffer.map();
        stagingBuffer.writeToBuffer((void*) vertices.data());

        vertexBuffer = std::make_unique<Buffer>(
            device,
            vertexSize,
            vertexCount,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
            | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR, // TODO ray tracing specific
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );

        device.copyBuffer(stagingBuffer.handle(), vertexBuffer->handle(), bufferSize);
    }

    void Mesh::createIndexBuffers(const std::vector<uint32_t> &indices) {
        indexCount = static_cast<uint32_t>(indices.size());
        hasIndexBuffer = indexCount > 0;

        if (!hasIndexBuffer) {
            return;
        }

        VkDeviceSize bufferSize = sizeof(indices[0]) * indexCount;
        uint32_t indexSize = sizeof(indices[0]);

        Buffer stagingBuffer{
            device,
            indexSize,
            indexCount,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        };

        stagingBuffer.map();
        stagingBuffer.writeToBuffer((void*) indices.data());

        indexBuffer = std::make_unique<Buffer>(
            device,
            indexSize,
            indexCount,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
            | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR, // TODO ray tracing specific
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );

        device.copyBuffer(stagingBuffer.handle(), indexBuffer->handle(), bufferSize);
    }

    void Mesh::bind(VkCommandBuffer commandBuffer) {
        VkBuffer buffers[] = {vertexBuffer->handle()};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers, offsets);

        if (hasIndexBuffer) {
            vkCmdBindIndexBuffer(commandBuffer, indexBuffer->handle(), 0, VK_INDEX_TYPE_UINT32);
        }
    }

    void Mesh::draw(VkCommandBuffer commandBuffer) {
        if (hasIndexBuffer) {
            vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);
        } else {
            vkCmdDraw(commandBuffer, vertexCount, 1, 0, 0);
        }
    }

    std::vector<VkVertexInputBindingDescription> Mesh::Vertex::getBindingDescriptions() {
        std::vector<VkVertexInputBindingDescription> bindingDescriptions(1);
        bindingDescriptions[0].binding = 0;
        bindingDescriptions[0].stride = sizeof(Vertex);
        bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return bindingDescriptions;
    }

    std::vector<VkVertexInputAttributeDescription> Mesh::Vertex::getAttributeDescriptions() {
        std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};

        attributeDescriptions.push_back({0, 0, VK_FORMAT_R32G32B32_SFLOAT   , offsetof(Vertex, position)});
        attributeDescriptions.push_back({1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vertex, color)});
        attributeDescriptions.push_back({2, 0, VK_FORMAT_R32G32B32_SFLOAT   , offsetof(Vertex, normal)});
        attributeDescriptions.push_back({3, 0, VK_FORMAT_R32G32_SFLOAT      , offsetof(Vertex, uv)});

        return attributeDescriptions;
    }

    void Mesh::Builder::loadModel(const std::string &filepath) {
        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string warn, err;

        if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filepath.c_str())) {
            throw std::runtime_error(warn + err);
        }

        vertices.clear();
        indices.clear();

        std::unordered_map<Vertex, uint32_t> uniqueVertices{};

        for (const auto &shape : shapes) {
            for (const auto &index : shape.mesh.indices) {
                Vertex vertex{};

                if (index.vertex_index >= 0) {
                    vertex.position = {
                        attrib.vertices[3 * index.vertex_index + 0],
                        attrib.vertices[3 * index.vertex_index + 1],
                        attrib.vertices[3 * index.vertex_index + 2],
                    };

                    vertex.color = {
                        attrib.colors[3 * index.vertex_index + 0],
                        attrib.colors[3 * index.vertex_index + 1],
                        attrib.colors[3 * index.vertex_index + 2],
                        1.0f
                    };
                }

                if (index.normal_index >= 0) {
                    vertex.normal = {
                        attrib.normals[3 * index.normal_index + 0],
                        attrib.normals[3 * index.normal_index + 1],
                        attrib.normals[3 * index.normal_index + 2],
                    };
                }

                if (index.texcoord_index >= 0) {
                    vertex.uv = {
                        attrib.texcoords[2 * index.texcoord_index + 0],
                        attrib.texcoords[2 * index.texcoord_index + 1],
                    };
                }

                if (uniqueVertices.count(vertex) == 0) {
                    uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
                    vertices.push_back(vertex);
                }

                indices.push_back(uniqueVertices[vertex]);
            }
        }
    }

    std::unique_ptr<Mesh> Mesh::createCubeModel(Device& device, glm::vec3 offset) {
        Mesh::Builder modelBuilder{};
        /*modelBuilder.vertices = {
            // left face (white)
            {{-.5f, -.5f, -.5f}, {.9f, .9f, .9f, 1.f}, {-1.f, .0f, .0f}},
            {{-.5f, .5f, .5f}  , {.9f, .9f, .9f, 1.f}, {-1.f, .0f, .0f}},
            {{-.5f, -.5f, .5f} , {.9f, .9f, .9f, 1.f}, {-1.f, .0f, .0f}},
            {{-.5f, .5f, -.5f} , {.9f, .9f, .9f, 1.f}, {-1.f, .0f, .0f}},
        
            // right face (yellow)
            {{.5f, -.5f, -.5f} , {.8f, .8f, .1f, 1.f}, { 1.f, .0f, .0f}},
            {{.5f, .5f, .5f}   , {.8f, .8f, .1f, 1.f}, { 1.f, .0f, .0f}},
            {{.5f, -.5f, .5f}  , {.8f, .8f, .1f, 1.f}, { 1.f, .0f, .0f}},
            {{.5f, .5f, -.5f}  , {.8f, .8f, .1f, 1.f}, { 1.f, .0f, .0f}},
        
            // top face (orange, remember y axis points down)
            {{-.5f, -.5f, -.5f}, {.9f, .6f, .1f, 1.f}, {.0f, -1.f, .0f}},
            {{.5f, -.5f, .5f}  , {.9f, .6f, .1f, 1.f}, {.0f, -1.f, .0f}},
            {{-.5f, -.5f, .5f} , {.9f, .6f, .1f, 1.f}, {.0f, -1.f, .0f}},
            {{.5f, -.5f, -.5f} , {.9f, .6f, .1f, 1.f}, {.0f, -1.f, .0f}},
        
            // bottom face (red)
            {{-.5f, .5f, -.5f} , {.8f, .1f, .1f, 1.f}, { .0f, 1.f, .0f}},
            {{.5f, .5f, .5f}   , {.8f, .1f, .1f, 1.f}, { .0f, 1.f, .0f}},
            {{-.5f, .5f, .5f}  , {.8f, .1f, .1f, 1.f}, { .0f, 1.f, .0f}},
            {{.5f, .5f, -.5f}  , {.8f, .1f, .1f, 1.f}, { .0f, 1.f, .0f}},
        
            // nose face (blue)
            {{-.5f, -.5f, .5f} , {.1f, .1f, .8f, 1.f}, { .0f, .0f, 1.f}},
            {{.5f, .5f, .5f}   , {.1f, .1f, .8f, 1.f}, { .0f, .0f, 1.f}},
            {{-.5f, .5f, .5f}  , {.1f, .1f, .8f, 1.f}, { .0f, .0f, 1.f}},
            {{.5f, -.5f, .5f}  , {.1f, .1f, .8f, 1.f}, { .0f, .0f, 1.f}},
        
            // tail face (green)
            {{-.5f, -.5f, -.5f}, {.1f, .8f, .1f, 1.f}, { .0f, .0f, -1.f}},
            {{.5f, .5f, -.5f}  , {.1f, .8f, .1f, 1.f}, { .0f, .0f, -1.f}},
            {{-.5f, .5f, -.5f} , {.1f, .8f, .1f, 1.f}, { .0f, .0f, -1.f}},
            {{.5f, -.5f, -.5f} , {.1f, .8f, .1f, 1.f}, { .0f, .0f, -1.f}},
        };
        for (auto& v : modelBuilder.vertices) {
            v.position += offset;
        }
 
        modelBuilder.indices = {
            0,  1,  2,  0,  3,  1,  4,  5,  6,  4,  7,  5,  8,  9,  10, 8,  11, 9,
            12, 13, 14, 12, 15, 13, 16, 17, 18, 16, 19, 17, 20, 21, 22, 20, 23, 21
        };*/
        
        return std::make_unique<Mesh>(device, modelBuilder);
    }
}