#include "game_object.hpp"

namespace mari {

    GameObject::GameObject() {
        static uint32_t currentId = 0;
        currentId++;
        GameObject{currentId};
    }

    void GameObject::update() {
        if (camera) {
            camera->update(transform);
        }
    }

    void GameObject::render() {

    }

    GameObject GameObject::makePointLight(float intensity, float radius, glm::vec3 color) {
        GameObject gameObj = GameObject();
        gameObj.color = color;
        gameObj.transform.scale.x = radius;
        gameObj.pointLight = std::make_unique<PointLightComponent>();
        gameObj.pointLight->lightIntensity = intensity;
        return gameObj;
    }
}