#pragma once

#include "transform.hpp"

namespace mari {
    // Matrix corrsponds to Translate * Ry * Rx * Rz * Scale
    // Rotations correspond to Tait-bryan angles of Y(1), X(2), Z(3)
    // https://en.wikipedia.org/wiki/Euler_angles#Rotation_matrix
    glm::mat4 Transform::mat4() const {
        const float c3 = glm::cos(rotation.z);
        const float s3 = glm::sin(rotation.z);
        const float c2 = glm::cos(rotation.x);
        const float s2 = glm::sin(rotation.x);
        const float c1 = glm::cos(rotation.y);
        const float s1 = glm::sin(rotation.y);
        return glm::mat4{
            {
                scale.x * (c1 * c3 + s1 * s2 * s3),
                scale.x * (c2 * s3),
                scale.x * (c1 * s2 * s3 - c3 * s1),
                0.0f,
            },
            {
                scale.y * (c3 * s1 * s2 - c1 * s3),
                scale.y * (c2 * c3),
                scale.y * (c1 * c3 * s2 + s1 * s3),
                0.0f,
            },
            {
                scale.z * (c2 * s1),
                scale.z * (-s2),
                scale.z * (c1 * c2),
                0.0f,
            },
            {
                position.x, position.y, position.z, 1.0f
            }
        };
    }

        VkTransformMatrixKHR Transform::matKHR() const {
        const float c3 = glm::cos(rotation.z);
        const float s3 = glm::sin(rotation.z);
        const float c2 = glm::cos(rotation.x);
        const float s2 = glm::sin(rotation.x);
        const float c1 = glm::cos(rotation.y);
        const float s1 = glm::sin(rotation.y);
        return VkTransformMatrixKHR{
            scale.x * (c1 * c3 + s1 * s2 * s3),
            scale.y * (c3 * s1 * s2 - c1 * s3),
            scale.z * (c2 * s1),
            position.x,

            scale.x * (c2 * s3),
            scale.y * (c2 * c3),
            scale.z * (-s2),
            position.y,

            scale.x * (c1 * s2 * s3 - c3 * s1),
            scale.y * (c1 * c3 * s2 + s1 * s3),
            scale.z * (c1 * c2),
            position.z
        };
    }

    glm::mat3 Transform::matrixNormal() const {
        const float c3 = glm::cos(rotation.z);
        const float s3 = glm::sin(rotation.z);
        const float c2 = glm::cos(rotation.x);
        const float s2 = glm::sin(rotation.x);
        const float c1 = glm::cos(rotation.y);
        const float s1 = glm::sin(rotation.y);
        const glm::vec3 invScale = 1.0f / scale;
        return glm::mat3{
            {
                invScale.x * (c1 * c3 + s1 * s2 * s3),
                invScale.x * (c2 * s3),
                invScale.x * (c1 * s2 * s3 - c3 * s1),
            },
            {
                invScale.y * (c3 * s1 * s2 - c1 * s3),
                invScale.y * (c2 * c3),
                invScale.y * (c1 * c3 * s2 + s1 * s3),
            },
            {
                invScale.z * (c2 * s1),
                invScale.z * (-s2),
                invScale.z * (c1 * c2),
            },
        };
    }

    glm::mat3 Transform::matrixRotation() const {
        const float c3 = glm::cos(rotation.z);
        const float s3 = glm::sin(rotation.z);
        const float c2 = glm::cos(rotation.x);
        const float s2 = glm::sin(rotation.x);
        const float c1 = glm::cos(rotation.y);
        const float s1 = glm::sin(rotation.y);

        return glm::mat3{
            {
                (c1 * c3 + s1 * s2 * s3),
                (c2 * s3),
                (c1 * s2 * s3 - c3 * s1)
            },
            {
                (c3 * s1 * s2 - c1 * s3),
                (c2 * c3),
                (c1 * c3 * s2 + s1 * s3)
            },
            {
                (c2 * s1),
                (-s2),
                (c1 * c2)
            }
        };
    }

    VkTransformMatrixKHR Transform::mat4ToKHR(const glm::mat4 &m) {
        return VkTransformMatrixKHR{
            m[0][0], m[1][0], m[2][0], m[3][0],
            m[0][1], m[1][1], m[2][1], m[3][1],
            m[0][2], m[1][2], m[2][2], m[3][2]
        };
    }
}