#pragma once

#include "mesh.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#include <cassert>
#include <cstring>
#include <iostream>
#include <unordered_map>

namespace mari {
    Mesh::Mesh(Device &device) : device{device} {
        
    }
   
    Mesh::~Mesh() {}

    void Mesh::createVertexBuffers(const std::vector<Vertex> &vertices) {
        vertexCount = static_cast<uint32_t>(vertices.size());
        assert(vertexCount >= 3 && "Vertex count must be at least 3!");

        this->vertices = vertices;

        // TODO calculateTangents(); // This will calculate tangents in this->vertices but not in vertices!

        vertexBuffer = std::make_unique<Buffer>(
            device,
            sizeof(this->vertices[0]),
            vertexCount,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
            | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );
        vertexBuffer->stageToBuffer((void*) this->vertices.data());
    }

    void Mesh::createIndexBuffers(const std::vector<uint32_t> &indices) {
        indexCount = static_cast<uint32_t>(indices.size());
        hasIndexBuffer = indexCount > 0;

        if (!hasIndexBuffer) {
            return;
        }

        this->indices = indices;

        indexBuffer = std::make_unique<Buffer>(
            device,
            sizeof(this->indices[0]),
            indexCount,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
            | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );
        indexBuffer->stageToBuffer((void*) this->indices.data());
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
    
    /**
     * Tangent Helper
     * 
     * Assumes every face is a triangle
     */

    void Mesh::calculateTangents() {
        SMikkTSpaceInterface iface{};
        SMikkTSpaceContext context{};

        iface.m_getNumFaces             = TangentHelper::getNumFaces;
        iface.m_getNumVerticesOfFace    = TangentHelper::getNumVerticesOfFace;
        iface.m_getPosition             = TangentHelper::getPosition;
        iface.m_getNormal               = TangentHelper::getNormal;
        iface.m_getTexCoord             = TangentHelper::getTexCoord;
        iface.m_setTSpaceBasic          = TangentHelper::setTSpaceBasic;

        context.m_pInterface = &iface;
        context.m_pUserData = this;

        genTangSpaceDefault(&context);
    }

    int TangentHelper::getNumFaces(const SMikkTSpaceContext *pContext) {
        Mesh* m = static_cast<Mesh*>(pContext->m_pUserData);

        int numFaces, numFacesRemainder;
        if (m->hasIndexBuffer) {
            numFacesRemainder = m->indexCount % 3; 
            numFaces = m->indexCount / 3;
        }
        else {
            numFacesRemainder = m->vertexCount % 3; 
            numFaces = m->vertexCount / 3;
        }
        assert(numFacesRemainder == 0 && "Mesh is not composed of triangles only!"); 
        return numFaces;
    }

    int TangentHelper::getNumVerticesOfFace(const SMikkTSpaceContext *pContext, const int iFace) {
        return 3;
    }

    Mesh::Vertex* TangentHelper::getVertex(const SMikkTSpaceContext *pContext, int iFace, int iVert) {
        Mesh* m = static_cast<Mesh*>(pContext->m_pUserData);

        uint32_t index = iFace * getNumVerticesOfFace(pContext, iFace) + iVert;
        
        if (m->hasIndexBuffer) {
            index = m->indices[index];
        }

        return &m->vertices[index];
    }

    void TangentHelper::getPosition(const SMikkTSpaceContext *pContext, float fvPosOut[], const int iFace, const int iVert) {
        glm::vec3 position = getVertex(pContext, iFace, iVert)->position;
        fvPosOut[0] = position.x;
        fvPosOut[1] = position.y;
        fvPosOut[2] = position.z;
    }

    void TangentHelper::getNormal(const SMikkTSpaceContext *pContext, float fvNormOut[], const int iFace, const int iVert) {
        glm::vec3 normal = getVertex(pContext, iFace, iVert)->normal;
        fvNormOut[0] = normal.x;
        fvNormOut[1] = normal.y;
        fvNormOut[2] = normal.z;
    }

    void TangentHelper::getTexCoord(const SMikkTSpaceContext *pContext, float fvTexcOut[], const int iFace, const int iVert) {
        glm::vec2 uv = getVertex(pContext, iFace, iVert)->uv;
        fvTexcOut[0] = uv.x;
        fvTexcOut[1] = uv.y;
    }

    void TangentHelper::setTSpaceBasic(const SMikkTSpaceContext *pContext, const float fvTangent[], const float fSign, const int iFace, const int iVert) {
        Mesh::Vertex* v = getVertex(pContext, iFace, iVert);

        v->tangent.x = fvTangent[0];
        v->tangent.y = fvTangent[1];
        v->tangent.z = fvTangent[2];
        v->tangent.w = fSign;
    }

    void TangentHelper::setTSpace(const SMikkTSpaceContext *pContext, const float fvTangent[], const float fvBiTangent[], const float fMagS, const float fMagT,
        const tbool bIsOrientationPreserving, const int iFace, const int iVert) {
            // TODO
    }
}