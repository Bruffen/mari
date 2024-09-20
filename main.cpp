#include "mari.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>

int main() {
    mari::Mari mari{};

    try {
        mari.run();
    }
    catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}