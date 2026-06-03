#include "mari.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>

/*
#include "../shaders/test/bsdf.hpp"

void printMessage(std::string s) {
    std::cout << s << std::endl;
}

void printMessage(float s) {
    std::cout << s << std::endl;
}

float rando() {
    return rand() / (RAND_MAX + 1.0f);
}

float rando(float M, float N) {
    return M + (rand() / ( RAND_MAX / (N-M) ) ) ;  
}
*/
int main() {
/*
    material.roughness = 1.0;
    material.ior = 1.5;

    //float s;
    BxdfSample bs;

    for (int i = 0; i < 500; i++) {
        std::cout << "Sample " << i << std::endl;

        //glm::vec3 wo = glm::normalize(glm::vec3{rando(-1.0f, 1.0f), rando(-1.0f, 1.0f), rando(-1.0f, 1.0f)});
        //glm::vec3 r  = { rando(), rando(), rando() };
        //glm::vec3 wi = glm::normalize(glm::vec3{rando(-1.0f, 1.0f), rando(-1.0f, 1.0f), rando(-1.0f, 1.0f)});
        
        glm::vec3 wo = glm::vec3(0.673586, -0.018224, 0.738884);
        glm::vec3 r  = glm::vec3(0.673586,  0.018224, 0.738884);

        bs = bsdfDielectricSample(wo, glm::vec3(r.y, r.z, r.x), TransportMode_Radiance);
        if (!bs.failed) {
            printMessage(bs.f);
            printMessage(bs.pdf == 0 ? 69696969 : bs.pdf);
        } else {
            printMessage("failed");
        } 

        //s = bsdfDiffuseF(wo, wi);
        //s = bsdfDielectricF(wo, wi, m);
        //s = bsdfConductorF(wo, wi, m);
        //printMessage(s);

        //s = bsdfDiffusePDF(wo, wi);
        //printMessage(s);
        //s = bsdfDielectricPDF(wo, wi, m);
        //printMessage(s);
        //s = bsdfConductorPDF(wo, wi, m);
        //printMessage(s);
    }
    return 0;
*/
    mari::Mari mari{};

    try {
        mari.run();
    }
    catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
        std::cin.get();
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}