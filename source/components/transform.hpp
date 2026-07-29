#pragma once

#include <glm/gtc/matrix_transform.hpp>
#include <vulkan/vulkan.h>

namespace mari {
    struct Transform {
        glm::vec3 position {0.0f, 0.0f, 0.0f};
        glm::vec3 rotation {0.0f, 0.0f, 0.0f};
        glm::vec3 scale    {1.0f, 1.0f, 1.0f};

        glm::mat4                   mat4()              const;
        glm::mat3                   matrixNormal()      const;
        glm::mat3                   matrixRotation()    const;

        glm::vec3 right()   { return matrixRotation() * glm::vec3{1.0f, 0.0f, 0.0f}; }
        glm::vec3 up()      { return matrixRotation() * glm::vec3{0.0f, 1.0f, 0.0f}; }
        glm::vec3 forward() { return matrixRotation() * glm::vec3{0.0f, 0.0f, 1.0f}; } // TODO get vector directly instead
    };
}