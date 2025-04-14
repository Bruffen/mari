#pragma once

#include "transform.hpp"

namespace mari {
    // Matrix corrsponds to Translate * Ry * Rz * Rx * Scale
    // Rotations correspond to Tait-bryan angles of Y(1), Z(2), X(3)
    // https://en.wikipedia.org/wiki/Euler_angles#Rotation_matrix
    glm::mat4 Transform::mat4() const {
        const float cy = glm::cos(rotation.x);
        const float sy = glm::sin(rotation.x);
        const float cb = glm::cos(rotation.z);
        const float sb = glm::sin(rotation.z);
        const float ca = glm::cos(rotation.y);
        const float sa = glm::sin(rotation.y);
        return glm::mat4{
            {
                scale.x * (ca * cb),
                scale.x * (sb),
                scale.x * (-cb * sa),
                0.0f,
            },
            {
                scale.y * (sa * sy - ca * cy * sb),
                scale.y * (cb * cy),
                scale.y * (ca * sy + cy * sa * sb),
                0.0f,
            },
            {
                scale.z * (cy * sa + ca * sb * sy),
                scale.z * (-cb * sy),
                scale.z * (ca * cy - sa * sb * sy),
                0.0f,
            },
            {
                position.x, position.y, position.z, 1.0f
            }
        };
    }

    glm::mat3 Transform::matrixNormal() const {
        const float cy = glm::cos(rotation.x);
        const float sy = glm::sin(rotation.x);
        const float cb = glm::cos(rotation.z);
        const float sb = glm::sin(rotation.z);
        const float ca = glm::cos(rotation.y);
        const float sa = glm::sin(rotation.y);
        const glm::vec3 invScale = 1.0f / scale;
        return glm::mat3{
            {
                invScale.x * (ca * cb),
                invScale.x * (sb),
                invScale.x * (-cb * sa),
            },
            {
                invScale.y * (sa * sy - ca * cy * sb),
                invScale.y * (cb * cy),
                invScale.y * (ca * sy + cy * sa * sb),
            },
            {
                invScale.z * (cy * sa + ca * sb * sy),
                invScale.z * (-cb * sy),
                invScale.z * (ca * cy - sa * sb * sy),
            }
        };
    }

    glm::mat3 Transform::matrixRotation() const {
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