#include "random.glsl"
#include "complex.glsl"
#include "math.glsl"

float fresnelDielectric(float cosTheta_i, float eta) {
    cosTheta_i = clamp(cosTheta_i, -1.0, 1.0);
    // Exiting the material
    if (cosTheta_i < 0.0) {
        eta = 1.0 / eta;
        cosTheta_i = -cosTheta_i;
    }

    // Snell's law
    float sin2Theta_i = 1.0 - sqr(cosTheta_i);
    float sin2Theta_t = sin2Theta_i / sqr(eta);
    // Handle total internal reflection
    if (sin2Theta_t >= 1.0) return 1.0;

    float cosTheta_t = safeSqrt(1.0 - sin2Theta_t);

    float rParl = (eta * cosTheta_i - cosTheta_t) / (eta * cosTheta_i + cosTheta_t);
    float rPerp = (cosTheta_i - eta * cosTheta_t) / (cosTheta_i + eta * cosTheta_t);
    return (sqr(rParl) + sqr(rPerp)) * 0.5;
}

float fresnelComplex(float cosTheta_i, Complex eta) {
    cosTheta_i = clamp(cosTheta_i, 0.0, 1.0);

    // Snell's law
    float sin2Theta_i = 1.0 - sqr(cosTheta_i);
    Complex sin2Theta_t = Complex_divide(sin2Theta_i, (Complex_sqr(eta)));
    Complex cosTheta_t = Complex_sqrt(Complex_subtract(1.0, sin2Theta_t));

    Complex etaMcosTheta_i = Complex_multiply(eta, cosTheta_i);
    Complex rParl = Complex_divide(Complex_subtract(etaMcosTheta_i, cosTheta_t), Complex_add(etaMcosTheta_i, cosTheta_t));
    Complex etaMcosTheta_t = Complex_multiply(eta, cosTheta_t);
    Complex rPerp = Complex_divide(Complex_subtract(cosTheta_i, etaMcosTheta_t), Complex_add(cosTheta_i, etaMcosTheta_t));
    return (Complex_norm(rParl) + Complex_norm(rPerp)) * 0.5;
}

// TODO spectral fresnel complex with k function


/****************************************************************
 * Shading frame to world frame transformations
 ****************************************************************
 */
 
vec3 toLocal(vec3 t, vec3 b, vec3 n, vec3 v) {
    return vec3(dot(v, t), dot(v, b), dot(v, n));
}

vec3 fromLocal(vec3 t, vec3 b, vec3 n, vec3 v) {
    return v.x * t + v.y * b + v.z * n;
}

vec3 fromLocal(vec4 tangent, vec3 normal, vec3 v) {
    vec3 bitangent = cross(normal, tangent.xyz) * tangent.w;
    return fromLocal(tangent.xyz, bitangent, normal, v);
}

/****************************************************************
 * Trowbridge-Reitz Model functions
 ****************************************************************
 */

float TR_D(vec3 wm) {
    float alpha_x = material.roughness; // TODO anisotropy
    float alpha_y = material.roughness; // TODO anisotropy

    float tan2t = tan2Theta(wm);
    if (isinf(tan2t)) return 0.0;

    float cos4t = sqr(cos2Theta(wm));
    if (cos4t < 1e-16) return 0.0;

    float e = tan2t * (sqr(cosPhi(wm) / alpha_x) + sqr(sinPhi(wm) / alpha_y));
    return 1.0 / (M_PI * alpha_x * alpha_y * cos4t * sqr(1.0 + e));
}

float TR_Lambda(vec3 w) {
    float alpha_x = material.roughness; // TODO anisotropy
    float alpha_y = material.roughness; // TODO anisotropy

    float tan2t = tan2Theta(w);
    if (isinf(tan2t)) return 0.0;

    float alpha2 = sqr(cosPhi(w) * alpha_x) + sqr(sinPhi(w) * alpha_y);
    return (sqrt(1 + alpha2 * tan2t) - 1.0) / 2.0;
}

float TR_G1(vec3 w) {
    return 1.0 / (1.0 + TR_Lambda(w));
}

float TR_G(vec3 wo, vec3 wi) {
    return 1.0 / (1.0 + TR_Lambda(wo) + TR_Lambda(wi));
}

float TR_D(vec3 w, vec3 wm) {
    return TR_G1(w) / absCosTheta(w) * TR_D(wm) * abs(dot(w, wm));
}

float TR_PDF(vec3 w, vec3 wm) { return TR_D(w, wm); }

vec3 TR_Sample(vec3 w, vec2 random) {
    float alpha_x = material.roughness; // TODO anisotropy
    float alpha_y = material.roughness; // TODO anisotropy

    // Transform w to hemispherical configuration
    vec3 wh = normalize(vec3(alpha_x * w.x, alpha_y * w.y, w.z));
    if (wh.z < 0.0) wh = -wh;

    // Find orthonormal basis for visible normal sampling
    vec3 t1 = wh.z < 0.99999 ? normalize(cross(vec3(0, 0, 1), wh)) : vec3(1, 0, 0);
    vec3 t2 = cross(wh, t1);

    // Generate uniformly distributed points on the unit disk
    vec2 p = sampleUniformDiskPolar(random.x, random.y);

    // Warp hemispherical projection for visible normal sampling
    float h = sqrt(1.0 - sqr(p.x));
    p.y = mix(h, p.y, (1.0 + wh.z) / 2.0);
    
    // Reproject to hemisphere and transform normal to ellipsoid configuration
    float pz = sqrt(max(0.0, 1.0 - (sqr(p.x) + sqr(p.y))));
    vec3 nh = p.x * t1 + p.y * t2 + pz * wh;
    return normalize(vec3(alpha_x * nh.x, alpha_y * nh.y, max(1e-6, nh.z)));
}

struct BsdfSample {
    vec3 wi;
    float f;
    float pdf;
    bool failed;
};

BsdfSample sampleFail() {
    return BsdfSample(vec3(0), 0, 0, true);
}

/****************************************************************
 * Diffuse
 ****************************************************************
 */

BsdfSample brdfDiffuseSample(vec3 wo, vec2 random) {
    vec3 wi = sampleCosineHemisphere(random.x, random.y);
    if (wo.z < 0.0) wi.z *= -1;
    float pdf = pdfCosineHemisphere(absCosTheta(wi));
    float f =  1.0 / M_1_PI;

    return BsdfSample(wi, f, pdf, false);
}

float brdfDiffuseF(vec3 wo, vec3 wi) {
    float r = 1.0;
    if (!sameHemisphere(wo, wi)) return 0.0;
    return r * M_1_PI;  // TODO what to do with r;
}

float brdfDiffusePDF(vec3 wo, vec3 wi) {
    if (!sameHemisphere(wo, wi)) return 0.0;
    return pdfCosineHemisphere(absCosTheta(wi));
}

/****************************************************************
 * Conductor
 ****************************************************************
 */

BsdfSample brdfConductorSample(vec3 wo, vec2 random) {
    float eta = material.ior;

    if (material.roughness == 0.0) {
        vec3 wi = vec3(-wo.x, -wo.y, wo.z);
        float act = absCosTheta(wi);
        float f = fresnelComplex(act, Complex(eta, 3.0)/* TODO conductors have a spectrally varying absorption coefficient k */) / act;
        return BsdfSample(wi, f, 1.0, false);
    }

    vec3 wm = TR_Sample(wo, random);
    vec3 wi = mReflect(wo, wm);
    if (!sameHemisphere(wo, wi)) return sampleFail();
    float pdf = TR_PDF(wo, wm) / (4.0 * abs(dot(wo, wm)));
    
    float cosTheta_o = absCosTheta(wo);
    float cosTheta_i = absCosTheta(wi);
    float fresnel = fresnelComplex(abs(dot(wo, wm)), Complex(eta, 3.0)/* TODO conductors have a spectrally varying absorption coefficient k */);
    float f = TR_D(wm) * fresnel * TR_G(wo, wi) / (4.0 * cosTheta_i * cosTheta_o);

    return BsdfSample(wi, f, pdf, false);
}

float brdfConductorF(vec3 wo, vec3 wi) {
    if (!sameHemisphere(wo, wi)) return 0.0;
    if (material.roughness == 0.0) return 0.0;

    // Evaluate rough conductor brdf
    // Compute cosines and wm for conductor brdf
    float cosTheta_o = absCosTheta(wo);
    float cosTheta_i = absCosTheta(wi);
    if (cosTheta_o == 0.0 || cosTheta_i == 0.0) return 0.0;

    vec3 wm = wi + wo;
    if (lengthSquared(wm) == 0.0) return 0.0;
    wm = normalize(wm);
    // Evaluate fresnel  factor for conductor brdf
    float fresnel = fresnelComplex(abs(dot(wo, wm)), Complex(material.ior, 3.0)/*, 3.0 TODO conductors have a spectrally varying absorption coefficient k */);
    return TR_D(wm) * fresnel * TR_G(wo, wi) / (4.0 * cosTheta_i * cosTheta_o);
}

float brdfConductorPDF(vec3 wo, vec3 wi) {
    if (!sameHemisphere(wo, wi)) return 0.0;
    if (material.roughness == 0.0) return 0.0;

    vec3 wm = wo + wi;
    if (lengthSquared(wm) == 0.0) return 0.0;
    wm = faceforward(normalize(wm), vec3(0, 0, 1), wm);
    return TR_PDF(wo, wm) / (4.0 * abs(dot(wo, wm)));
}


/****************************************************************
 * Dielectric
 ****************************************************************
 */

BsdfSample bsdfDielectricSample(vec3 wo, vec3 random) {
    float eta = material.ior;

    // Perfectly specular 
    if (eta == 1.0 || material.roughness == 0.0) {
        float r = fresnelDielectric(cosTheta(wo), eta);
        float t = 1.0 - r;
        float probability_reflection = r / (r + t);
        // Sample brdf
        if (random.z < probability_reflection) { 
            vec3 wi = vec3(-wo.x, -wo.y, wo.z);
            float f = r / absCosTheta(wi);
            return BsdfSample(wi, f, probability_reflection, false);

        // Sample btdf 
        } else {
            float etap = -1.0;
            vec3 wt = mRefract(wo, vec3(0, 0, 1), eta, etap);
            if (etap == -1.0) return sampleFail();
            float probability_transmission = t / r + t;
            float ft = t / absCosTheta(wt);
            // TODO account for non symmetry between camera paths and light paths

            return BsdfSample(wt, ft, probability_transmission, false);
        }
    }

    // Rough specular
    vec3 wm = TR_Sample(wo, random.xy);
    float r = fresnelDielectric(dot(wo, wm), eta);
    float t = 1.0 - r;
    float probability_reflection = r / (r + t);
    
    float pdf;
    // Sample brdf
    if (random.z < probability_reflection) {
        vec3 wi = mReflect(wo, wm);
        if (!sameHemisphere(wo, wi)) return sampleFail();
        pdf = TR_PDF(wo, wm) / (4.0 * abs(dot(wo, wm)) * probability_reflection);
        float f = TR_D(wm) * TR_G(wo, wi) * r / (4.0 * cosTheta(wi) * cosTheta(wo));
        return BsdfSample(wi, f, pdf, false);

    // Sample btdf
    } else {
        float etap = -1.0;
        vec3 wt = mRefract(wo, wm, eta, etap);
        if (sameHemisphere(wo, wt) || wt.z == 0 || etap == -1.0) return sampleFail();
        float probability_transmission = t / (r + t);
        float denom = sqr(dot(wt, wm) + dot(wo, wm) / etap);
        float dwm_dwt = abs(dot(wt, wm)) / denom;
        pdf = TR_PDF(wo, wm) * dwm_dwt * probability_transmission;
        float ft = t * TR_D(wm) * TR_G(wo, wt) * abs(dot(wt, wm) * dot(wo, wm) / (cosTheta(wt) * cosTheta(wo) * denom));
        // TODO account for non symmetry between camera paths and light paths

        return BsdfSample(wt, ft, pdf, false);
    }
}

float bsdfDielectricF(vec3 wo, vec3 wi) {
    float eta = material.ior;
    
    if (eta == 1.0 || material.roughness == 0.0) {
        return 0.0;
    }

    // Compute generalized half vector
    float cosTheta_o = cosTheta(wo);
    float cosTheta_i = cosTheta(wi);
    bool reflect = cosTheta_o * cosTheta_i > 0.0;

    float etap = 1.0;
    if (!reflect) {
        etap = cosTheta_o > 0.0 ? eta : 1.0 / eta;
    }

    vec3 wm = wi * etap + wo;
    if (cosTheta_i == 0 || cosTheta_o == 0 ||  lengthSquared(wm) == 0) return 0.0;  // TODO return 0? need to handle discards
    wm = faceforward(normalize(wm), vec3(0, 0, 1), wm);

    // Discard backfacing microfacets
    if (dot(wm, wi) * cosTheta_i < 0 || dot(wm, wo) * cosTheta_o < 0) {
        return 0.0; // TODO return 0? need to handle discards
    }

    float fresnel = fresnelDielectric(dot(wo, wm), eta);
    if (reflect) {
        // Compute reflection at rough dielectric interface
        return TR_D(wm) * TR_G(wo, wi) * fresnel / abs(4.0 * cosTheta_i * cosTheta_o);
    }
    else {
        // Compute transmission at rough dielectric interface
        float denom = sqr(dot(wi, wm) + dot(wo, wm) / etap) * cosTheta_i * cosTheta_o;
        float ft = TR_D(wm) * TR_G(wo, wi) * (1.0 - fresnel) * abs(dot(wi, wm) * dot(wo, wm) / denom);
        // TODO account for non symmetry between camera paths and light paths
        return ft;
    }
}

float bsdfDielectricPDF(vec3 wo, vec3 wi) {
    float eta = material.ior;

    // Perfectly specular 
    if (eta == 1.0 || material.roughness == 0.0) {
        return 0.0;
    }

    // Rough specular
    // Compute generalized half vector
    float cosTheta_o = cosTheta(wo);
    float cosTheta_i = cosTheta(wi);
    bool reflect = cosTheta_o * cosTheta_i > 0.0;

    float etap = 1.0;
    if (!reflect) {
        etap = cosTheta_o > 0.0 ? eta : 1.0 / eta;
    }

    vec3 wm = wi * etap + wo;
    wm = faceforward(normalize(wm), vec3(0, 0, 1), wm);

    // Discard backfacing microfacets
    if (dot(wm, wi) * cosTheta_i < 0 || dot(wm, wo) * cosTheta_o < 0) {
        return 0.0;
    }

    // Determine Fresnel reflectance of rough dielectric boundary
    float r = fresnelDielectric(dot(wo, wm), eta);
    float t = 1.0 - r;

    // Return pdf for rough dielectric
    // pdf of rough reflection
    if (reflect) {
        float probability_reflection = r / (r + t);
        return TR_PDF(wo, wm) / (4.0 * abs(dot(wo, wm)) * probability_reflection);

    // pdf of rough transmission
    } else {
        float probability_transmission = t / (r + t);
        float denom = sqr(dot(wi, wm) + dot(wo, wm) / etap);
        float dwm_dwi = abs(dot(wi, wm)) / denom;
        return TR_PDF(wo, wm) * dwm_dwi * probability_transmission;
    }
}

// TODO thin dielectric bsdf

