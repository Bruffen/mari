#include "keyboard_controller.hpp"
#include <iostream>

namespace mari {
    KeyboardController::KeyboardController(Gui& gui) : gui{gui} {}

    void KeyboardController::update(GLFWwindow *window, float deltatime, Node &node) {
        moveCamera(window, deltatime, node);
        rotateCamera(window, deltatime, node);
        handleInput(window);
    }

    void KeyboardController::moveCamera(GLFWwindow *window, float deltatime, Node &node) {
        glm::vec3 f = node.transform.forward();
        glm::vec3 r = node.transform.right();
        glm::vec3 u = node.transform.up();
        float acceleration = 1.0f;

        glm::vec3 moveDir{0.0f};
        if (glfwGetKey(window, keys.moveForward)    == GLFW_PRESS) moveDir += f;
        if (glfwGetKey(window, keys.moveBackward)   == GLFW_PRESS) moveDir -= f;
        if (glfwGetKey(window, keys.moveRight)      == GLFW_PRESS) moveDir += r;
        if (glfwGetKey(window, keys.moveLeft)       == GLFW_PRESS) moveDir -= r;
        if (glfwGetKey(window, keys.moveUp)         == GLFW_PRESS) moveDir -= u;
        if (glfwGetKey(window, keys.moveDown)       == GLFW_PRESS) moveDir += u;

        if (glfwGetKey(window, keys.sprint)         == GLFW_PRESS) currentSpeed += currentSpeed * acceleration * deltatime;
        if (glfwGetKey(window, keys.sprint)         == GLFW_RELEASE || glm::length(moveDir) == 0.0f) currentSpeed = moveSpeed;

        if (glm::dot(moveDir, moveDir) > glm::epsilon<float>()) {
            node.transform.position += currentSpeed * deltatime * glm::normalize(moveDir);
            resetFrame = true;
        }
    }

    void KeyboardController::rotateCamera(GLFWwindow *window, float deltatime, Node &node) {
        double x, y;
        glfwGetCursorPos(window, &x, &y);

        gui.getIO()->AddMouseButtonEvent(mouse.cameraDrag, glfwGetMouseButton(window, mouse.cameraDrag));

        if (glfwGetMouseButton(window, mouse.cameraDrag) == GLFW_PRESS) {
            mouse.cameraDragDown = true;
        }
        else if (mouse.cameraDragDown && glfwGetMouseButton(window, mouse.cameraDrag) == GLFW_RELEASE) {
            mouse.cameraDragDown = false;
        }
        
        if (!gui.getIO()->WantCaptureMouse && mouse.cameraDragDown) {
            glm::vec2 movement{static_cast<float>(x - mouse.lastPositionX), static_cast<float>(y - mouse.lastPositionY)};
            if (glm::dot(movement, movement) > glm::epsilon<float>()) {
                node.transform.rotation.x -= movement.y * lookSpeed;
                node.transform.rotation.y += movement.x * lookSpeed;

                //node.transform.rotation.x = glm::clamp(node.transform.rotation.x, -1.5f, 1.5f);
                node.transform.rotation.y = glm::mod(node.transform.rotation.y, glm::two_pi<float>());       
                resetFrame = true;     
            }
        }

        mouse.lastPositionX = x;
        mouse.lastPositionY = y;
    }

    void KeyboardController::handleInput(GLFWwindow *window) {
        if (glfwGetKey(window, keys.changePipeline) == GLFW_PRESS) {
            changePipelinePressed = true;
        }
        if (glfwGetKey(window, keys.changePipeline) == GLFW_RELEASE && changePipelinePressed) {
            changePipelinePressed = false;
            isRaytracing = !isRaytracing;
            resetFrame = true;
        }

        if (glfwGetKey(window, keys.hideshowGui) == GLFW_PRESS && !hideshowGuiPressed) {
            gui.isActive = !gui.isActive;
            hideshowGuiPressed = true;
        }
        if (glfwGetKey(window, keys.hideshowGui) == GLFW_RELEASE) {
            hideshowGuiPressed = false;
        }
        
        if (gui.inputChanged) {
            gui.inputChanged = false;
            resetFrame = true;
        }
    }

    bool KeyboardController::checkFrameAccumulationReset() {
        if (resetFrame) {
            resetFrame = false;
            return true;
        }
        return false;
    }
}