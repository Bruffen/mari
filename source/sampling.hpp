#pragma once

#include "buffer.hpp"

#include <glm/glm.hpp>
#include <vector>
#include <span>
#include <memory>

namespace mari {
    class PiecewiseConstant1D {
        public:
            PiecewiseConstant1D() = default;
            PiecewiseConstant1D(const std::span<float> values, float min, float max, const Device *device = nullptr);

            std::span<const float>      getFunction()       const { return func;                 }
            std::span<const float>      getCdf()            const { return cdf;                  }
            float                       getIntegral()       const { return integral;             }
            glm::vec2                   getBounds()         const { return {min, max};           }
            const Buffer*               getFunctionBuffer() const { return functionBuffer.get(); }
            const Buffer*               getCdfBuffer()      const { return cdfBuffer.get();      }

        private:
            std::vector<float>          func{};
            std::vector<float>          cdf{};
            float                       integral;
            float                       min;
            float                       max;

            std::unique_ptr<Buffer>     functionBuffer;
            std::unique_ptr<Buffer>     cdfBuffer;
    };

    class PiecewiseConstant2D {
        public:
            PiecewiseConstant2D() = default;
            PiecewiseConstant2D(const Device &device, const std::span<float> values, int nWidth, int nHeight);

            const PiecewiseConstant1D*  getMarginal() const { return &pMarginal; }

            std::unique_ptr<Buffer>     conditionalIntegralsBuffer;
            std::unique_ptr<Buffer>     conditionalFunctionsBuffer;
            std::unique_ptr<Buffer>     conditionalCdfsBuffer;

        private:
            std::vector<PiecewiseConstant1D> pConditional;
            PiecewiseConstant1D         pMarginal;
    };
}