#pragma once

#include "components/camera.hpp"
#include "components/mesh.hpp"
#include "components/transform.hpp"
#include "components/light.hpp"
#include "components/volume.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include <memory>
#include <unordered_map>

namespace mari {
    struct PointLightComponent { // TODO
        float lightIntensity = 1.0f;
    };

    // TODO https://austinmorlan.com/posts/entity_component_system/
    class Node {
        public:
            Node();
            Node(std::string name);
            //Node(const Node &) = delete;
            //Node &operator=(const Node &) = delete;
            //Node(Node &&) = default;
            //Node &operator=(Node &&) = default;

            void start(const glm::mat4& parent);
            void update(const glm::mat4& parent);
            void render();

            const uint32_t                              getId() { return id; }
            void                                        setVolumeTransform() { volume->setTransform(transform); }

            std::string                                 name        = "";
            glm::mat4                                   worldMatrix {1.0f};     // World values
            Transform                                   transform   {};         // local values
            bool                                        isStatic    {false};

            // Optional pointer components
            std::weak_ptr<Node>                         parent;
            std::vector<std::shared_ptr<Node>>          children;
            std::shared_ptr<Mesh>                       mesh;
            std::shared_ptr<Camera>                     camera;
            std::shared_ptr<Light>                      light;
            std::unique_ptr<Volume>                     volume;
        private:
            uint32_t id;
    };
}