#pragma once

#include "area_light.hpp"

#include <numbers>

namespace mari {

    AreaLight::AreaLight(Device &device, const Node &node, const PrimMesh &primMesh, const uint32_t indexStart) : Light(device) {
        assert(primMesh.start + indexStart + 2 <= node.mesh->indexCount - 1 && "AreaLight(): Indices out of bounds");
        
        for (uint32_t i = 0; i < 3; i++) {
            uint32_t vertexIndex = node.mesh->indices[primMesh.start + indexStart + i];

            glm::vec4 position = glm::vec4(node.mesh->vertices[vertexIndex].position, 1.0f);
            info.positions[i] = node.worldMatrix * position;
        }
        
        glm::vec4 color  = primMesh.material->data.constants.emission;
        
        info.type = LightType::AREA;
        info.emission = glm::vec3(color.r, color.g, color.b) * color.a;
        info.doubleSided = 1; // TODO implement double or single sided light in material
        info.area = glm::length(glm::cross(info.positions[1] - info.positions[0], info.positions[2] - info.positions[0])) * 0.5f;
        info.power = glm::length(info.emission) * info.area * (info.doubleSided ? 2.0f : 1.0f) * static_cast<float>(std::numbers::pi);
    }
}