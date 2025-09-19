#ifndef _VOLUME_GLSL_
#define _VOLUME_GLSL_

struct Medium {
    vec3 absorption_color;
    float absorption;
    float scattering;
};

#define MAX_MEDIA 4
Medium media[MAX_MEDIA];
int current_medium = -1;

void medium_push(vec3 absorption_color, float absorption, float scattering) {
    if (current_medium >= MAX_MEDIA - 1) return;
    
    current_medium++;
    media[current_medium].absorption_color = absorption_color;
    media[current_medium].absorption = absorption;
    media[current_medium].scattering = scattering;
}

void medium_remove() {
    if (current_medium >= 0) current_medium--;
}

vec3 get_transmittance(Medium medium, float distance) {
    float attenuation = medium.absorption + medium.scattering;
    if (attenuation <= 0.0) return vec3(1.0);

    // if null-scattering coefficient == 0 (medium is homogenous)
    // apply beer's law directly
    //return pow(absorption_color, vec3(distance / attenuation));
    return exp(-attenuation * (vec3(1.0) - medium.absorption_color) * distance);

    // else
    // delta tracking
}

#endif //_VOLUME_GLSL_