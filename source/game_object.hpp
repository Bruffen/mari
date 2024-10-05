#pragma once

#include "model.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include <memory>
#include <unordered_map>

namespace mari {

    struct TransformComponent {
        glm::vec3 translation{};
        glm::vec3 scale{1.0f, 1.0f, 1.0f};
        glm::vec3 rotation{};

        glm::mat4 mat4();
        glm::mat3 normalMatrix();

        glm::mat3 matrixRotation() {
            glm::vec3 forward{0.0f, 0.0f, 1.0f};
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

        glm::vec3 forward() { return matrixRotation() * glm::vec3{0.0f, 0.0f, 1.0f}; }
        glm::vec3 right()   { return matrixRotation() * glm::vec3{1.0f, 0.0f, 0.0f}; }
        glm::vec3 up()      { return matrixRotation() * glm::vec3{0.0f, 1.0f, 0.0f}; }
    };

    struct PointLightComponent {
        float lightIntensity = 1.0f;
    };

    // TODO https://austinmorlan.com/posts/entity_component_system/
    class GameObject {
        public:
            using id_t = unsigned int;
            using Map = std::unordered_map<id_t, GameObject>;

            static GameObject createGameObject() {
                static id_t currentId = 0;
                return GameObject{currentId++};
            }

            static GameObject makePointLight(float intensity = 10.0f, float radius = 0.1, glm::vec3 color = glm::vec3(1.0f));

            GameObject(const GameObject &) = delete;
            GameObject &operator=(const GameObject &) = delete;
            GameObject(GameObject &&) = default;
            GameObject &operator=(GameObject &&) = default;

            const id_t getId() { return id; }

            glm::vec3 color{};
            TransformComponent transform{};
            float fovy{50.0f};

            // Optional pointer components
            std::shared_ptr<Model> model{};
            std::unique_ptr<PointLightComponent> pointLight = nullptr;

            private:
                GameObject(id_t objId) : id{objId} {}
            id_t id;
    };
}