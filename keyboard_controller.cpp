#include "keyboard_controller.hpp"

namespace mari {
    void KeyboardController::moveInPlaneXZ(GLFWwindow *window, float deltatime, GameObject &gameObject) {
        glm::vec3 rotate{0};
        if (glfwGetKey(window, keys.lookRight) == GLFW_PRESS) rotate.y += 1.0f;
        if (glfwGetKey(window, keys.lookLeft) == GLFW_PRESS) rotate.y -= 1.0f;
        if (glfwGetKey(window, keys.lookUp) == GLFW_PRESS) rotate.x += 1.0f;
        if (glfwGetKey(window, keys.lookDown) == GLFW_PRESS) rotate.x -= 1.0f;

        if (glm::dot(rotate, rotate) > std::numeric_limits<float>::epsilon()) {
            gameObject.transform.rotation += lookSpeed * deltatime * glm::normalize(rotate);
        }

        gameObject.transform.rotation.x = glm::clamp(gameObject.transform.rotation.x, -1.5f, 1.5f);
        gameObject.transform.rotation.y = glm::mod(gameObject.transform.rotation.y, glm::two_pi<float>());

        float yaw = gameObject.transform.rotation.y;
        const glm::vec3 forward{sin(yaw), 0.0f, cos(yaw)};
        const glm::vec3 right{forward.z, 0.0f, -forward.x};
        const glm::vec3 up{0.0f, -1.0f, 0.0f};

        glm::vec3 moveDir{0.0f};
        if (glfwGetKey(window, keys.moveForward) == GLFW_PRESS) moveDir.z += 1.0f;
        if (glfwGetKey(window, keys.moveBackward) == GLFW_PRESS) moveDir.z -= 1.0f;
        if (glfwGetKey(window, keys.moveRight) == GLFW_PRESS) moveDir.x += 1.0f;
        if (glfwGetKey(window, keys.moveLeft) == GLFW_PRESS) moveDir.x -= 1.0f;
        if (glfwGetKey(window, keys.moveUp) == GLFW_PRESS) moveDir.y -= 1.0f;
        if (glfwGetKey(window, keys.moveDown) == GLFW_PRESS) moveDir.y += 1.0f;

        if (glm::dot(moveDir, moveDir) > std::numeric_limits<float>::epsilon()) {
            gameObject.transform.translation += moveSpeed * deltatime * glm::normalize(moveDir);
        }
    }
}