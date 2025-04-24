#pragma once

#include "device.hpp"
#include "buffer.hpp"
#include "scene/material.hpp"

#include <MikkTSpace/mikktspace.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include <memory>
#include <vector>

namespace mari {
    class Mesh {
        public:
            struct Vertex {
                glm::vec3 position;
                float pad0;
                glm::vec4 tangent;
                glm::vec3 normal;
                float pad1;
                glm::vec4 color;
                glm::vec2 uv;
                glm::vec2 pad2;
                
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

            Mesh(Device &device);
            ~Mesh();
            Mesh(const Mesh &) = delete;
            Mesh &operator=(const Mesh &) = delete;

            static std::unique_ptr<Mesh> createModelFromFile(Device &device, const std::string &filepath);
            static std::unique_ptr<Mesh> createCubeModel(Device& device, glm::vec3 offset);

            void bind(VkCommandBuffer commandBuffer);
            void draw(VkCommandBuffer commandBuffer);
            void calculateTangents();

            /**/ // TODO public members for now for ray tracing testing
            void createVertexBuffers(const std::vector<Vertex> &vertices);
            void createIndexBuffers(const std::vector<uint32_t> &indices);

            std::unique_ptr<Buffer>     vertexBuffer;
            uint32_t                    vertexCount;
            std::unique_ptr<Buffer>     indexBuffer;
            uint32_t                    indexCount;
            bool                        hasIndexBuffer = false;

            std::vector<Vertex>     vertices{};
            std::vector<uint32_t>   indices{};
            /**/

            std::string                 name = "";
            std::vector<PrimMesh>       primMeshes;
        private:
            Device &device;
    };

    class TangentHelper {
        public:
        static Mesh::Vertex*    getVertex(const SMikkTSpaceContext *context, int iFace, int iVert);
        static int              getNumFaces(const SMikkTSpaceContext * pContext);
	    static int              getNumVerticesOfFace(const SMikkTSpaceContext * pContext, const int iFace);
	    static void             getPosition(const SMikkTSpaceContext * pContext, float fvPosOut[], const int iFace, const int iVert);
	    static void             getNormal(const SMikkTSpaceContext * pContext, float fvNormOut[], const int iFace, const int iVert);
	    static void             getTexCoord(const SMikkTSpaceContext * pContext, float fvTexcOut[], const int iFace, const int iVert);
	    static void             setTSpaceBasic(const SMikkTSpaceContext * pContext, const float fvTangent[], const float fSign, const int iFace, const int iVert);
	    static void             setTSpace(const SMikkTSpaceContext * pContext, const float fvTangent[], const float fvBiTangent[], const float fMagS, const float fMagT,
						            const tbool bIsOrientationPreserving, const int iFace, const int iVert);
    };
}