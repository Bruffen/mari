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
#include "common/hitcommon.glsl"
#include "common/tonemapping.glsl"
#include "common/light.glsl"

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
} properties;

#include "common/medium.glsl"

#endif //_RAY_GEN_COMMON_GLSL_