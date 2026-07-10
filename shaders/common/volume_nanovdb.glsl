#ifndef _VOLUME_NANOVDB_GLSL_
#define _VOLUME_NANOVDB_GLSL_

#include "material.glsl"

layout(binding = 6, set = 0, scalar) uniform Volume {
    uint64_t density_device_address;
    uint64_t temperature_device_address;
    Medium medium;
    float temperature_multiplier;
    float emissiveness_multiplier;
    float jittering_amount;
} volume;

layout(buffer_reference, scalar) readonly buffer PNanoVDBBuffer { uint p[]; };

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

#endif //_VOLUME_NANOVDB_GLSL_