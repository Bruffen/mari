#pragma once

#include "components/camera.hpp"
#include "scene/node.hpp"
#include "gui.hpp"
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
                int sprint = GLFW_KEY_LEFT_SHIFT;
                int changePipeline = GLFW_KEY_P;
                int hideshowGui = GLFW_KEY_H;
            };

            struct Mouse {
                int cameraDrag = GLFW_MOUSE_BUTTON_LEFT;
                bool cameraDragDown = false;
                double lastPositionX;
                double lastPositionY;
            };

            KeyboardController(Gui& gui);

            void update(GLFWwindow *window, float deltatime, Node &node);
            bool checkFrameAccumulationReset();
            bool isRayTracingOn() { return isRaytracing; }

        private:
            void moveCamera(GLFWwindow *window, float deltatime, Node &node);
            void rotateCamera(GLFWwindow *window, float deltatime, Node &node);
            void handleInput(GLFWwindow *window);

            const KeyMappings keys;
            Mouse mouse{};
            Gui&  gui;
            float moveSpeed{1.0f};
            float currentSpeed{moveSpeed};
            float lookSpeed{0.002f};
            bool  changePipelinePressed = false;// TODO
            bool  resetFrame = false;
            bool  isRaytracing = true;
            bool  hideshowGuiPressed = false;
    };
    
}