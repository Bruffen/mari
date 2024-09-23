#pragma once

#include "camera.hpp"
#include "game_object.hpp"
#include "window.hpp"

namespace mari {
    class KeyboardController {
        public:
            struct KeyMappings {
                int moveLeft = GLFW_KEY_A;
                int moveRight = GLFW_KEY_D;
                int moveForward = GLFW_KEY_W;
                int moveBackward = GLFW_KEY_S;
                int moveUp = GLFW_KEY_E;
                int moveDown = GLFW_KEY_Q;
                int lookLeft = GLFW_KEY_LEFT;
                int lookRight = GLFW_KEY_RIGHT;
                int lookUp = GLFW_KEY_UP;
                int lookDown = GLFW_KEY_DOWN;
            };

            struct Mouse {
                int cameraDrag = GLFW_MOUSE_BUTTON_LEFT;
                bool cameraDragDown = false;
                double lastPositionX;
                double lastPositionY;
            };

            void moveInPlaneXZ(GLFWwindow *window, float deltatime, GameObject &gameObject);
            void moveCamera(GLFWwindow *window, float deltatime, GameObject &gameObject);
            void rotateCamera(GLFWwindow *window, float deltatime, GameObject &gameObject);

            KeyMappings keys{};
            Mouse mouse{};
            float moveSpeed{3.0f};
            float lookSpeed{0.25f};
    };
    
}