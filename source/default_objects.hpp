#pragma once

#include "device.hpp"
#include "image.hpp"

#include <memory>

namespace mari {
    class DefaultObjects {
        public:
            DefaultObjects() = delete;

            static void initialize(Device &device);
            static void cleanup(Device &device);

            static std::shared_ptr<Image> getImageWhite()               { return imageWhite; }
            static std::shared_ptr<Image> getImageBlack()               { return imageBlack; }
            static std::shared_ptr<Image> getImageError()               { return imageError; }
            static std::shared_ptr<Image> getImageWhite32f()            { return imageWhite32f; }
            static std::shared_ptr<Image> getImageBlack32f()            { return imageBlack32f; }

            static VkSampler              getSamplerNearest()           { return samplerNearest; }
            static VkSampler              getSamplerLinear()            { return samplerLinear;  }
            static VkSampler              getSamplerLinearClampEdge()   { return samplerLinearClampEdge;  }

        private:
            static VkSampler              samplerNearest;
            static VkSampler              samplerLinear;
            static VkSampler              samplerLinearClampEdge;
            //static VkSampler            samplerCubic; requires extension;
            static std::shared_ptr<Image> imageWhite;
            static std::shared_ptr<Image> imageBlack;
            static std::shared_ptr<Image> imageError;
            static std::shared_ptr<Image> imageWhite32f;
            static std::shared_ptr<Image> imageBlack32f;
            // TODO default material
    };
}