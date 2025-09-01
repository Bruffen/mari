#extension GL_GOOGLE_include_directive                      : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64    : require
#extension GL_EXT_scalar_block_layout                       : require
#extension GL_EXT_buffer_reference2                         : require

#include "sampling.glsl"
#include "math.glsl"

layout(binding = 4, set = 0, scalar) uniform InfiniteLight {
    int environmentID;
    vec2 environmentRotation;
    float marginalIntegral;
    uvec2 textureSize;
    uint64_t marginalFunctionBufferAddress;
    uint64_t marginalCdfBufferAddress;
    uint64_t conditionalIntegralBufferAddress;
    uint64_t conditionalFunctionBufferAddress;
    uint64_t conditionalCdfBufferAddress;
} infiniteLight;

layout(binding = 5, set = 0, scalar) uniform Lights {
    uint64_t lightsBufferAddress;
    uint64_t functionBufferAddress;
    uint64_t cdfBufferAddress;
    float    integral;
    int      size;
} lights;

struct LightInfo {
    int     type;
    vec3    positions[3];
    vec3    emission;
    float   power;
    float   area;
    int     doubleSided;
};

const uint LightType_Area       = 2;
const uint LightType_Infinite   = 3;

layout(buffer_reference, scalar) readonly buffer LightsBuffer { LightInfo l[]; };

struct LiSample {
    vec3  position;
    float distance;
    vec3  wi;
    vec3  radiance;
    float pdf;
};

LightInfo sampleLight(float random, inout float pdf) {
    uint offset;

    samplePiecewiseConstant1D(
        lights.functionBufferAddress, 
        lights.cdfBufferAddress, 
        lights.size, 
        lights.integral, 
        random, 
        pdf, 
        offset
    );

    LightsBuffer lightsBuffer = LightsBuffer(lights.lightsBufferAddress);
    return lightsBuffer.l[offset];
}

LiSample sampleLiArea(vec3 origin, vec2 random, LightInfo light) {
    LiSample liSample;

    vec3 barycentric = sampleUniformTriangle(random); // TODO sample by solid angle?

    // TODO calculate and pass shading normal from cpu
    // TODO uvs and texture index for emissive textures
    vec3 normal_g = normalize(cross(light.positions[1] - light.positions[0], light.positions[2] - light.positions[0])); 


    liSample.position = light.positions[0] * barycentric.x + light.positions[1] * barycentric.y + light.positions[2] * barycentric.z;
    liSample.wi       = liSample.position - origin;
    liSample.distance = length(liSample.wi);
    liSample.wi      /= liSample.distance;

    // Ensure pdf is zero when backface is sampled on a single sided light
    float ndotl = dot(normal_g, -liSample.wi);
    float correctSide = 1.0;
    if (light.doubleSided == 0 && ndotl < 0.0) {
        correctSide = 0.0;
    }

    liSample.radiance = light.emission;
    liSample.pdf      = liSample.distance * liSample.distance / (light.area * abs(ndotl)) * correctSide;

    return liSample;
}

LiSample sampleLiInfinite(inout float pdf, vec2 random, LightInfo light) {
    LiSample liSample;
    float piecewise_pdf;
    uvec2 offset;

    vec2 uv = samplePiecewiseConstant2D(
        infiniteLight.marginalFunctionBufferAddress, 
        infiniteLight.marginalCdfBufferAddress, 
        infiniteLight.marginalIntegral, 
        infiniteLight.conditionalFunctionBufferAddress, 
        infiniteLight.conditionalCdfBufferAddress, 
        infiniteLight.conditionalIntegralBufferAddress,
        infiniteLight.textureSize, 
        random, 
        piecewise_pdf, 
        offset
    );

    pdf *= piecewise_pdf;

    liSample.radiance = vec3(1.0);
    if (infiniteLight.environmentID > -1) {
        liSample.radiance *= texture(textures[nonuniformEXT(infiniteLight.environmentID)], uv).rgb;
    }

    vec3 dir = equalAreaSquareToSphere(uv);

    dir = rotateAroundAxis(dir, vec3(0.0f, 0.0f, 1.0f), -infiniteLight.environmentRotation.x);
    dir = rotateAroundAxis(dir, vec3(0.0f, 1.0f, 0.0f), -infiniteLight.environmentRotation.y);

    liSample.wi = vec3(-dir.x, -dir.z, dir.y);
    liSample.pdf = 1.0 / light.area;
    liSample.distance = 10000.0;

    return liSample;
}