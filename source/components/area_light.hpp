#pragma once
    
#include "light.hpp"
#include "../scene/node.hpp"
#include "../sampling.hpp"

namespace mari {       
    class AreaLight : public Light {
        public:
            AreaLight(Device &device, const Node &node, const PrimMesh &primMesh, const uint32_t indexStart);
        
        private:
    };
}