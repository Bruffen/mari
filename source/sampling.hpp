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
            PiecewiseConstant1D(const std::span<float> values, float min, float max);

            std::span<const float>  getFunction()   const { return func;          }
            std::span<const float>  getCdf()        const { return cdf;           }
            float                   getIntegral()   const { return integral;      }
            glm::vec2               getBounds()     const { return {min, max};    }

        private:
            std::vector<float>      func{};
            std::vector<float>      cdf{};
            float                   integral;
            float                   min;
            float                   max;
    };

    class PiecewiseConstant2D {
        public:
            PiecewiseConstant2D() = default;
            PiecewiseConstant2D(const Device &device, const std::span<float> values, int nWidth, int nHeight);

            float                   getIntegral()   const { return pMarginal.getIntegral(); }

            std::unique_ptr<Buffer> conditionalIntegralBuffer;
            std::unique_ptr<Buffer> marginalFunctionBuffer;
            std::unique_ptr<Buffer> conditionalFunctionBuffer;
            std::unique_ptr<Buffer> marginalCdfBuffer;
            std::unique_ptr<Buffer> conditionalCdfBuffer;

        private:
            std::vector<PiecewiseConstant1D> pConditional;
            PiecewiseConstant1D     pMarginal;
    };
}