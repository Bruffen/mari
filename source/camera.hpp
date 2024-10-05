#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace mari {
    class Camera {
        public:
            static constexpr float MAX_FOV = glm::radians(179.0f);
            static constexpr float MIN_FOV = glm::radians(1.0f); 

            Camera();

            void setOrthographicProjection(float left, float right, float top, float bottom, float near, float far);
            void setPerspectiveProjection(float fovy, float aspect, float near, float far);
            void setPerspectiveProjection(float aspect, float near, float far);

            void setViewDirection(glm::vec3 position, glm::vec3 direction, glm::vec3 up = glm::vec3(0.0f, -1.0f, 0.0f));
            void setViewTarget(glm::vec3 position, glm::vec3 direction, glm::vec3 up = glm::vec3(0.0f, -1.0f, 0.0f));
            void setViewYXZ(glm::vec3 position, glm::vec3 rotation);

            const glm::mat4& getProjection() const { return projectionMatrix; }
            const glm::mat4& getView() const { return viewMatrix; }
            const glm::mat4& getInverseView() const { return inverseViewMatrix; }

            void changeFOV(float value);
            static void scrollCallback(GLFWwindow *window, double xoffset, double yoffset);

        private:
            float fov{glm::radians(50.0f)};
            float zoomSpeed{0.05f};
            glm::mat4 projectionMatrix{1.0f};
            glm::mat4 viewMatrix{1.0f};
            glm::mat4 inverseViewMatrix{1.0f};
    };
}