#pragma once

#include "device.hpp"
#include "image.hpp"

#include <memory>

namespace mari {
    class DefaultObjects { // TODO static?
        public:
            DefaultObjects(Device &device);
            ~DefaultObjects();

            std::shared_ptr<Image>  getImageWhite()     const { return imageWhite; }
            std::shared_ptr<Image>  getImageBlack()     const { return imageBlack; }
            std::shared_ptr<Image>  getImageError()     const { return imageError; }

            const VkSampler         getSamplerNearest() const { return samplerNearest; }
            const VkSampler         getSamplerLinear()  const { return samplerLinear;  }
        private:
            std::shared_ptr<Image> imageWhite, imageBlack, imageError;
            VkSampler samplerNearest, samplerLinear; //samplerCubic requires extension;
            
            Device &device;
    };
}