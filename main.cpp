#include "renderer.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>

int main() {
    mari::Renderer renderer{};

    try {
        renderer.run();
    }
    catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}