#ifndef _VOLUME_GLSL_
#define _VOLUME_GLSL_

#include "math.glsl"
#include "sampling.glsl"

layout(binding = 6, set = 0, scalar) uniform Volume {
    uint64_t density_device_address;
    uint64_t temperature_device_address;
    vec4  albedo;
    float g;
    float sigma_a;
    float sigma_s;
    float temperature_multiplier;
    float emissiveness_multiplier;
    float jittering_amount;
} volume;

layout(buffer_reference, scalar) readonly buffer PNanoVDBBuffer { uint p[]; };
//uint pnanovdb_buf_data[] = PNanoVDBBuffer(volume.device_address).p;

#define PNANOVDB_GLSL
#include "PNanoVDB.h"

struct VolumeNanoData {
    pnanovdb_buf_t          buf;
    pnanovdb_grid_handle_t  grid;
    pnanovdb_grid_type_t    grid_type;
    pnanovdb_readaccessor_t accessor;
    vec3                    bbox_min;
    vec3                    bbox_max;
};

struct VolumeNano {
    VolumeNanoData density;
    VolumeNanoData temperature;
} volume_nano;


VolumeNanoData initialize_volume_nano_data(uint64_t device_address) {
    VolumeNanoData vnd;
    vnd.buf.device_address = device_address;
    vnd.grid.address.byte_offset = 0;
    vnd.grid_type = pnanovdb_buf_read_uint32(vnd.buf, PNANOVDB_GRID_OFF_GRID_TYPE);

    pnanovdb_tree_handle_t tree = pnanovdb_grid_get_tree(vnd.buf, vnd.grid);
	pnanovdb_root_handle_t root = pnanovdb_tree_get_root(vnd.buf, tree);
    pnanovdb_readaccessor_init(vnd.accessor, root);

    vnd.bbox_min = vec3(
        pnanovdb_grid_get_world_bbox(vnd.buf, vnd.grid, 0),
        pnanovdb_grid_get_world_bbox(vnd.buf, vnd.grid, 1),
        pnanovdb_grid_get_world_bbox(vnd.buf, vnd.grid, 2)
    );

    vnd.bbox_max = vec3(
        pnanovdb_grid_get_world_bbox(vnd.buf, vnd.grid, 3),
        pnanovdb_grid_get_world_bbox(vnd.buf, vnd.grid, 4),
        pnanovdb_grid_get_world_bbox(vnd.buf, vnd.grid, 5)
    );

    return vnd;
}

void initialize_volume_nano() {
    if (volume.density_device_address > 0) {
        volume_nano.density = initialize_volume_nano_data(volume.density_device_address);
    }
    if (volume.temperature_device_address > 0) {
        volume_nano.temperature = initialize_volume_nano_data(volume.temperature_device_address);
    }
}

vec2 sample_volume_nano(vec3 position) {
    vec2 data = vec2(0.0);

    if (volume.density_device_address > 0) {
        vec3 index_space_position = pnanovdb_grid_world_to_indexf(volume_nano.density.buf, volume_nano.density.grid, position);
        pnanovdb_coord_t ijk = pnanovdb_hdda_pos_to_ijk(index_space_position);
        pnanovdb_address_t address = pnanovdb_readaccessor_get_value_address(
            volume_nano.density.grid_type,
            volume_nano.density.buf, 
            volume_nano.density.accessor, 
            ijk
        );
        data.x = pnanovdb_read_float(volume_nano.density.buf, address);
    }

    if (volume.temperature_device_address > 0) {
        vec3 index_space_position = pnanovdb_grid_world_to_indexf(volume_nano.temperature.buf, volume_nano.temperature.grid, position);
        pnanovdb_coord_t ijk = pnanovdb_hdda_pos_to_ijk(index_space_position);
        pnanovdb_address_t address = pnanovdb_readaccessor_get_value_address(
            volume_nano.temperature.grid_type,
            volume_nano.temperature.buf, 
            volume_nano.temperature.accessor, 
            ijk
        );
        data.y = pnanovdb_read_float(volume_nano.temperature.buf, address);
    }

    return data;
}

struct Medium {
    vec3  absorption_color;
    float absorption;
    float scattering;
    float majorant;
    float g;
};

#define MAX_MEDIA 8
Medium media[MAX_MEDIA];
int current_medium = -1;

void medium_push(vec3 absorption_color, float absorption, float scattering, float majorant, float g) {
    if (current_medium >= MAX_MEDIA - 1) return;
    
    current_medium++;
    media[current_medium].absorption_color = absorption_color;
    media[current_medium].absorption = absorption;
    media[current_medium].scattering = scattering;
    media[current_medium].majorant   = majorant;
    media[current_medium].g          = g;
}

void medium_push(Medium medium) {
    medium_push(
        medium.absorption_color,
        medium.absorption,
        medium.scattering,
        medium.majorant,
        medium.g
    );
}

Medium medium_get() {
    return media[current_medium];
}

void medium_remove() {
    if (current_medium >= 0) current_medium--;
}

vec3 get_transmittance(Medium medium, float distance) {
    float attenuation = medium.absorption + medium.scattering;
    if (attenuation <= 0.0) return vec3(1.0);

    // if null-scattering coefficient == 0 (medium is homogenous) ???
    // apply beer's law directly
    //return pow(absorption_color, vec3(distance / attenuation));
    return exp(-attenuation * (vec3(1.0) - medium.absorption_color) * distance);

    // else
    // delta tracking
}

float get_transmittance(float majorant, float distance) {
    if (majorant <= 0.0) return 1.0;
    return exp(-majorant * distance);
}

struct PhaseFunctionSample {
    vec3  wi;
    float p;
    float pdf;
};

PhaseFunctionSample pf_isotropic_sample(vec2 random) {
    return PhaseFunctionSample(
        sample_uniform_sphere(random.x, random.y), 
        pdf_uniform_sphere(),
        pdf_uniform_sphere()
    );
}

// TODO pf_isotropic_p()
// TODO pf_isotropic_pdf()

float pf_henyey_greenstein(float cos_theta, float g) {
    float denom = 1.0 + sqr(g) + 2.0 * g * cos_theta;
    return M_1_4PI * (1.0 - sqr(g)) / (denom * safe_sqrt(denom));
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

float pf_henyey_greenstein_p(vec3 wo, vec3 wi, float g) {
    return pf_henyey_greenstein(dot(wo, wi), g);
}

float pf_henyey_greenstein_pdf(vec3 wo, vec3 wi, float g) {
    return pf_henyey_greenstein_p(wo, wi, g);
}

struct MediumSample {
    float sigma_a;
    float sigma_s;
    vec3  albedo;
    vec3  emission;
};

struct MediumEvent {
    PhaseFunctionSample pf;
    MediumSample sampl;
    Medium medium;
    float t;
    float n_a;
    float n_s;
    float n_n;
    vec3  transmittance;
    bool  terminated;
    bool  scattered;
};

MediumSample sample_medium(vec3 position, vec3 wo, Medium medium) {
    vec2 sample_data  = sample_volume_nano(position);
	float density     = sample_data.x;
	float temperature = max(sample_data.y * volume.temperature_multiplier, 0.0);
    float intensity   = M_stefan_boltzmann * pow(temperature, 4) * volume.emissiveness_multiplier;

    MediumSample medium_sample;
    medium_sample.sigma_a  = clamp(medium.absorption * density, 0.0, medium.majorant);
    medium_sample.sigma_s  = clamp(medium.scattering * density, 0.0, medium.majorant);
    medium_sample.albedo   = medium.absorption_color;
    medium_sample.emission = blackbody(temperature) * intensity;

    return medium_sample;
}

MediumEvent sample_medium_event(vec3 position, vec3 wo, inout uint seed) {
    MediumEvent medium_event;
    medium_event.medium        = medium_get();
    medium_event.t             = 0.0;
    medium_event.sampl         = sample_medium(position, wo, medium_event.medium);
    medium_event.pf            = PhaseFunctionSample(vec3(0.0), 1, 1);
    medium_event.transmittance = vec3(1.0);
    medium_event.terminated    = false;
    medium_event.scattered     = false;
    medium_event.n_a           = medium_event.sampl.sigma_a / medium_event.medium.majorant;
    medium_event.n_s           = medium_event.sampl.sigma_s / medium_event.medium.majorant;
    medium_event.n_n           = max(0.0, 1.0 - medium_event.n_a - medium_event.n_s);

    float event = random1D(seed);

    // Absorption
    if (event < medium_event.n_a) {                   
        medium_event.terminated = true;
        medium_event.transmittance = vec3(0.0);
    } 
    // Scattering
    else if (event < medium_event.n_a + medium_event.n_s) {
        vec2 r = random2D(seed);

        //medium_event.pf = pf_isotropic_sample(r);
        medium_event.pf = pf_henyey_greenstein_sample(wo, medium_event.medium.g, r);
        medium_event.scattered = true;
        medium_event.transmittance = vec3(0.0);
    } 
    // Null
    else {

    }

    return medium_event;
}

MediumEvent delta_tracking(vec3 origin, vec3 direction, float tmax, inout uint seed) {
    MediumEvent medium_event;
    medium_event.medium        = medium_get();
    medium_event.t             = 0.0;
    medium_event.sampl         = MediumSample(0, 0, vec3(0), vec3(0));
    medium_event.pf            = PhaseFunctionSample(vec3(0.0), 1, 1);
    medium_event.transmittance = vec3(1.0);
    medium_event.terminated    = false;
    medium_event.scattered     = false;
    medium_event.n_a           = 0.0;
    medium_event.n_s           = 0.0;
    medium_event.n_n           = 0.0;
    float tmin = 0.0;
    float t = 0.0;

    while (true) {
        t = tmin + sample_exponential(random1D(seed), medium_event.medium.majorant);
        if (t < tmax) {
            vec3 position = origin + direction * t;

            vec3 jitter = (2.0 * random3D(seed) - vec3(1.0)) * volume.jittering_amount;
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

MediumEvent sample_medium_along_ray(vec3 origin, vec3 direction, float tmax, inout uint seed) {
    vec3 transmittance = vec3(1.0);
    direction = normalize(direction);

    float tmin = 0.0;
    float t = 0.0;
    float unused = 0.0;
    bool done = false;

    float tmax_surface = tmax;// - 0.001;
    bool hit = pnanovdb_hdda_ray_clip(
        volume_nano.density.bbox_min,
        volume_nano.density.bbox_max,
        origin,
        tmin,
        direction,
        tmax // TODO test tmax actually gets updated here
    );
    tmax = min(tmax_surface, tmax);

    if (!hit || tmin > tmax) {
        // TODO don't terminate since there can be a bigger non vdb medium, like fog, that needs to be sampled
        MediumEvent medium_event;
        medium_event.medium        = Medium(vec3(0), 0, 0, 0, 0);
        medium_event.t             = tmax;
        medium_event.sampl         = MediumSample(0, 0, vec3(0), vec3(0));
        medium_event.pf            = PhaseFunctionSample(vec3(0.0), 1, 1);
        medium_event.transmittance = vec3(1.0);
        medium_event.terminated    = false;
        medium_event.scattered     = false;
        medium_event.n_a           = 0.0;
        medium_event.n_s           = 0.0;
        medium_event.n_n           = 0.0;
        return medium_event; 
    }

    // Push volume_nano data into array of media
    Medium medium;
    medium.absorption_color = volume.albedo.rgb;
    medium.absorption = volume.sigma_a;
    medium.scattering = volume.sigma_s;
    medium.g = volume.g;
    medium.majorant = medium.absorption + medium.scattering;
    medium_push(medium);

    MediumEvent medium_event = delta_tracking(origin, direction, tmax, seed);

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
    medium_remove();
    return medium_event;
}

#endif //_VOLUME_GLSL_