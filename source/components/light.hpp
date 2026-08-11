#pragma once

#include "../device.hpp"

#include <glm/glm.hpp>

namespace mari {

    enum LightType {
        POINT           = 0,
        DIRECTIONAL     = 1,
        AREA            = 2,
        INFINITE        = 3
    };

    /** 
     * Information to be sent to the GPU for any kind of light
     * One emissive triangle is one area light
     * By passing vertex positions, we have the advantage of 
     * transforming them by the worldMatrix beforehand
     */
    struct LightInfo {
        LightType type;
        glm::vec3 positions[3];
        // TODO normals so we calculate the light's shading normal on the gpu
        // TODO uvs for sampling emissive texture
        // TODO texture id for emissive texture, potentially also usable with envmap?
        glm::vec3 emission;
        float     power;
        float     area;
        VkBool32  doubleSided;
    };

    class Light {
        public:
            Light(Device &device) : device{device} {}

            virtual ~Light() {};
            
            LightInfo info{};
        protected:
            Device &device;
    };
}