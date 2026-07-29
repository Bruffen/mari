#pragma once

#include "transform.hpp"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/euler_angles.hpp>

namespace mari {
    // Matrix corrsponds to Translate * Ry * Rz * Rx * Scale
    // Rotations correspond to Tait-bryan angles of Y(1), Z(2), X(3)
    // https://en.wikipedia.org/wiki/Euler_angles#Rotation_matrix
    glm::mat4 Transform::mat4() const {
        glm::mat3 rotation = matrixRotation();
        return glm::mat4{
            {
                scale.x * rotation[0][0],
                scale.x * rotation[0][1],
                scale.x * rotation[0][2],
                0.0f,
            },
            {
                scale.y * rotation[1][0],
                scale.y * rotation[1][1],
                scale.y * rotation[1][2],
                0.0f,
            },
            {
                scale.z * rotation[2][0],
                scale.z * rotation[2][1],
                scale.z * rotation[2][2],
                0.0f,
            },
            {
                position.x, position.y, position.z, 1.0f
            }
        };
    }

    glm::mat3 Transform::matrixNormal() const {
        const glm::vec3 invScale = 1.0f / scale;
        glm::mat3 m = matrixRotation();
        for (int i = 0; i < 3; i++)
            m[i] *= invScale;

        return m;
    }

    glm::mat3 Transform::matrixRotation() const {
        return glm::mat4(glm::quat(rotation)); // glm is in XYZ order

        // TODO fix
        const float cy = glm::cos(rotation.x);
        const float sy = glm::sin(rotation.x);
        const float cb = glm::cos(rotation.z);
        const float sb = glm::sin(rotation.z);
        const float ca = glm::cos(rotation.y);
        const float sa = glm::sin(rotation.y);

        return glm::mat3{
            {
                (ca * cb),
                (sb),
                (-cb * sa),
            },
            {
                (sa * sy - ca * cy * sb),
                (cb * cy),
                (ca * sy + cy * sa * sb),
            },
            {
                (cy * sa + ca * sb * sy),
                (-cb * sy),
                (ca * cy - sa * sb * sy),
            }
        };
    }
}