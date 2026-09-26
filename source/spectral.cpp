#pragma once

#include "spectral.hpp"

namespace mari {
    Spectral::Spectral() {
        rgb2Spec = rgb2spec_load("../../data/srgb.coeff");
    }

    Spectral& Spectral::instance() {
        static Spectral spectral;
        return spectral;
    }

    glm::vec3 Spectral::rgbToSpectral(glm::vec3 rgb) {
        glm::vec3 coeff;
        rgb2spec_fetch_opt(rgb2Spec, &rgb.x, &coeff.x);
        return coeff;
    }

    Spectral::~Spectral() {
        rgb2spec_free(rgb2Spec);
    }
}