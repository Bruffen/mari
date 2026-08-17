/**
 * Helper functions
 */

#ifndef _MATH_GLSL_
#define _MATH_GLSL_

#include "constants.glsl"
#include "complex.glsl"

float sqr(float v) {
    return v * v;
}

float safe_sqrt(float x) {
    return sqrt(max(0.0, x));
}

float safe_asin(float x) {
    return asin(clamp(x, -1.0, 1.0));
}

float safe_acos(float x) {
    return acos(clamp(x, -1.0, 1.0));
}

float angle_between(vec3 v1, vec3 v2) {
    if (dot(v1, v2) < 0.0)
        return M_PI - 2.0 * safe_asin(length(v1 + v2) / 2.0);
    return 2.0 * safe_asin(length(v2 - v1) / 2.0);
}

float length_squared(vec3 w) {
    return w.x * w.x + w.y * w.y + w.z * w.z;
}

bool same_hemisphere(vec3 wo, vec3 wi) {
    return wo.z * wi.z > 0.0;
}

float abs_cos_theta(vec3 w) {
    return abs(w.z);
}

float cos_theta(vec3 w) {
    return w.z;
}

float cos2_theta(vec3 w) {
    return sqr(w.z);
}

float sin2_theta(vec3 w) {
    return max(0.0, 1.0 - cos2_theta(w));
}

float sin_theta(vec3 w) {
    return safe_sqrt(sin2_theta(w));
}

float tan2_theta(vec3 w) {
    return sin2_theta(w) / cos2_theta(w);
}

float cos_phi(vec3 w) {
    float sint = sin_theta(w);
    return sint == 0 ? 1 : clamp(w.x / sint, -1.0, 1.0);
}

float sin_phi(vec3 w) {
    float sint = sin_theta(w);
    return sint == 0 ? 0 : clamp(w.y / sint, -1.0, 1.0);
}

vec3 rotate_around_axis(vec3 v, vec3 axis, float theta) {
    float cos_theta = cos(theta);
    float sin_theta = sin(theta);

    return (v * cos_theta) + (cross(axis, v) * sin_theta) + (axis * dot(axis, v)) * (1.0f - cos_theta);
}

vec3 spherical_direction(float sin_theta, float cos_theta, float phi) {
    return vec3(
        clamp(sin_theta, -1.0, 1.0) * cos(phi),
        clamp(sin_theta, -1.0, 1.0) * sin(phi),
        clamp(cos_theta, -1.0, 1.0)
    );
}

vec3 gram_schmidt(vec3 v, vec3 w) {
    return v - dot(v, w) * w;
}

/* 
 * GLSL reflect function expects the incident vector to point towards the surface
 * We do the opposite and so this method exists
 */
vec3 m_reflect(vec3 wo, vec3 n) {
    return -wo + 2 * dot(wo, n) * n;
}

vec3 m_refract(vec3 wi, vec3 normal, inout float eta) {
    float cos_theta_i = dot(normal, wi);

    if (cos_theta_i < 0.0) {
        eta = 1.0 / eta;
        cos_theta_i = -cos_theta_i;
        normal = -normal;
    }

    // Snell's law
    float sin2_theta_i = max(0.0, 1.0 - (cos_theta_i * cos_theta_i));
    float sin2_theta_t = sin2_theta_i / sqr(eta);

    float cos_theta_t = sqrt(1.0 - sin2_theta_t);
    return -wi / eta + (cos_theta_i / eta - cos_theta_t) * normal;
}

float fresnel_dielectric(float cos_theta_i, float eta) {
    cos_theta_i = clamp(cos_theta_i, -1.0, 1.0);
    // Exiting the material
    if (cos_theta_i < 0.0) {
        eta = 1.0 / eta;
        cos_theta_i = -cos_theta_i;
    }

    // Snell's law
    float sin2_theta_i = 1.0 - sqr(cos_theta_i);
    float sin2_theta_t = sin2_theta_i / sqr(eta);
    // Handle total internal reflection
    if (sin2_theta_t >= 1.0) return 1.0;

    float cos_theta_t = safe_sqrt(1.0 - sin2_theta_t);

    float r_parl = (eta * cos_theta_i - cos_theta_t) / (eta * cos_theta_i + cos_theta_t);
    float r_perp = (cos_theta_i - eta * cos_theta_t) / (cos_theta_i + eta * cos_theta_t);
    return (sqr(r_parl) + sqr(r_perp)) * 0.5;
}

float fresnel_complex(float cos_theta_i, Complex eta) {
    cos_theta_i = clamp(cos_theta_i, 0.0, 1.0);

    // Snell's law
    float sin2_theta_i = 1.0 - sqr(cos_theta_i);
    Complex sin2_theta_t = complex_divide(sin2_theta_i, (complex_sqr(eta)));
    Complex cos_theta_t = complex_sqrt(complex_subtract(1.0, sin2_theta_t));

    Complex etaMcos_theta_i = complex_multiply(eta, cos_theta_i);
    Complex r_parl = complex_divide(complex_subtract(etaMcos_theta_i, cos_theta_t), complex_add(etaMcos_theta_i, cos_theta_t));
    Complex etaMcos_theta_t = complex_multiply(eta, cos_theta_t);
    Complex r_perp = complex_divide(complex_subtract(cos_theta_i, etaMcos_theta_t), complex_add(cos_theta_i, etaMcos_theta_t));
    return (complex_norm(r_parl) + complex_norm(r_perp)) * 0.5;
}

// TODO spectral fresnel complex with k function

// Given a temperature (in Kelvin), estimate an RGB equivalent
// https://tannerhelland.com/2012/09/18/convert-temperature-rgb-algorithm-code.html
vec3 blackbody(float temperature) {
    if (temperature <= 0.0) return vec3(0.0);
    vec3 color;
    float tmp;

    // Temperature must fall between 1000 and 40000 degrees
    if (temperature < 1000) temperature = 1000;
    if (temperature > 40000) temperature = 40000;
    
    // All calculations require temperature / 100, so only do the conversion once
    temperature = temperature / 100;
    
    // Calculate each color in turn
    
    // First: red
    if (temperature <= 66) {
        color.r = 255;
    } else {
        // Note: the R-squared value for this approximation is .988
        tmp = temperature - 60;
        tmp = 329.698727446 * pow(tmp, -0.1332047592);
        color.r = tmp;
        if (color.r < 0) color.r = 0;
        if (color.r > 255) color.r = 255;
    }
    
    //Second: green
    if (temperature <= 66) {
        // Note: the R-squared value for this approximation is .996
        tmp = temperature;
        tmp = 99.4708025861 * log(tmp) - 161.1195681661;
        color.g = tmp;
        if (color.g < 0) color.g = 0;
        if (color.g > 255) color.g = 255;
    }
    else {
        // Note: the R-squared value for this approximation is .987
        tmp = temperature - 60;
        tmp = 288.1221695283 * pow(tmp, -0.0755148492);
        color.g = tmp;
        if (color.g < 0) color.g = 0;
        if (color.g > 255) color.g = 255;
    }
    
    // 'Third: blue
    if (temperature >= 66) {
        color.b = 255;
    }
    else if (temperature <= 19) {
        color.b = 0;
    }
    else {
        //Note: the R-squared value for this approximation is .998
        tmp = temperature - 10;
        tmp = 138.5177312231 * log(tmp) - 305.0447927307;
        
        color.b = tmp;
        if (color.b < 0) color.b = 0;
        if (color.b > 255) color.b = 255;
    }

    return color;
}

/****************************************************************
 * Shading frame to and from world frame transformations
 ****************************************************************
 */
 
vec3 to_local(vec3 t, vec3 b, vec3 n, vec3 v) {
    return vec3(dot(v, t), dot(v, b), dot(v, n));
}

vec3 from_local(vec3 t, vec3 b, vec3 n, vec3 v) {
    return v.x * t + v.y * b + v.z * n;
}

vec3 from_local(vec4 tangent, vec3 normal, vec3 v) {
    vec3 bitangent = cross(normal, tangent.xyz) * tangent.w;
    return from_local(tangent.xyz, bitangent, normal, v);
}

mat3 frame_from_z(vec3 wo) {
    float sign = sign(wo.z);
    float _a = -1.0 / (sign + wo.z);
    float _b = wo.x * wo.y * _a;
    vec3 t = vec3(1.0 + sign * sqr(wo.x) * _a, sign * _b, -sign * wo.x);
    vec3 b = vec3(_b, sign + sqr(wo.y) * _a, -wo.y);
    return mat3(t, b, wo);
} 


vec3 equal_area_square_to_sphere(vec2 p) {
    float u = 2.0 * p.x - 1.0;
    float v = 2.0 * p.y - 1.0;
    float up = abs(u);
    float vp = abs(v);

    float signedDistance = 1.0 - (up + vp);
    float d = abs(signedDistance);
    float r = 1.0 - d;

    float phi = (r == 0 ? 1 : (vp - up) / r + 1) * M_PI_4;
    float z = abs(1.0 - sqr(r)) * sign(signedDistance);

    float cos_phi = abs(cos(phi)) * sign(u);
    float sin_phi = abs(sin(phi)) * sign(v);

    float x = cos_phi * r * safe_sqrt(2.0 - sqr(r));
    float y = sin_phi * r * safe_sqrt(2.0 - sqr(r));
    return vec3(x, y, z);
}

vec2 equal_area_sphere_to_square(vec3 d) {
    float x = abs(d.x);
    float y = abs(d.y);
    float z = abs(d.z);

    float r = safe_sqrt(1.0 - z);

    float a = max(x, y);
    float b = min(x, y);
    b = a == 0 ? 0 : b / a;

    float phi = atan(b) * 2.0 * M_1_PI;

    if (x < y)
        phi = 1 - phi;

    float v = phi * r;
    float u = r - v;

    if (d.z < 0) {
        float tmp = u;
        u = 1 - v;
        v = 1 - tmp;
    }

    u = abs(u) * sign(d.x);
    v = abs(v) * sign(d.y);

    return vec2(0.5 * (u + 1.0), 0.5 * (v + 1.0));
}

vec2 equal_area_wrap_square(vec2 uv) {
    if (uv.x < 0) {
        uv.x = -uv.x;     // mirror across u = 0
        uv.y = 1 - uv.y;  // mirror across v = 0.5
    } else if (uv.x > 1) {
        uv.x = 2 - uv.x;  // mirror across u = 1
        uv.y = 1 - uv.y;  // mirror across v = 0.5
    }
    if (uv.y < 0) {
        uv.x = 1 - uv.x;  // mirror across u = 0.5
        uv.y = -uv.y;     // mirror across v = 0;
    } else if (uv.y > 1) {
        uv.x = 1 - uv.x;  // mirror across u = 0.5
        uv.y = 2 - uv.y;  // mirror across v = 1
    }
    return uv;
}

#endif // _MATH_GLSL