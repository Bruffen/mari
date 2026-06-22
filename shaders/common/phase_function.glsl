#ifndef _PHASE_FUNCTION_GLSL_
#define _PHASE_FUNCTION_GLSL_

#include "math.glsl"
#include "sampling.glsl"

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

    float pdf = pf_draine(cos_theta, g, a);

    return PhaseFunctionSample(wi, pdf, pdf);
}

#define PhaseFunctionType_Isotropic 0
#define PhaseFunctionType_HenyeyGreenstein 1
#define PhaseFunctionType_Draine 2

float pf_eval_p(uint id, vec3 wo, vec3 wi, float g, float a) {
    switch (id) {
        default:
            return pf_isotropic_p();
        case PhaseFunctionType_HenyeyGreenstein:
            return pf_henyey_greenstein_p(wo, wi, g);
        case PhaseFunctionType_Draine:
            return pf_draine(wo, wi, g, a);
    }
}

float pf_eval_pdf(uint id, vec3 wo, vec3 wi, float g, float a) {
    switch (id) {
        default:
            return pf_isotropic_pdf();
        case PhaseFunctionType_HenyeyGreenstein:
            return pf_henyey_greenstein_pdf(wo, wi, g);
        case PhaseFunctionType_Draine:
            return pf_draine(wo, wi, g, a);
    }
}

PhaseFunctionSample pf_sample(uint id, vec3 wo, float g, float a, vec2 random) {
    switch (id) {
        default:
            return pf_isotropic_sample(random);
        case PhaseFunctionType_HenyeyGreenstein:
            return pf_henyey_greenstein_sample(wo, g, random);
        case PhaseFunctionType_Draine:
            return pf_draine_sample(wo, g, a, random);
    }
}

#endif //_PHASE_FUNCTION_GLSL_