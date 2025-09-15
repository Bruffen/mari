#extension GL_GOOGLE_include_directive                      : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64    : require
#extension GL_EXT_scalar_block_layout                       : require
#extension GL_EXT_buffer_reference2                         : require
#extension GL_EXT_nonuniform_qualifier                      : require

#include "hitcommon.glsl"
#include "sampling.glsl"
#include "math.glsl"

layout(binding = 4, set = 0, scalar) uniform InfiniteLight {
    int      environment_id;
    vec2     environment_rotation;
    float    marginal_integral;
    uvec2    texture_size;
    uint64_t marginal_function_bda;
    uint64_t marginal_cdf_bda;
    uint64_t conditional_integrals_bda;
    uint64_t conditional_function_bda;
    uint64_t conditional_cdf_bda;
} infinite_light;

layout(binding = 5, set = 0, scalar) uniform Lights {
    uint64_t lights_bda;
    uint64_t function_bda;
    uint64_t cdf_bda;
    float    integral;
    int      size;
} lights;

struct LightInfo {
    int      type;
    vec3     positions[3];
    vec3     emission;
    float    power;
    float    area;
    int      double_sided;
};

const uint Light_Type_Area     = 2;
const uint Light_Type_Infinite = 3;

layout(buffer_reference, scalar) readonly buffer LightsBuffer { LightInfo l[]; };

struct LiSample {
    vec3  position;
    float distance;
    vec3  wi;
    vec3  radiance;
    float pdf;
};

LightInfo sample_light(float random, inout float pdf) {
    uint offset;

    sample_piecewise_constant_1D(
        lights.function_bda, 
        lights.cdf_bda, 
        lights.size, 
        lights.integral, 
        random, 
        pdf, 
        offset
    );
    pdf /= lights.size;

    LightsBuffer lights_buffer = LightsBuffer(lights.lights_bda);
    return lights_buffer.l[offset];
}

/*****************************************************************
 * Area Light
 *****************************************************************/

LiSample sample_Li_area(vec3 origin, vec2 random, LightInfo light) {
    LiSample li_sample;

    vec3 barycentric = sample_uniform_triangle(random); // TODO sample by solid angle?

    // TODO calculate and pass shading normal from cpu
    // TODO uvs and texture index for emissive textures
    vec3 normal_g = normalize(cross(light.positions[1] - light.positions[0], light.positions[2] - light.positions[0])); 


    li_sample.position = light.positions[0] * barycentric.x + light.positions[1] * barycentric.y + light.positions[2] * barycentric.z;
    li_sample.wi       = li_sample.position - origin;
    li_sample.distance = length(li_sample.wi);
    li_sample.wi      /= li_sample.distance;

    // Ensure pdf is zero when backface is sampled on a single sided light
    float ndotl = dot(normal_g, -li_sample.wi);
    float correct_side = 1.0;
    if (light.double_sided == 0 && ndotl < 0.0) {
        correct_side = 0.0;
    }

    li_sample.radiance = light.emission;
    li_sample.pdf      = sqr(li_sample.distance) / (light.area * abs(ndotl)) * correct_side;

    return li_sample;
}

// It would be nice to be able to index into the array of lights so as not to recalculate values.
// Would potentially have to pass light id in the primitive info, however.
float pdf_light_area(vec3 positions[3], vec3 wi, float distance, vec3 emission, bool double_sided) {
    vec3  normal_g = cross(positions[1] - positions[0], positions[2] - positions[0]);
    float normal_g_length = length(normal_g); 
    float area = normal_g_length * 0.5f;
    normal_g /= normal_g_length;

    float ndotl = dot(normal_g, -wi);

    float correct_side = 1.0;
    if (!double_sided && ndotl < 0.0) {
        correct_side = 0.0;
    }

    float power = length(emission) * /*area */ (double_sided ? 2.0f : 1.0f) * M_PI;
    float total_power = lights.integral * lights.size;

    return sqr(distance) * power / (/*area */ abs(ndotl) * total_power); // Areas cancel out
}

/*****************************************************************
 * Infinite Area Light
 *****************************************************************/

LiSample sample_Li_infinite(inout float pdf, vec2 random, LightInfo light) {
    LiSample li_sample;
    float piecewise_pdf;
    uvec2 offset;

    vec2 uv = sample_piecewise_constant_2D(
        infinite_light.marginal_function_bda, 
        infinite_light.marginal_cdf_bda, 
        infinite_light.marginal_integral, 
        infinite_light.conditional_function_bda, 
        infinite_light.conditional_cdf_bda, 
        infinite_light.conditional_integrals_bda,
        infinite_light.texture_size, 
        random, 
        piecewise_pdf, 
        offset
    );

    pdf *= piecewise_pdf;

    li_sample.radiance = vec3(1.0);
    if (infinite_light.environment_id > -1) {
        li_sample.radiance *= texture(textures[nonuniformEXT(infinite_light.environment_id)], uv).rgb;
    }

    vec3 dir = equal_area_square_to_sphere(uv);

    dir = rotate_around_axis(dir, vec3(0.0f, 0.0f, 1.0f), -infinite_light.environment_rotation.x);
    dir = rotate_around_axis(dir, vec3(0.0f, 1.0f, 0.0f), -infinite_light.environment_rotation.y);

    li_sample.wi = vec3(-dir.x, -dir.z, dir.y);
    li_sample.pdf = 1.0 / light.area;
    li_sample.distance = 10000.0;

    return li_sample;
}

vec3 sample_Le_infinite(vec3 direction, inout vec2 uv) {
    vec3 dir = vec3(-direction.x, direction.z, -direction.y);
    dir = rotate_around_axis(dir, vec3(0.0f, 0.0f, 1.0f), -infinite_light.environment_rotation.x);
    dir = rotate_around_axis(dir, vec3(0.0f, 1.0f, 0.0f), -infinite_light.environment_rotation.y);
    uv = equal_area_sphere_to_square(dir);

    vec3 color = vec3(1.0, 1.0, 1.0);
    if (infinite_light.environment_id > -1) {
        color *= texture(textures[nonuniformEXT(infinite_light.environment_id)], uv).rgb;
    }
    return color;
}

float pdf_light_infinite(vec2 uv) {
    if (lights.size <= 0) return 0.0;

    LightsBuffer lights_buffer = LightsBuffer(lights.lights_bda);
    LightInfo light = lights_buffer.l[lights.size - 1]; // Assumes infinite light is the last in the buffer

    if (light.type != Light_Type_Infinite) return 0.0;

    uv = clamp(uv, vec2(0.0), vec2(0.99999));
    uint offsety = uint(floor(uv.y * infinite_light.texture_size));
    uint offsetx = uint(floor(uv.x * infinite_light.texture_size));

    FunctionBuffer m_fb = FunctionBuffer(infinite_light.marginal_function_bda);
    float pdfy = m_fb.func[offsety] / infinite_light.marginal_integral;
    if (isnan(pdfy) || isinf(pdfy)) return 0.0;

    uint64_t byteOffset = 4 * offsety;
    FunctionBuffer c_fb = FunctionBuffer(infinite_light.conditional_function_bda + (byteOffset * uint64_t(infinite_light.texture_size.x)));
    float conditionalIntegral = ConditionalIntegralBuffer(infinite_light.conditional_integrals_bda).integrals[offsety];

    float pdfx = c_fb.func[offsetx] / conditionalIntegral;
    if (isnan(pdfx) || isinf(pdfx)) return 0.0;
    
    float totalPower = lights.integral * lights.size;
    return light.power * pdfx * pdfy / (light.area * totalPower);
}