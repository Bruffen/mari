#pragma once

#include "model.hpp"

#include <memory>

namespace mari {

    struct Transform2dComponent {
        glm::vec2 translation{};
        glm::vec2 scale{1.0f, 1.0f};
        float rotation;
        glm::mat2 mat2() { 
            const float s = glm::sin(rotation);
            const float c = glm::cos(rotation);
            glm::mat2 rotMatrix{{c, s}, {-s, c}};
            glm::mat2 scaleMatrix{{scale.x, 0.0f}, {0.0f, scale.y}}; 

            return rotMatrix * scaleMatrix;
        }
    };

    // TODO https://austinmorlan.com/posts/entity_component_system/
    class GameObject {
        public:
            using id_t = unsigned int;

            static GameObject createGameObject() {
                static id_t currentId = 0;
                return GameObject{currentId++};
            }

            GameObject(const GameObject &) = delete;
            GameObject &operator=(const GameObject &) = delete;
            GameObject(GameObject &&) = default;
            GameObject &operator=(GameObject &&) = default;

            const id_t getId() { return id; }

            std::shared_ptr<Model> model{};
            glm::vec3 color{};
            Transform2dComponent transform2d{};

            private:
                GameObject(id_t objId) : id{objId} {}
            id_t id;
    };
}