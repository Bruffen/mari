#include "keyboard_controller.hpp"
#include <iostream>

namespace mari {
    void KeyboardController::moveInPlaneXZ(GLFWwindow *window, float deltatime, GameObject &gameObject) {
        glm::vec3 rotate{0.0f};
        if (glfwGetKey(window, keys.lookRight)  == GLFW_PRESS) rotate.y += 1.0f;
        if (glfwGetKey(window, keys.lookLeft)   == GLFW_PRESS) rotate.y -= 1.0f;
        if (glfwGetKey(window, keys.lookUp)     == GLFW_PRESS) rotate.x += 1.0f;
        if (glfwGetKey(window, keys.lookDown)   == GLFW_PRESS) rotate.x -= 1.0f;

        if (glm::dot(rotate, rotate) > glm::epsilon<float>()) {
            gameObject.transform.rotation += lookSpeed * deltatime * glm::normalize(rotate);
        }

        gameObject.transform.rotation.x = glm::clamp(gameObject.transform.rotation.x, -1.5f, 1.5f);
        gameObject.transform.rotation.y = glm::mod(gameObject.transform.rotation.y, glm::two_pi<float>());

        float yaw = gameObject.transform.rotation.y;
        const glm::vec3 forward{sin(yaw), 0.0f, cos(yaw)};
        const glm::vec3 right{forward.z, 0.0f, -forward.x};
        const glm::vec3 up{0.0f, -1.0f, 0.0f};
        
        glm::vec3 moveDir{0.0f};
        if (glfwGetKey(window, keys.moveForward)    == GLFW_PRESS) moveDir += forward;
        if (glfwGetKey(window, keys.moveBackward)   == GLFW_PRESS) moveDir -= forward;
        if (glfwGetKey(window, keys.moveRight)      == GLFW_PRESS) moveDir += right;
        if (glfwGetKey(window, keys.moveLeft)       == GLFW_PRESS) moveDir -= right;
        if (glfwGetKey(window, keys.moveUp)         == GLFW_PRESS) moveDir -= up;
        if (glfwGetKey(window, keys.moveDown)       == GLFW_PRESS) moveDir += up;

        if (glm::dot(moveDir, moveDir) > glm::epsilon<float>()) {
            gameObject.transform.translation += moveSpeed * deltatime * glm::normalize(moveDir);
        }
    }

    void KeyboardController::moveCamera(GLFWwindow *window, float deltatime, GameObject &gameObject) {
        glm::vec3 rotate{0.0f};
        if (glfwGetKey(window, keys.lookRight)  == GLFW_PRESS) rotate.y += 1.0f;
        if (glfwGetKey(window, keys.lookLeft)   == GLFW_PRESS) rotate.y -= 1.0f;
        if (glfwGetKey(window, keys.lookUp)     == GLFW_PRESS) rotate.x += 1.0f;
        if (glfwGetKey(window, keys.lookDown)   == GLFW_PRESS) rotate.x -= 1.0f;

        if (glm::dot(rotate, rotate) > glm::epsilon<float>()) {
            gameObject.transform.rotation += lookSpeed * deltatime * glm::normalize(rotate);
        }

        gameObject.transform.rotation.x = glm::clamp(gameObject.transform.rotation.x, -1.5f, 1.5f);
        gameObject.transform.rotation.y = glm::mod(gameObject.transform.rotation.y, glm::two_pi<float>());

        glm::vec3 f = gameObject.transform.forward();
        glm::vec3 r = gameObject.transform.right();
        glm::vec3 u = gameObject.transform.up();

        glm::vec3 moveDir{0.0f};
        if (glfwGetKey(window, keys.moveForward)    == GLFW_PRESS) moveDir += f;
        if (glfwGetKey(window, keys.moveBackward)   == GLFW_PRESS) moveDir -= f;
        if (glfwGetKey(window, keys.moveRight)      == GLFW_PRESS) moveDir += r;
        if (glfwGetKey(window, keys.moveLeft)       == GLFW_PRESS) moveDir -= r;
        if (glfwGetKey(window, keys.moveUp)         == GLFW_PRESS) moveDir -= u;
        if (glfwGetKey(window, keys.moveDown)       == GLFW_PRESS) moveDir += u;

        if (glm::dot(moveDir, moveDir) > glm::epsilon<float>()) {
            gameObject.transform.translation += moveSpeed * deltatime * glm::normalize(moveDir);
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
                //movement = glm::normalize(movement);
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