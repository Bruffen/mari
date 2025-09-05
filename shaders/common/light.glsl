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
    pdf /= lights.size;

    LightsBuffer lightsBuffer = LightsBuffer(lights.lightsBufferAddress);
    return lightsBuffer.l[offset];
}

/*****************************************************************
 * Area Light
 *****************************************************************/

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

// It would be nice to be able to index into the array of lights so as not to recalculate values.
// Would potentially have to pass light id in the primitive info, however.
float pdfLightArea(vec3 p0, vec3 p1, vec3 p2, vec3 wi, float distance, vec3 emission, bool doubleSided) {
    vec3  normal_g = cross(p1 - p0, p2 - p0);
    float normal_gLength = length(normal_g); 
    float area = normal_gLength * 0.5f;
    normal_g /= normal_gLength;

    float ndotl = dot(normal_g, -wi);

    float correctSide = 1.0;
    if (!doubleSided && ndotl < 0.0) {
        correctSide = 0.0;
    }

    float power = length(emission) * /*area */ (doubleSided ? 2.0f : 1.0f) * M_PI;
    float totalPower = lights.integral * lights.size;

    return sqr(distance) * power / (/*area */ abs(ndotl) * totalPower); // Areas cancel out
}

/*****************************************************************
 * Infinite Area Light
 *****************************************************************/

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

float pdfLightInfinite(vec2 uv) {
    if (lights.size <= 0) return 0.0;

    LightsBuffer lightsBuffer = LightsBuffer(lights.lightsBufferAddress);
    LightInfo light = lightsBuffer.l[lights.size - 1]; // Assumes infinite light is the last in the buffer

    if (light.type != LightType_Infinite) return 0.0;

    uv = clamp(uv, vec2(0.0), vec2(0.99999));
    uint offsety = uint(floor(uv.y * infiniteLight.textureSize));
    uint offsetx = uint(floor(uv.x * infiniteLight.textureSize));

    FunctionBuffer m_fb = FunctionBuffer(infiniteLight.marginalFunctionBufferAddress);
    float pdfy = m_fb.func[offsety] / infiniteLight.marginalIntegral;
    if (isnan(pdfy) || isinf(pdfy)) return 0.0;

    uint64_t byteOffset = 4 * offsety;
    FunctionBuffer c_fb = FunctionBuffer(infiniteLight.conditionalFunctionBufferAddress + (byteOffset * uint64_t(infiniteLight.textureSize.x)));
    float conditionalIntegral = ConditionalIntegralBuffer(infiniteLight.conditionalIntegralBufferAddress).integrals[offsety];

    float pdfx = c_fb.func[offsetx] / conditionalIntegral;
    if (isnan(pdfx) || isinf(pdfx)) return 0.0;
    
    float totalPower = lights.integral * lights.size;
    return light.power * pdfx * pdfy / (light.area * totalPower);
}