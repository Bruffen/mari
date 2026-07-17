#pragma once

namespace mari {
    enum PhaseFunctionType {
        Isotropic = 0,
        Rayleigh = 1,
        HenyeyGreenstein = 2,
        MieApproximation = 3
    };

    struct PhaseFunction {
        PhaseFunctionType           type = PhaseFunctionType::HenyeyGreenstein;
        float                       anisotropy = 0.0f;
        float                       particleSize = 0.1f;
        auto operator<=>(const PhaseFunction&) const = default;
    };

    struct Medium {
        glm::vec3                   albedo = glm::vec3(1.0f);
        float                       absorption = 0.0f;
        float                       scattering = 0.0f;
        PhaseFunction               phaseFunction;
        VkBool32                    heterogeneous = false;
        auto operator<=>(const Medium&) const = default;
    };
}