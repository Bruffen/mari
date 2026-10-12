#ifndef _RAY_GEN_COMMON_GLSL_
#define _RAY_GEN_COMMON_GLSL_

//#ifdef MARI_DEBUG 
#extension GL_EXT_debug_printf : enable
//#endif

#ifdef SPECTRAL
vec3 wavelengths;
#include "common/spectral.glsl"
#endif
#include "common/material.glsl"
MaterialConstants material;
#include "common/bxdf.glsl"
#include "common/sampling.glsl"
#include "common/raycommon.glsl"

layout(location = 0) rayPayloadEXT Payload prd;

layout(binding = 0, set = 0)            uniform accelerationStructureEXT tlas;
layout(binding = 1, set = 0, rgba32f)   uniform image2D image;
layout(binding = 2, set = 0, rgba8)     uniform image2D present_image;
layout(binding = 3, set = 0, scalar)    uniform Properties {
    mat4  view_inverse;
    mat4  proj_inverse;
    int   frame_count;
    int   max_depth;
    float exposure;
    int   tonemapper;
    int   medium_integrator;
    int   transmittance_algo;
    bool  frame_accumulation;
    bool  russian_roulette;
    bool  next_event_estimation;
    int   samples_per_pixel;
    int   volume_instance_offset;
} properties;

#include "common/hitcommon.glsl"
#include "common/medium.glsl"
#include "common/tonemapping.glsl"
#include "common/light.glsl"


void initialize_rays(inout vec3 origin, inout vec3 direction) {
    const vec2 pixel_jitter = random2D(prd.seed);
    const vec2 pixel_center = vec2(gl_LaunchIDEXT.xy) + pixel_jitter;
    const vec2 s = (pixel_center / vec2(gl_LaunchSizeEXT.xy)) * 2.0 - 1.0;

    vec4 target =  properties.proj_inverse * vec4(s.x, s.y, 1, 1);
    origin      = (properties.view_inverse * vec4(0, 0, 0, 1)).xyz;
    direction   = (properties.view_inverse * vec4(normalize(target.xyz), 0)).xyz;
}

void accumulate_result(vec3 result) {
    result /= properties.samples_per_pixel;

    if (isnan(result.r) || isnan(result.g) || isnan(result.b)) result = ERROR_COLOR;
    if (isinf(result.r) || isinf(result.g) || isinf(result.b)) result = ERROR_COLOR;

    if (properties.frame_accumulation) {
        // Load existing accumulated color
        vec4 previous_color = vec4(0.0);
        if (properties.frame_count > 0) {
            previous_color = imageLoad(image, ivec2(gl_LaunchIDEXT.xy));
        }

        // Calculate new average color
        float avg = 1.0 / (properties.frame_count + 1.0);
        result = (result + previous_color.rgb * properties.frame_count) * avg;
        imageStore(image, ivec2(gl_LaunchIDEXT.xy), vec4(result, 1.0));
    }

    // Apply tone mapping
    vec3 tonemapped_color = apply_tonemapping(properties.tonemapper, result, properties.exposure);
    imageStore(present_image, ivec2(gl_LaunchIDEXT.xy), vec4(tonemapped_color, 1.0));
}

#endif //_RAY_GEN_COMMON_GLSL_