#include "sampling.glsl"
#include "math.glsl"

#define Transport_Mode_Radiance   0 // Camera paths
#define Transport_Mode_Importance 1 // Light paths

/****************************************************************
 * Trowbridge-Reitz Model functions
 ****************************************************************
 */

float TR_D(vec3 wm) {
    float alpha_x = material.roughness; // TODO anisotropy
    float alpha_y = material.roughness; // TODO anisotropy

    float tan2t = tan2_theta(wm);
    if (isinf(tan2t)) return 0.0;

    float cos4t = sqr(cos2_theta(wm));
    if (cos4t < 1e-16) return 0.0;

    float e = tan2t * (sqr(cos_phi(wm) / alpha_x) + sqr(sin_phi(wm) / alpha_y));
    return 1.0 / (M_PI * alpha_x * alpha_y * cos4t * sqr(1.0 + e));
}

float TR_Lambda(vec3 w) {
    float alpha_x = material.roughness; // TODO anisotropy
    float alpha_y = material.roughness; // TODO anisotropy

    float tan2t = tan2_theta(w);
    if (isinf(tan2t)) return 0.0;

    float alpha2 = sqr(cos_phi(w) * alpha_x) + sqr(sin_phi(w) * alpha_y);
    return (sqrt(1 + alpha2 * tan2t) - 1.0) / 2.0;
}

float TR_G1(vec3 w) {
    return 1.0 / (1.0 + TR_Lambda(w));
}

float TR_G(vec3 wo, vec3 wi) {
    return 1.0 / (1.0 + TR_Lambda(wo) + TR_Lambda(wi));
}

float TR_D(vec3 w, vec3 wm) {
    return TR_G1(w) / abs_cos_theta(w) * TR_D(wm) * abs(dot(w, wm));
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
    vec2 p = sample_uniform_disk_polar(random.x, random.y);

    // Warp hemispherical projection for visible normal sampling
    float h = sqrt(1.0 - sqr(p.x));
    p.y = mix(h, p.y, (1.0 + wh.z) / 2.0);
    
    // Reproject to hemisphere and transform normal to ellipsoid configuration
    float pz = sqrt(max(0.0, 1.0 - (sqr(p.x) + sqr(p.y))));
    vec3 nh = p.x * t1 + p.y * t2 + pz * wh;
    return normalize(vec3(alpha_x * nh.x, alpha_y * nh.y, max(1e-6, nh.z)));
}

struct BxdfSample {
    vec3  wi;
    float f;
    float pdf;
    float eta;
    bool  is_dirac_delta;
    bool  is_transmission;
    bool  failed;
};

BxdfSample sample_fail() {
    return BxdfSample(vec3(0), 0, 0, 0, false, false, true);
}

/****************************************************************
 * Diffuse
 ****************************************************************
 */

BxdfSample brdf_diffuse_sample(vec3 wo, vec2 random) {
    vec3 wi = sample_cosine_hemisphere(random.x, random.y);
    if (wo.z < 0.0) wi.z *= -1;
    float pdf = pdf_cosine_hemisphere(abs_cos_theta(wi));
    float f =  M_1_PI;

    return BxdfSample(wi, f, pdf, 1.0, false, false, false);
}

float brdf_diffuse_f(vec3 wo, vec3 wi) {
    if (!same_hemisphere(wo, wi)) return 0.0;
    return M_1_PI;
}

float brdf_diffuse_pdf(vec3 wo, vec3 wi) {
    if (!same_hemisphere(wo, wi)) return 0.0;
    return pdf_cosine_hemisphere(abs_cos_theta(wi));
}

/****************************************************************
 * Conductor
 ****************************************************************
 */

BxdfSample brdf_conductor_sample(vec3 wo, vec2 random) {
    float eta = material.ior;

    if (material.roughness == 0.0) {
        vec3 wi = vec3(-wo.x, -wo.y, wo.z);
        float act = abs_cos_theta(wi);
        float f = fresnel_complex(act, Complex(eta, 3.0)/* TODO conductors have a spectrally varying absorption coefficient k */) / act;
        return BxdfSample(wi, f, 1.0, 1.0, true, false, false);
    }

    vec3 wm = TR_Sample(wo, random);
    vec3 wi = m_reflect(wo, wm);
    if (!same_hemisphere(wo, wi)) return sample_fail();
    float pdf = TR_PDF(wo, wm) / (4.0 * abs(dot(wo, wm)));
    
    float cos_theta_o = abs_cos_theta(wo);
    float cos_theta_i = abs_cos_theta(wi);
    if (cos_theta_i == 0 || cos_theta_o == 0) return sample_fail();

    float fresnel = fresnel_complex(abs(dot(wo, wm)), Complex(eta, 3.0)/* TODO conductors have a spectrally varying absorption coefficient k */);
    float f = TR_D(wm) * fresnel * TR_G(wo, wi) / (4.0 * cos_theta_i * cos_theta_o);

    return BxdfSample(wi, f, pdf, 1.0, false, false, false);
}

float brdf_conductor_f(vec3 wo, vec3 wi) {
    if (!same_hemisphere(wo, wi)) return 0.0;
    if (material.roughness == 0.0) return 0.0;

    // Evaluate rough conductor brdf
    // Compute cosines and wm for conductor brdf
    float cos_theta_o = abs_cos_theta(wo);
    float cos_theta_i = abs_cos_theta(wi);
    if (cos_theta_o == 0.0 || cos_theta_i == 0.0) return 0.0;

    vec3 wm = wi + wo;
    if (length_squared(wm) < 1e-5) return 0.0;
    wm = normalize(wm);
    // Evaluate fresnel  factor for conductor brdf
    float fresnel = fresnel_complex(abs(dot(wo, wm)), Complex(material.ior, 3.0)/*, 3.0 TODO conductors have a spectrally varying absorption coefficient k */);
    return TR_D(wm) * fresnel * TR_G(wo, wi) / (4.0 * cos_theta_i * cos_theta_o);
}

float brdf_conductor_pdf(vec3 wo, vec3 wi) {
    if (!same_hemisphere(wo, wi)) return 0.0;
    if (material.roughness == 0.0) return 0.0;

    vec3 wm = wo + wi;
    if (length_squared(wm) < 1e-5) return 0.0;
    wm = normalize(wm);
    wm = faceforward(wm, vec3(0, 0, -1), wm);
    return TR_PDF(wo, wm) / (4.0 * abs(dot(wo, wm)));
}


/****************************************************************
 * Dielectric
 ****************************************************************
 */

BxdfSample bsdf_dielectric_sample(vec3 wo, vec3 random, const uint mode) {
    float eta = material.ior;

    // Perfectly specular 
    if (eta == 1.0 || material.roughness == 0.0) {
        float r = fresnel_dielectric(cos_theta(wo), eta);
        float t = 1.0 - r;
        float probability_reflection = r / (r + t);
        // Sample brdf
        if (random.z < probability_reflection) { 
            vec3 wi = vec3(-wo.x, -wo.y, wo.z);
            float f = r / abs_cos_theta(wi);
            return BxdfSample(wi, f, probability_reflection, 1.0, true, false, false);

        // Sample btdf 
        } else {
            vec3 wt = m_refract(wo, vec3(0, 0, 1), eta);
            float probability_transmission = t / (r + t);
            float ft = t / abs_cos_theta(wt);

            // Account for non symmetry between camera paths and light paths
            if (mode == Transport_Mode_Radiance) {
                ft /= sqr(eta);
            }

            return BxdfSample(wt, ft, probability_transmission, eta, true, true, false);
        }
    }

    // Rough specular
    vec3 wm = TR_Sample(wo, random.xy);
    float r = fresnel_dielectric(dot(wo, wm), eta);
    float t = 1.0 - r;
    float probability_reflection = r / (r + t);
    
    float pdf;
    // Sample brdf
    if (random.z < probability_reflection) {
        vec3 wi = m_reflect(wo, wm);
        if (!same_hemisphere(wo, wi)) return sample_fail();
        pdf = TR_PDF(wo, wm) / (4.0 * abs(dot(wo, wm))) * probability_reflection;
        float f = TR_D(wm) * TR_G(wo, wi) * r / (4.0 * cos_theta(wi) * cos_theta(wo));
        return BxdfSample(wi, f, pdf, 1.0, false, false, false);

    // Sample btdf
    } else {
        vec3 wt = m_refract(wo, wm, eta);
        if (same_hemisphere(wo, wt) || wt.z == 0) return sample_fail();
        float probability_transmission = t / (r + t);
        float denom = sqr(dot(wt, wm) + dot(wo, wm) / eta);
        float dwm_dwt = abs(dot(wt, wm)) / denom;
        pdf = TR_PDF(wo, wm) * dwm_dwt * probability_transmission;
        float ft = t * TR_D(wm) * TR_G(wo, wt) * abs(dot(wt, wm) * dot(wo, wm) / (cos_theta(wt) * cos_theta(wo) * denom));

        // Account for non symmetry between camera paths and light paths
        if (mode == Transport_Mode_Radiance) {
            ft /= sqr(eta);
        }

        return BxdfSample(wt, ft, pdf, eta, false, true, false);
    }
}

float bsdf_dielectric_f(vec3 wo, vec3 wi, const uint mode) {
    float eta = material.ior;
    
    if (eta == 1.0 || material.roughness == 0.0) {
        return 0.0;
    }

    // Compute generalized half vector
    float cos_theta_o = cos_theta(wo);
    float cos_theta_i = cos_theta(wi);
    bool reflect = cos_theta_o * cos_theta_i > 0.0;

    float etap = 1.0;
    if (!reflect) {
        etap = cos_theta_o > 0.0 ? eta : 1.0 / eta;
    }

    vec3 wm = wi * etap + wo;
    if (cos_theta_i == 0 || cos_theta_o == 0 ||  length_squared(wm) < 1e-5) return 0.0;
    wm = normalize(wm);
    wm = faceforward(wm, vec3(0, 0, -1), wm);

    // Discard backfacing microfacets
    if (dot(wm, wi) * cos_theta_i < 0 || dot(wm, wo) * cos_theta_o < 0) {
        return 0.0;
    }

    float fresnel = fresnel_dielectric(dot(wo, wm), eta);
    if (reflect) {
        // Compute reflection at rough dielectric interface
        return TR_D(wm) * TR_G(wo, wi) * fresnel / abs(4.0 * cos_theta_i * cos_theta_o);
    }
    else {
        // Compute transmission at rough dielectric interface
        float denom = sqr(dot(wi, wm) + dot(wo, wm) / etap) * cos_theta_i * cos_theta_o;
        float ft = TR_D(wm) * TR_G(wo, wi) * (1.0 - fresnel) * abs(dot(wi, wm) * dot(wo, wm) / denom);
        
        // Account for non symmetry between camera paths and light paths
        if (mode == Transport_Mode_Radiance) {
            ft /= sqr(etap);
        }
        return ft;
    }
}

float bsdf_dielectric_pdf(vec3 wo, vec3 wi) {
    float eta = material.ior;

    // Perfectly specular 
    if (eta == 1.0 || material.roughness == 0.0) {
        return 0.0;
    }

    // Rough specular
    // Compute generalized half vector
    float cos_theta_o = cos_theta(wo);
    float cos_theta_i = cos_theta(wi);
    bool reflect = cos_theta_o * cos_theta_i > 0.0;

    float etap = 1.0;
    if (!reflect) {
        etap = cos_theta_o > 0.0 ? eta : 1.0 / eta;
    }

    vec3 wm = wi * etap + wo;
    if (cos_theta_i == 0 || cos_theta_o == 0 ||  length_squared(wm) < 1e-5) return 0.0;
    wm = normalize(wm);
    wm = faceforward(wm, vec3(0, 0, -1), wm);

    // Discard backfacing microfacets
    if (dot(wm, wi) * cos_theta_i < 0 || dot(wm, wo) * cos_theta_o < 0) {
        return 0.0;
    }

    // Determine Fresnel reflectance of rough dielectric boundary
    float r = fresnel_dielectric(dot(wo, wm), eta);
    float t = 1.0 - r;

    // Return pdf of rough reflection
    if (reflect) {
        float probability_reflection = r / (r + t);
        return TR_PDF(wo, wm) / (4.0 * abs(dot(wo, wm))) * probability_reflection;

    // Return pdf of rough transmission
    } else {
        float probability_transmission = t / (r + t);
        float denom = sqr(dot(wi, wm) + dot(wo, wm) / etap);
        float dwm_dwi = abs(dot(wi, wm)) / denom;
        return TR_PDF(wo, wm) * dwm_dwi * probability_transmission;
    }
}

// TODO thin dielectric bsdf

BxdfSample bxdf_sample_material(int material_type, vec3 wo, inout uint seed) {
    switch(material_type) { 
        case MaterialType_Diffuse:    
            return brdf_diffuse_sample(wo, random2D(seed)); 
        break;          
        case MaterialType_Dielectric: 
            return bsdf_dielectric_sample(wo, random3D(seed), Transport_Mode_Radiance); 
        break;
        case MaterialType_Conductor:
            if (random1D(seed) < material.metallic)
                return brdf_conductor_sample(wo, random2D(seed));
            return brdf_diffuse_sample(wo, random2D(seed));
        break;
    }
}

float bxdf_f(int material_type, vec3 wo, vec3 wi, float random) {
    switch(material_type) {
        case MaterialType_Diffuse:    
            return brdf_diffuse_f(wo, wi); 
        break;
        case MaterialType_Dielectric: 
            return bsdf_dielectric_f(wo, wi, Transport_Mode_Radiance); 
        break;
        case MaterialType_Conductor:
            if (random < material.metallic) 
                return brdf_conductor_f(wo, wi);
            return brdf_diffuse_f(wo, wi);
        break;
    }
}

float bxdf_pdf(int material_type, vec3 wo, vec3 wi, float random) {
    switch(material_type) {
        case MaterialType_Diffuse:    
            return brdf_diffuse_pdf(wo, wi); 
        break;
        case MaterialType_Dielectric: 
            return bsdf_dielectric_pdf(wo, wi); 
        break;
        case MaterialType_Conductor:
            if (random < material.metallic)
                return brdf_conductor_pdf(wo, wi);
            return brdf_diffuse_pdf(wo, wi);
        break;
    }
}
