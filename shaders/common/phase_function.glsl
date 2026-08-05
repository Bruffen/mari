#ifndef _PHASE_FUNCTION_GLSL_
#define _PHASE_FUNCTION_GLSL_

#include "math.glsl"
#include "sampling.glsl"
#include "material.glsl"

#define PhaseFunctionType_Isotropic 0
#define PhaseFunctionType_Rayleigh 1
#define PhaseFunctionType_HenyeyGreenstein 2
#define PhaseFunctionType_Mie_Approximation 3

struct PhaseFunctionSample {
    vec3  wi;
    float p;
    float pdf;
};

/****************************************************************
 * Isotropic
 ****************************************************************
 */

float pf_isotropic_p() {
    return pdf_uniform_sphere();
}

float pf_isotropic_pdf() {
    return pdf_uniform_sphere();
}

PhaseFunctionSample pf_isotropic_sample(vec2 random) {
    return PhaseFunctionSample(
        sample_uniform_sphere(random.x, random.y), 
        pf_isotropic_p(),
        pf_isotropic_pdf()
    );
}

/****************************************************************
 * Rayleigh
 ****************************************************************
 * Penndorf approximation
 * Weakly anisotropic, so no importance sampling here
 */

float pf_rayleigh(float cos_theta) {
    return 0.7629 * (1.0 + 0.932 * cos_theta * cos_theta) * M_1_4PI;
}

float pf_rayleigh_p(vec3 wo, vec3 wi) {
    return pf_rayleigh(dot(wo, wi));
}

float pf_rayleigh_pdf(vec3 wo, vec3 wi) {
    return pdf_uniform_sphere();
}

PhaseFunctionSample pf_rayleigh_sample(vec3 wo, vec2 random) {
    vec3 wi = sample_uniform_sphere(random.x, random.y);

    return PhaseFunctionSample(
        wi,
        pf_rayleigh_p(wo, wi),
        pf_rayleigh_pdf(wo, wi)
    );
}

/****************************************************************
 * Henyey-Greenstein
 ****************************************************************
 */

float pf_henyey_greenstein(float cos_theta, float g) {
    float denom = 1.0 + sqr(g) + 2.0 * g * cos_theta;
    return M_1_4PI * (1.0 - sqr(g)) / (denom * safe_sqrt(denom));
}

float pf_henyey_greenstein_p(vec3 wo, vec3 wi, float g) {
    return pf_henyey_greenstein(dot(wo, wi), g);
}

float pf_henyey_greenstein_pdf(vec3 wo, vec3 wi, float g) {
    return pf_henyey_greenstein_p(wo, wi, g);
}

PhaseFunctionSample pf_henyey_greenstein_sample(vec3 wo, float g, vec2 random) {
    float cos_theta;
    if (abs(g) < 1e-3) {
        cos_theta = 1.0 - 2.0 * random.x;
    } else {
        cos_theta = -1.0 / (2.0 * g) * (1.0 + sqr(g) - sqr((1.0 - sqr(g)) / (1.0 + g - 2.0 * g * random.x)));
    }

    float sin_theta = safe_sqrt(1.0 - sqr(cos_theta));
    float phi = 2.0 * M_PI * random.y;
    mat3 frame = frame_from_z(wo);
    vec3 wi = from_local(frame[0], frame[1], frame[2], spherical_direction(sin_theta, cos_theta, phi));

    float pdf = pf_henyey_greenstein(cos_theta, g);

    return PhaseFunctionSample(wi, pdf, pdf);
}

/****************************************************************
 * Draine
 ****************************************************************
 * Code adapted from:
 * [Jendersie and d'Eon 2023]
 * SIGGRAPH 2023 Talks
 * https://doi.org/10.1145/3587421.3595409
 */

float pf_draine(float cos_theta, float g, float a) {
    return ((1 - g*g)*(1 + a*cos_theta*cos_theta))/(4.*(1 + (a*(1 + 2*g*g))/3.) * M_PI * pow(1 + g*g - 2*g*cos_theta,1.5));
}

float pf_draine(vec3 wo, vec3 wi, float g, float a) {
    return pf_draine(dot(wo, wi), g, a);
}

PhaseFunctionSample pf_draine_sample(vec3 wo, float g, float a, vec2 random) {
    const float g2 = g * g;
	const float g3 = g * g2;
	const float g4 = g2 * g2;
	const float g6 = g2 * g4;
	const float pgp1_2 = (1 + g2) * (1 + g2);
	const float T1 = (-1 + g2) * (4 * g2 + a * pgp1_2);
	const float T1a = -a + a * g4;
	const float T1a3 = T1a * T1a * T1a;
	const float T2 = -1296 * (-1 + g2) * (a - a * g2) * (T1a) * (4 * g2 + a * pgp1_2);
	const float T3 = 3 * g2 * (1 + g * (-1 + 2 * random.x)) + a * (2 + g2 + g3 * (1 + 2 * g2) * (-1 + 2 * random.x));
	const float T4a = 432 * T1a3 + T2 + 432 * (a - a * g2) * T3 * T3;
	const float T4b = -144 * a * g2 + 288 * a * g4 - 144 * a * g6;
	const float T4b3 = T4b * T4b * T4b;
	const float T4 = T4a + sqrt(-4 * T4b3 + T4a * T4a);
	const float T4p3 = pow(T4, 1.0 / 3.0);
	const float T6 = (2 * T1a + (48 * pow(2, 1.0 / 3.0) *
		(-(a * g2) + 2 * a * g4 - a * g6)) / T4p3 + T4p3 / (3. * pow(2, 1.0 / 3.0))) / (a - a * g2);
	const float T5 = 6 * (1 + g2) + T6;
	float cos_theta = (1 + g2 - pow(-0.5 * sqrt(T5) + sqrt(6 * (1 + g2) - (8 * T3) / (a * (-1 + g2) * sqrt(T5)) - T6) / 2., 2)) / (2. * g);
       
    float sin_theta = safe_sqrt(1.0 - sqr(cos_theta));
    float phi = 2.0 * M_PI * random.y;
    mat3 frame = frame_from_z(wo);
    vec3 wi = from_local(frame[0], frame[1], frame[2], spherical_direction(sin_theta, cos_theta, phi));

    //float pdf = pf_draine(cos_theta, g, a);

    return PhaseFunctionSample(wi, 1.0, 1.0);
}

/****************************************************************
 * Approximate Mie scattering
 * [Jendersie and d'Eon 2023]
 * SIGGRAPH 2023 Talks
 * https://doi.org/10.1145/3587421.3595409
 ****************************************************************
 */

void pf_mie_approximate_get_parameters(float particle_size, out float g_hg, out float g_draine, out float a_draine, out float weight) {
    if (particle_size <= 0.1) {
        g_hg = 13.8 * particle_size * particle_size;
        g_draine = 1.1456 * particle_size * sin(9.29044 * particle_size);
        a_draine = 250;
        weight = 0.252977 - 312.983 * pow(particle_size, 4.3);
    }
    else if (particle_size < 1.5) {
        float lps = log(particle_size);
        g_hg = 0.862 - 0.143 * lps * lps;
        g_draine = 0.379685 *
            cos(1.19692 *
                cos(((lps - 0.238604) * (lps + 1.00667)) /
                    (0.507522 - 0.15677 * lps))
                + 1.37932 * lps
                + 0.0625835)
            + 0.344213;
        a_draine = 250;
        weight = 0.146209 * cos(3.38707 * lps + 2.11193) + 0.316072 + 0.0778917 * lps;
    }
    else if (particle_size < 5.0) {
        float lps  = log(particle_size);
        float llps = log(lps);
        g_hg = 0.0604931 * llps + 0.940256;
        g_draine = 0.500411 - 0.081287 / (-2.0 * lps + tan(lps) + 1.27551);
        a_draine = 7.30354 * lps + 6.31675;
        weight = 0.026914 * (lps - cos(5.68947 * (llps - 0.0292149))) + 0.376475;
    }
    else {
        g_hg = exp(-0.0990567 / (particle_size - 1.67154));
        g_draine = exp(-2.20679 / (particle_size + 3.91029) - 0.428934);
        a_draine = exp(3.62489 - 8.29288 / (particle_size + 5.52825));
        weight = exp(-0.599085 / (particle_size - 0.641583) - 0.665888);
    }
}

float pf_mie_approximate_p(vec3 wo, vec3 wi, float particle_size, float random) {
    float g_hg, g_draine, a_draine, weight;
    pf_mie_approximate_get_parameters(particle_size, g_hg, g_draine, a_draine, weight);

    if (random < weight) {
        return pf_draine(dot(wo, wi), g_draine, a_draine);
    }
    else {
        return pf_henyey_greenstein_p(wo, wi, g_hg);
    }
}

float pf_mie_approximate_pdf(vec3 wo, vec3 wi, float particle_size, float random) {
    float g_hg, g_draine, a_draine, weight;
    pf_mie_approximate_get_parameters(particle_size, g_hg, g_draine, a_draine, weight);

    if (random < weight) {
        return pf_draine(dot(wo, wi), g_draine, a_draine);
    }
    else {
        return pf_henyey_greenstein_pdf(wo, wi, g_hg);
    }
}

PhaseFunctionSample pf_mie_approximate_sample(vec3 wo, float particle_size, vec3 random) {
    float g_hg, g_draine, a_draine, weight;
    pf_mie_approximate_get_parameters(particle_size, g_hg, g_draine, a_draine, weight);

    if (random.z < weight) {
        return pf_draine_sample(wo, g_draine, a_draine, random.xy);
    }
    else {
        return pf_henyey_greenstein_sample(wo, g_hg, random.xy);
    }
}

/****************************************************************
 * General Functions
 ****************************************************************
 */

float pf_eval_p(PhaseFunction pf, vec3 wo, vec3 wi, float random) {
    switch (pf.type) {
        default:
            return pf_isotropic_p();
        case PhaseFunctionType_Rayleigh:
            return pf_rayleigh_p(wo, wi);
        case PhaseFunctionType_HenyeyGreenstein:
            return pf_henyey_greenstein_p(wo, wi, pf.anisotropy);
        case PhaseFunctionType_Mie_Approximation:
            return pf_mie_approximate_p(wo, wi, pf.particle_size, random);
    }
}

float pf_eval_pdf(PhaseFunction pf, vec3 wo, vec3 wi, float random) {
    switch (pf.type) {
        default:
            return pf_isotropic_pdf();
        case PhaseFunctionType_Rayleigh:
            return pf_rayleigh_pdf(wo, wi);
        case PhaseFunctionType_HenyeyGreenstein:
            return pf_henyey_greenstein_pdf(wo, wi, pf.anisotropy);
        case PhaseFunctionType_Mie_Approximation:
            return pf_mie_approximate_pdf(wo, wi, pf.particle_size, random);
    }
}

PhaseFunctionSample pf_sample(PhaseFunction pf, vec3 wo, inout uint seed) {
    vec2 random = random2D(seed);

    switch (pf.type) {
        default:
            return pf_isotropic_sample(random);
        case PhaseFunctionType_Rayleigh:
            return pf_rayleigh_sample(wo, random);
        case PhaseFunctionType_HenyeyGreenstein:
            return pf_henyey_greenstein_sample(wo, pf.anisotropy, random);
        case PhaseFunctionType_Mie_Approximation:
            return pf_mie_approximate_sample(wo, pf.particle_size, vec3(random, random1D(seed)));
    }
}

#endif //_PHASE_FUNCTION_GLSL_