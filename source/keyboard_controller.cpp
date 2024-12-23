#include "keyboard_controller.hpp"
#include <iostream>

namespace mari {
    void KeyboardController::moveCamera(GLFWwindow *window, float deltatime, GameObject &gameObject) {
        glm::vec3 f = gameObject.transform.forward();
        glm::vec3 r = gameObject.transform.right();
        glm::vec3 u = gameObject.transform.up();
        float speed = 1.0f;

        glm::vec3 moveDir{0.0f};
        if (glfwGetKey(window, keys.moveForward)    == GLFW_PRESS) moveDir += f;
        if (glfwGetKey(window, keys.moveBackward)   == GLFW_PRESS) moveDir -= f;
        if (glfwGetKey(window, keys.moveRight)      == GLFW_PRESS) moveDir += r;
        if (glfwGetKey(window, keys.moveLeft)       == GLFW_PRESS) moveDir -= r;
        if (glfwGetKey(window, keys.moveUp)         == GLFW_PRESS) moveDir -= u;
        if (glfwGetKey(window, keys.moveDown)       == GLFW_PRESS) moveDir += u;
        if (glfwGetKey(window, keys.leftShift)      == GLFW_PRESS) speed = 2.0f;

        if (glm::dot(moveDir, moveDir) > glm::epsilon<float>()) {
            gameObject.transform.translation += moveSpeed * speed * deltatime * glm::normalize(moveDir);
        }
    }

    void KeyboardController::rotateCamera(GLFWwindow *window, float deltatime, GameObject &gameObject) {
        double x, y;
        glfwGetCursorPos(window, &x, &y);

        if (glfwGetMouseButton(window, mouse.cameraDrag) == GLFW_PRESS) {
            mouse.cameraDragDown = true;
        }
        else if (mouse.cameraDragDown && glfwGetMouseButton(window, mouse.cameraDrag) == GLFW_RELEASE) {
            mouse.cameraDragDown = false;
        }
        
        if (mouse.cameraDragDown) {
            glm::vec2 movement{static_cast<float>(x - mouse.lastPositionX), static_cast<float>(y - mouse.lastPositionY)};
            if (glm::dot(movement, movement) > glm::epsilon<float>()) {
                gameObject.transform.rotation.x -= movement.y * lookSpeed * deltatime;
                gameObject.transform.rotation.y += movement.x * lookSpeed * deltatime;

                gameObject.transform.rotation.x = glm::clamp(gameObject.transform.rotation.x, -1.5f, 1.5f);
                gameObject.transform.rotation.y = glm::mod(gameObject.transform.rotation.y, glm::two_pi<float>());            
            }
        }

        mouse.lastPositionX = x;
        mouse.lastPositionY = y;
    }
}