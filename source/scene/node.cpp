#include "node.hpp"

namespace mari {
    static uint32_t currentId = 0;
    
    Node::Node() {
        id = ++currentId;
    }

    void Node::start() {

    }

    void Node::update() {
        if (camera) {
            camera->update(transform, worldMatrix);
        }
    }

    void Node::render() {

    }

    Node Node::makePointLight(float intensity, float radius, glm::vec3 color) {
        Node node = Node();
        node.color = color;
        node.transform.scale.x = radius;
        node.pointLight = std::make_unique<PointLightComponent>();
        node.pointLight->lightIntensity = intensity;
        return node;
    }
}