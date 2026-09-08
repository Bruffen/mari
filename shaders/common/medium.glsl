#ifndef _MEDIUM_GLSL_
#define _MEDIUM_GLSL_

#include "math.glsl"
#include "sampling.glsl"
#include "material.glsl"
#include "phase_function.glsl"
#include "volume_nanovdb.glsl"

#define MAX_MEDIA 4
Medium media[MAX_MEDIA];
int current_medium = -1;

Medium get_medium_empty() {
    return Medium(vec3(0.0), 0.0, 0.0, PhaseFunction(0, 0.0, 0.0), false);
}

Medium get_medium_error() {
    return Medium(ERROR_COLOR / 1000.0, 5.0, 5.0, PhaseFunction(0, 0.0, 0.0), false);
}

void initialize_participating_media() {
    initialize_volume_nano();

    // Initialize stack with mediums that will show an error color if they get incorrectly sampled
    for (int i = 0; i < MAX_MEDIA; i++) {
        media[i] = get_medium_error();
    }
}

void medium_push(Medium medium) {
    if (current_medium >= MAX_MEDIA - 1) return;

    current_medium++;
    media[current_medium] = medium;
}

Medium medium_get() {
    if (current_medium >= 0 && current_medium < MAX_MEDIA) {
        return media[current_medium];
    } else {
        return get_medium_error();
    }
}

void medium_remove() {
    if (current_medium >= 0) {
        media[current_medium] = get_medium_error();
        current_medium--;
    }
}

#define Transmittance_Delta_Tracking          0
#define Transmittance_Ray_Marching            1
#define Transmittance_Ratio_Tracking          2
#define Transmittance_Residual_Ratio_Tracking 3

// Beer's law directly
vec3 get_transmittance(Medium medium, float distance) { // TODO attenuate with albedo
    float attenuation = medium.absorption + medium.scattering;
    if (attenuation <= 0.0) return vec3(1.0);

    return vec3(exp(-attenuation * (1.0 - medium.albedo) * distance));
}

float get_transmittance(float attenuation, float distance) {
    if (attenuation <= 0.0) return 1.0;
    return exp(-attenuation * distance);
}

struct MediumSample {
    vec3  albedo;
    float sigma_a;
    float sigma_s;
    vec3  emission;
};

MediumSample get_medium_sample_empty() {
    return MediumSample( vec3(0), 0, 0, vec3(0));
}

struct MediumEvent {
    PhaseFunctionSample pf;
    MediumSample sampl;
    Medium medium;
    float t;
    float majorant;
    float p_a;
    float p_s;
    float p_n;
    vec3  transmittance;
    bool  terminated;
    bool  scattered;
};

MediumEvent get_medium_event_empty() {
    return MediumEvent(
        PhaseFunctionSample(vec3(0.0), 1, 1),
        get_medium_sample_empty(),
        get_medium_empty(),
        0.0,
        0.0,
        0.0,
        0.0,
        0.0,
        vec3(1.0),
        false,
        false
    );
}

MediumSample sample_medium(vec3 position, vec3 wo, Medium medium, float majorant, inout uint seed) {
    if (!medium.heterogeneous) {
        //if (length(position) < 0.1) {
        return MediumSample(medium.albedo, medium.absorption, medium.scattering, vec3(0.0));
        //} else {
        //    //return get_medium_sample_empty();
        //    return MediumSample(medium.albedo, 0.0, 0.3, vec3(0.0));
        //}
    }

    vec3  jitter      = (2.0 * random3D(seed) - 1.0) * volume.jittering_amount; // TODO scale with voxel size -> pnanovdb_grid_get_voxel_size()
    vec2  sample_data = sample_volume_nano(position + jitter);
	float density     = sample_data.x;
	float temperature = max(sample_data.y * volume.temperature_multiplier, 0.0);
    float intensity   = M_stefan_boltzmann * pow(temperature, 4) * volume.emissiveness_multiplier;

    MediumSample medium_sample;
    medium_sample.sigma_a  = clamp(medium.absorption * density, 0.0, majorant);
    medium_sample.sigma_s  = clamp(medium.scattering * density, 0.0, majorant);
    medium_sample.albedo   = medium.albedo;
    medium_sample.emission = blackbody(temperature) * intensity;

    return medium_sample;
}

MediumEvent sample_medium_event(vec3 position, vec3 wo, Medium medium, inout uint seed) {
    MediumEvent medium_event = get_medium_event_empty();
    medium_event.medium      = medium; // TODO does medium event need medium?
    medium_event.majorant    = medium_event.medium.absorption + medium_event.medium.scattering;
    medium_event.sampl       = sample_medium(position, wo, medium_event.medium, medium_event.majorant, seed);
    medium_event.p_a         = medium_event.sampl.sigma_a / medium_event.majorant;
    medium_event.p_s         = medium_event.sampl.sigma_s / medium_event.majorant;
    medium_event.p_n         = max(0.0, 1.0 - medium_event.p_a - medium_event.p_s);

    float event = random1D(seed);

    // Absorption
    if (event < medium_event.p_a) {                   
        medium_event.terminated = true;
        medium_event.transmittance = vec3(0.0);
    } 
    // Scattering
    else if (event < medium_event.p_a + medium_event.p_s) {
        medium_event.pf = pf_sample(medium_event.medium.phase_function, wo, seed);
        medium_event.scattered = true;
    } 
    // Null
    else {

    }

    return medium_event;
}

// TODO unbiased ray marching
MediumEvent ray_marching(vec3 origin, vec3 direction, float tmin, float tmax, inout uint seed) {
    MediumEvent medium_event = get_medium_event_empty();
    medium_event.medium = medium_get();
    medium_event.majorant = medium_event.medium.absorption + medium_event.medium.scattering;
    float t;
    float step_size = (tmax - tmin) * 0.01;

    for (int i = 0; i < 200; i++) { // TODO crashes with a while loop?
        t = tmin + step_size;// + step_size * (random1D(seed) * 0.5 - 1.0); // TODO jitter
        if (t < tmax) {
            vec3 position = origin + direction * t;

            medium_event.sampl = sample_medium(position, -direction, medium_event.medium, medium_event.majorant, seed);
            medium_event.transmittance *= get_transmittance((t - tmin), (medium_event.sampl.sigma_a + medium_event.sampl.sigma_s));
            tmin = t;
        }
        else {
            // Reached end of ray
            vec3 position = origin + direction * tmax;

            medium_event.sampl = sample_medium(position, -direction, medium_event.medium, medium_event.majorant, seed);
            medium_event.transmittance *= get_transmittance((tmax - tmin), (medium_event.sampl.sigma_a + medium_event.sampl.sigma_s));
            medium_event.t = tmax;
            break;
        }
    }

    return medium_event;
}


MediumEvent delta_tracking(vec3 origin, vec3 direction, float tmin, float tmax, inout uint seed) {
    MediumEvent medium_event = get_medium_event_empty();
    medium_event.medium = medium_get();
    medium_event.majorant = medium_event.medium.absorption + medium_event.medium.scattering;
    float t;

    while (true) {
        t = tmin + sample_exponential(random1D(seed), medium_event.majorant);
        if (t < tmax) {
            vec3 position = origin + direction * t;
            
            medium_event = sample_medium_event(position, -direction, medium_event.medium, seed);

            if (medium_event.terminated || medium_event.scattered) {
                medium_event.t = t;
                break;
            }
            // Null scatter event
            // TODO accumulate emission with null scatters and return through MediumEvent
            tmin = t;
        }
        else {
            // Reached end of ray
            medium_event.t = tmax;
            break;
        }
    }

    return medium_event;
}

MediumEvent ratio_tracking(vec3 origin, vec3 direction, float tmin, float tmax, inout uint seed) {
    MediumEvent medium_event = get_medium_event_empty();
    medium_event.medium = medium_get();
    medium_event.majorant = medium_event.medium.absorption + medium_event.medium.scattering;
    float t;
    vec3 transmittance = vec3(1.0);

    while (true) {
        t = tmin + sample_exponential(random1D(seed), medium_event.majorant);
        if (t < tmax) {
            vec3 position = origin + direction * t;

            medium_event = sample_medium_event(position, -direction, medium_event.medium, seed);
                
            transmittance *= medium_event.p_n;
            tmin = t;
        }
        else {
            // Reached end of ray
            medium_event.t = tmax;
            break;
        }
    }

    medium_event.transmittance = transmittance;
    return medium_event;
}

MediumEvent residual_ratio_tracking(vec3 origin, vec3 direction, float tmin, float tmax, inout uint seed) {
    MediumEvent medium_event = get_medium_event_empty();
    medium_event.medium = medium_get();
    medium_event.majorant = medium_event.medium.absorption + medium_event.medium.scattering;
    float majorant_control = medium_event.majorant * 0.1;
    vec3 transmittance_control = vec3(get_transmittance(majorant_control, tmax - tmin));
    float majorant_residual = medium_event.majorant - majorant_control;
    vec3 transmittance_residual = vec3(1.0);

    float t;
    while (true) {
        t = tmin + sample_exponential(random1D(seed), majorant_residual);
        if (t < tmax) {
            vec3 position = origin + direction * t;

            medium_event = sample_medium_event(position, -direction, medium_event.medium, seed);
                
            transmittance_residual *= 1.0 - ((medium_event.sampl.sigma_a + medium_event.sampl.sigma_s) - majorant_control) / majorant_residual;
            tmin = t;
        }
        else {
            // Reached end of ray
            medium_event.t = tmax;
            break;
        }
    }

    medium_event.transmittance = transmittance_control * transmittance_residual;
    return medium_event;
}

vec3 sample_transmittance_along_ray(vec3 origin, vec3 direction, float tmin, float tmax, inout uint seed, int transmittance_algo) {
    vec3 transmittance = vec3(1.0);

    if (current_medium >= 0) {
        Medium m = medium_get();
        if (m.heterogeneous) {
            switch(transmittance_algo) {
                case Transmittance_Delta_Tracking: 
                    MediumEvent e = delta_tracking(origin, direction, tmin, tmax, seed);
                    if (e.terminated || e.scattered) {
                        transmittance = vec3(0.0);
                    }
                    break;
                case Transmittance_Ray_Marching:
                    transmittance = ray_marching(origin, direction, tmin, tmax, seed).transmittance;
                    break;
                case Transmittance_Ratio_Tracking:
                    transmittance = ratio_tracking(origin, direction, tmin, tmax, seed).transmittance;
                    break;
                case Transmittance_Residual_Ratio_Tracking:
                    transmittance = residual_ratio_tracking(origin, direction, tmin, tmax, seed).transmittance;
                    break;
            }
        } else {
            // TODO: could we separate transmittance for absorption and scattering and multiply scattering's transmittance 
            // with the phase function p / pdf of continuing in the same direction?
            // A: apparently there is a similar idea called the reduced scattering coefficient 
            // https://pbr-book.org/3ed-2018/Light_Transport_II_Volume_Rendering/Subsurface_Scattering_Using_the_Diffusion_Equation
            // but nothing describes removing direction perserving paths from delta tracking to beer's law
            transmittance = vec3(get_transmittance(m.absorption + m.scattering, tmax - tmin)); // TODO shadows lose color with single float transmittance
        }
    }
    return transmittance;
}


MediumEvent sample_medium_along_ray(vec3 origin, vec3 direction, float t, inout uint seed) {
    direction = normalize(direction);

    float tmin = 1e-6;
    float tmax = t;
    bool  done = false;
    bool  to_remove_medium = false;

    if (current_medium < 0) { // TODO replace with test_intersection_volume_nano() to ray gen shader
        float tmax_surface = tmax;
        bool hit = pnanovdb_hdda_ray_clip(
            volume_nano.density.bbox_min,
            volume_nano.density.bbox_max,
            origin,
            tmin,
            direction,
            tmax
        );
        // Handling impossible cases
        if (tmax < 0.0)  hit = false;
        if (tmin < 1e-6) hit = false;
        if (tmin > tmax) hit = false;
        tmax = min(tmax_surface, tmax);

        if (!hit) {
            // TODO don't terminate since there can be a bigger non vdb medium, like fog, that needs to be sampled
            return get_medium_event_empty(); 
        }

        // Push volume_nano data into array of media
        medium_push(volume.medium);
        to_remove_medium = true;
    }

    MediumEvent medium_event;
    if (medium_get().scattering > 0.0 || medium_get().heterogeneous) {
        medium_event = delta_tracking(origin, direction, tmin, tmax, seed);
    } else {
        medium_event = get_medium_event_empty();
        medium_event.transmittance = get_transmittance(medium_get(), tmax - tmin); // TODO
    }

    if (to_remove_medium) {
        medium_remove();
    }
    return medium_event;
}

#endif //_MEDIUM_GLSL_