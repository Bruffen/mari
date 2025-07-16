#pragma once

#include "components/camera.hpp"
#include "components/mesh.hpp"
#include "components/transform.hpp"

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
            using Map = std::unordered_map<uint32_t, Node>;

            Node();
            //Node(const Node &) = delete;
            //Node &operator=(const Node &) = delete;
            //Node(Node &&) = default;
            //Node &operator=(Node &&) = default;

            void start();
            void update();
            void render();

            const uint32_t                              getId() { return id; }
            static Node                                 makePointLight(float intensity = 10.0f, float radius = 0.1f, glm::vec3 color = glm::vec3(1.0f));

            std::string                                 name        = "";
            glm::vec3                                   color       {};         // TODO move to light component
            glm::mat4                                   worldMatrix {};         // world values
            Transform                                   transform   {};         // local values
            bool                                        isStatic    {true};
            float                                       fovy        {50.0f};

            // Optional pointer components
            std::weak_ptr<Node>                         parent;
            std::vector<std::shared_ptr<Node>>          children;
            std::shared_ptr<Mesh>                       mesh;
            std::shared_ptr<Camera>                     camera;
            // TODO skin component
            // TODO light component
            std::unique_ptr<PointLightComponent>        pointLight  = nullptr;

        private:
            uint32_t id;
            //Node(uint32_t id) : id{id} {}
    };
}