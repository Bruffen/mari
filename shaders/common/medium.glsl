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

// Beer's law directly
vec3 get_transmittance(Medium medium, float distance) { // TODO attenuate with albedo
    float attenuation = medium.absorption + medium.scattering;
    if (attenuation <= 0.0) return vec3(1.0);

    return vec3(exp(-attenuation * distance));
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
    float n_a;
    float n_s;
    float n_n;
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

MediumSample sample_medium(vec3 position, vec3 wo, Medium medium, float majorant) {
    if (!medium.heterogeneous) {
        //if (length(position) < 0.1) {
        return MediumSample(medium.albedo, medium.absorption, medium.scattering, vec3(0.0));
        //} else {
        //    //return get_medium_sample_empty();
        //    return MediumSample(medium.albedo, 0.0, 0.3, vec3(0.0));
        //}
    }

    vec2  sample_data = sample_volume_nano(position);
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

MediumEvent sample_medium_event(vec3 position, vec3 wo, inout uint seed) {
    MediumEvent medium_event = get_medium_event_empty();
    medium_event.medium      = medium_get();
    medium_event.majorant    = medium_event.medium.absorption + medium_event.medium.scattering;
    medium_event.sampl       = sample_medium(position, wo, medium_event.medium, medium_event.majorant);
    medium_event.n_a         = medium_event.sampl.sigma_a / medium_event.majorant;
    medium_event.n_s         = medium_event.sampl.sigma_s / medium_event.majorant;
    medium_event.n_n         = max(0.0, 1.0 - medium_event.n_a - medium_event.n_s);

    float event = random1D(seed);

    // Absorption
    if (event < medium_event.n_a) {                   
        medium_event.terminated = true;
        medium_event.transmittance = vec3(0.0);
    } 
    // Scattering
    else if (event < medium_event.n_a + medium_event.n_s) {
        medium_event.pf = pf_sample(medium_event.medium.phase_function, wo, seed);
        medium_event.scattered = true;
        medium_event.transmittance = vec3(0.0);
    } 
    // Null
    else {

    }

    return medium_event;
}

MediumEvent ray_marching(vec3 origin, vec3 direction, float tmin, float tmax, inout uint seed) {
    MediumEvent medium_event = get_medium_event_empty();
    medium_event.medium = medium_get();
    medium_event.majorant = medium_event.medium.absorption + medium_event.medium.scattering;
    float t;
    float step_size = (tmax - tmin) * 0.01;

    for (int i = 0; i < 200; i++) {
    //while (true) {
        t = tmin + step_size;// + step_size * (random1D(seed) * 0.5 - 1.0);
        if (t < tmax) {
            vec3 position = origin + direction * t;

            vec3 jitter = vec3(0.0); //TODO (2.0 * random3D(seed) - vec3(1.0)) * volume.jittering_amount;
            medium_event.sampl = sample_medium(position, -direction, medium_event.medium, medium_event.majorant);
            medium_event.transmittance *= get_transmittance((t - tmin), (medium_event.sampl.sigma_a + medium_event.sampl.sigma_s));
            tmin = t;
        }
        else {
            // Reached end of ray
            vec3 position = origin + direction * tmax;

            medium_event.sampl = sample_medium(position, -direction, medium_event.medium, medium_event.majorant);
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

            vec3 jitter = vec3(0.0); //TODO (2.0 * random3D(seed) - vec3(1.0)) * volume.jittering_amount;
            medium_event = sample_medium_event(position + jitter, -direction, seed);

            if (medium_event.terminated || medium_event.scattered) {
                medium_event.t = t;
                break;
            }
            // Null scatter event
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

vec3 sample_transmittance_along_ray(vec3 origin, vec3 direction, float tmin, float tmax, inout uint seed, int transmittance_algo) {
    vec3 transmittance = vec3(1.0);

    if (current_medium >= 0) {
        if (medium_get().heterogeneous) {
            switch(transmittance_algo) {
                case 0: 
                    transmittance = delta_tracking(origin, direction, tmin, tmax, seed).transmittance;
                    break;
                case 1:
                    transmittance = ray_marching(origin, direction, tmin, tmax, seed).transmittance;
                    break;
            }
        } else {
            transmittance = get_transmittance(medium_get(), tmax - tmin);
        }
    }
    return transmittance;
}


MediumEvent sample_medium_along_ray(vec3 origin, vec3 direction, float t, inout uint seed) {
    vec3 transmittance = vec3(1.0);
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

    MediumEvent medium_event = delta_tracking(origin, direction, tmin, tmax, seed);

    // TODO separate into ray segments for tighter fitting majorants instead of a single broad one
    //while(!done) {
        //if (no more segments == 0) return transmittance;
        //if (majorant == 0) continue;
/*
        bool hit = pnanovdb_hdda_tree_marcher(
            volume_nano.density.grid_type,
            volume_nano.density.buf,
            volume_nano.density.accessor,
            origin,
            tmin,
            direction,
            tmax,
            t,
            unused
        );

        if (!hit) break;
        // Does pnanovdb_hdda_tree_marcher return t = 0.0 if it's inside a non zero grid or does it return the next one?
        // if the first do tmin = t
        tmin = t;
*/

/* ratio tracking attempt
        float transmittance_maj = 1.0;
        while (true) {
        //for (int i = 0; i < 100; i++) {

            t = tmin + sample_exponential(random1D(seed), medium.majorant);
            if (t < tmax) {
                vec3 position = origin + direction * t;

                vec3 jitter = random3D(seed) * volume.jittering_amount;
                medium_sample = sample_medium_event(position + jitter, -direction, seed); // TODO pass medium instead of majorant? where should medium be chosen and assigned?

                transmittance_maj *= get_transmittance(medium_sample.medium.absorption + medium_sample.medium.scattering, t - tmin);
                float transmittance_pdf = transmittance_maj * medium_sample.medium.majorant;
                transmittance *= transmittance_maj * medium_sample.sigma_n / transmittance_pdf; 
                // TODO russian roulette for ratio tracking
                
                // Apply russian roulette
                //if (is_ratio_tracking) {
                //    float probability = max(max(transmittance.r, transmittance.g), transmittance.b);
                //    if (probability < 0.75) {
                //        if (random1D(seed) > probability) {
                //            transmittance *= 0.0;
                //            break;
                //        }
                //        transmittance /= probability;
                //    }
                //}

                if (/*!is_ratio_tracking &&*//* (medium_sample.terminated || medium_sample.scattered)) {
                    transmittance = vec3(0.0, 0.0, 0.0);
                    medium_sample.t = t;
                    done = true;
                    break;
                }
                // Null scatter event, reset transmittance
                transmittance_maj = 1.0;
                transmittance = vec3(1.0);
                tmin = t;
            }
            else {
                //float dt = tmax - tmin;
                //transmittance_maj *= get_transmittance(medium.majorant, dt);
                //transmittance *= transmittance_maj;  // TODO ???
                transmittance = vec3(1.0);
                break;
            }
        }
    //}
*/
    if (to_remove_medium) {
        medium_remove();
    }
    return medium_event;
}

#endif //_MEDIUM_GLSL_