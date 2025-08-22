#pragma once

#include "sampling.hpp"

namespace mari {
    PiecewiseConstant1D::PiecewiseConstant1D(const std::span<float> values, float min, float max) : min(min), max(max) {
        cdf = std::vector<float>(values.size() + 1, 0.0f);
        
        func.reserve(values.size());
        for (const float& v : values) {
            func.push_back(std::abs(v));
        }
        
        cdf[0] = 0.0f;
        int n = static_cast<int>(values.size());
        for (int i = 1; i < n + 1; i++) {
            cdf[i] = cdf[i - 1] + func[i - 1] * (max - min) / float(n);
        }

        integral = cdf[n];
        if (integral == 0) {
            for (int i = 1; i < n + 1; i++) {
                cdf[i] = float(i) / float(n);   // Create linear cdf
            }
        }
        else {
            for (int i = 1; i < n + 1; i++) {
                cdf[i] /= integral;             // Normalize our valid cdf
            }
        }
    }

    PiecewiseConstant2D::PiecewiseConstant2D(const Device &device, const std::span<float> values, int nWidth, int nHeight) {
        pConditional.reserve(nHeight);

        for (int v = 0; v < nHeight; v++) {
            pConditional.emplace_back(values.subspan(v * nWidth, nWidth), 0.0f, 1.0f);
        }

        std::vector<float> marginal;
        for (int v = 0; v < nHeight; v++) {
            marginal.push_back(pConditional[v].getIntegral());
        }
        pMarginal = PiecewiseConstant1D(marginal, 0.0f, 1.0f);

        // Create buffers in device memory
        marginalFunctionBuffer = std::make_unique<Buffer>(
            device, 
            sizeof(float), 
            static_cast<int>(pMarginal.getFunction().size()),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );

        marginalFunctionBuffer->stageToBuffer((void*)pMarginal.getFunction().data());

        marginalCdfBuffer = std::make_unique<Buffer>(
            device, 
            sizeof(float), 
            static_cast<int>(pMarginal.getCdf().size()),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );

        marginalCdfBuffer->stageToBuffer((void*)pMarginal.getCdf().data());

        std::vector<float> conditionalCdfData;
        std::vector<float> conditionalFunctionData;
        std::vector<float> conditionalIntegrals;
        conditionalFunctionData.reserve(nWidth * nHeight);
        conditionalCdfData.reserve((nWidth + 1) * nHeight);
        conditionalIntegrals.reserve(nHeight);

        for (const PiecewiseConstant1D& p : pConditional) {
            for (const float &v : p.getFunction()) {
                conditionalFunctionData.push_back(v);
            }
            for (const float &v : p.getCdf()) {
                conditionalCdfData.push_back(v);
            }
            conditionalIntegrals.push_back(p.getIntegral());
        }

        conditionalFunctionBuffer = std::make_unique<Buffer>(
            device, 
            sizeof(float), 
            static_cast<int>(conditionalFunctionData.size()),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );

        conditionalFunctionBuffer->stageToBuffer(conditionalFunctionData.data());

        conditionalCdfBuffer = std::make_unique<Buffer>(
            device, 
            sizeof(float), 
            static_cast<int>(conditionalCdfData.size()),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );

        conditionalCdfBuffer->stageToBuffer(conditionalCdfData.data());

        conditionalIntegralBuffer = std::make_unique<Buffer>(
            device, 
            sizeof(float), 
            static_cast<int>(conditionalIntegrals.size()),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );

        conditionalIntegralBuffer->stageToBuffer(conditionalIntegrals.data());
    }
}