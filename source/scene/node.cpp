#include "node.hpp"

namespace mari {
    static uint32_t currentId = 0;
    
    Node::Node(std::string name) : name{name} {
        id = ++currentId;
    }

    Node::Node() : Node("") {}

    void Node::start(const glm::mat4& parent) {
        worldMatrix = parent * transform.mat4(); 
        for (auto &child : children) {
            child->start(worldMatrix);
        }

        if (volume) {
            // TODO this makes it so the buffers are built and sent to gpu twice unnecessarily.
            // Fix is the TODO written on top of the function's implementation
            setVolumeTransform(); 
        }
    }

    void Node::update(const glm::mat4& parent) {
        if (isStatic) {
            return;
        }

        if (camera) {
            camera->update(transform, parent);
        }

        worldMatrix = parent * transform.mat4();
        for (auto &child : children) {
            child->update(worldMatrix);
        }
    }

    void Node::render() {

    }
}