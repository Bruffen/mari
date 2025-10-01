#ifndef _VOLUME_GLSL_
#define _VOLUME_GLSL_

#include "math.glsl"
#include "sampling.glsl"

layout(binding = 6, set = 0, scalar) uniform Volume {
    uint64_t device_address;
    float g;
    float sigma_a;
    float sigma_s;
} volume;

layout(buffer_reference, scalar) readonly buffer PNanoVDBBuffer { uint p[]; };
//uint pnanovdb_buf_data[] = PNanoVDBBuffer(volume.device_address).p;

#define PNANOVDB_GLSL
#include "PNanoVDB.h"

struct VolumeNano {
    pnanovdb_buf_t          buf;
    pnanovdb_grid_handle_t  grid;
	pnanovdb_grid_type_t    grid_type;
	pnanovdb_readaccessor_t accessor;
    vec3                    bbox_min;
    vec3                    bbox_max;
} volume_nano;


void initialize_volume_nano() {
    volume_nano.buf.unused = 0; // Unused because we create a buffer from device address
    volume_nano.grid.address.byte_offset = 0;
    volume_nano.grid_type = pnanovdb_buf_read_uint32(volume_nano.buf, PNANOVDB_GRID_OFF_GRID_TYPE);

    pnanovdb_tree_handle_t tree = pnanovdb_grid_get_tree(volume_nano.buf, volume_nano.grid);
	pnanovdb_root_handle_t root = pnanovdb_tree_get_root(volume_nano.buf, tree);
    pnanovdb_readaccessor_init(volume_nano.accessor, root);

    volume_nano.bbox_min = pnanovdb_root_get_bbox_min(volume_nano.buf, root);
    volume_nano.bbox_max = pnanovdb_root_get_bbox_max(volume_nano.buf, root);
}

float sample_volume_nano(vec3 position) {
    vec3 index_space_position = pnanovdb_grid_world_to_indexf(volume_nano.buf, volume_nano.grid, position);
    pnanovdb_coord_t ijk = pnanovdb_hdda_pos_to_ijk(index_space_position);
	pnanovdb_address_t address = pnanovdb_readaccessor_get_value_address(
        volume_nano.grid_type,
        volume_nano.buf, 
        volume_nano.accessor, 
        ijk
    );

    return pnanovdb_read_float(volume_nano.buf, address);
}

struct Medium {
    vec3 absorption_color;
    float absorption;
    float scattering;
    float majorant;
    float g;
};

#define MAX_MEDIA 4
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
    PhaseFunctionSample pf;
    float t;
    vec3  emission;
    vec3  transmittance;
    bool  terminated;
    bool  scattered;
};

MediumSample sample_medium_event(vec3 position, vec3 wo, float majorant, inout uint seed) { // TODO remove medium from arguments
    Medium medium;
    //if (length(position) < 1.0) {
        medium = media[current_medium];
    //} else {
    //    medium.absorption = 0.0;
    //    medium.scattering = 0.0;
    //    medium.majorant = majorant;
    //}

	float density = sample_volume_nano(position);
    medium.absorption = clamp(medium.absorption * density, 0.0, medium.majorant);
    medium.scattering = clamp(medium.scattering * density, 0.0, medium.majorant);
    
    float absorb = medium.absorption / majorant;
    float scatter = medium.scattering / majorant;
    float null = max(0.0, 1.0 - absorb - scatter);

    MediumSample medium_sample;
    medium_sample.t             = 0.0;
    medium_sample.pf            = PhaseFunctionSample(vec3(0.0), 1, 1);
    medium_sample.emission      = vec3(0.0);
    medium_sample.transmittance = vec3(0.0);
    medium_sample.terminated    = false;
    medium_sample.scattered     = false;

    float event = random1D(seed);
    // Absorption
    if (event < absorb) {
        medium_sample.emission += vec3(0.0); // TODO medium emission
        medium_sample.terminated = true;
    // Scattering
    } else if (event < absorb + scatter) {
        //if (depth == properties.max_depth - 1) break;

        vec2 r = random2D(seed);

        medium_sample.pf = pf_henyey_greenstein_sample(wo, medium.g, r);
        //medium_sample.pf.wi = sample_uniform_sphere(r.x, r.y); 
        //medium_sample.pf.p = pdf_uniform_sphere();
        //medium_sample.pf.pdf = pdf_uniform_sphere();
        medium_sample.scattered = true;
    // Null
    } else {

    }

    return medium_sample;
}

MediumSample sample_medium_along_ray(vec3 origin, vec3 direction, float tmax, float majorant, inout uint seed) {
    vec3 transmittance = vec3(1.0);
    direction = normalize(direction);

    MediumSample medium_sample;
    medium_sample.terminated = false;
    medium_sample.scattered = false;
    medium_sample.transmittance = transmittance;

    // TODO flipping Y axis for now, but should implement matrix transformation instead
    origin.y *= -1.0;
    direction.y *= -1.0;

    float tmin = 0.0;
    float t = 0.0;
    float unused = 0.0;
    bool done = false;

    pnanovdb_hdda_ray_clip(
        volume_nano.bbox_min,
        volume_nano.bbox_max,
        origin,
        tmin,
        direction,
        tmax
    );

    // TODO separate into ray segments for tighter fitting majorants instead of a single broad one
    //while(!done) {
        //if (no more segments == 0) return transmittance;
        //if (majorant == 0) continue;
/*
        bool hit = pnanovdb_hdda_tree_marcher(
            volume_nano.grid_type,
            volume_nano.buf,
            volume_nano.accessor,
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

        while (true) {
        //for (int i = 0; i < 100; i++) {

            t = tmin + sample_exponential(random1D(seed), majorant);
            if (t < tmax) {
                vec3 position = origin + direction * t;
                transmittance *= 1.0; // TODO get_transmittance(majorant, t - tmin);
                medium_sample = sample_medium_event(position, -direction, majorant, seed);
                if (medium_sample.terminated || medium_sample.scattered) {
                    medium_sample.t = t;
                    done = true;
                    break;
                }
                // Null scatter event, reset transmittance
                transmittance = vec3(1.0);
                tmin = t;
            }
            else {
                float dt = tmax - tmin;
                transmittance *= 1.0; // TODO get_transmittance(majorant, dt);
                break;
            }
        }
    //}

    medium_sample.transmittance = transmittance;
    return medium_sample;
}

#endif //_VOLUME_GLSL_