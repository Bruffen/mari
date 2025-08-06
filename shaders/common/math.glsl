/**
 * Helper functions
 */

float sqr(float v) {
    return v * v;
}

float safeSqrt(float x) {
    return sqrt(max(0.0, x));
}

float lengthSquared(vec3 w) {
    return w.x * w.x + w.y * w.y + w.z * w.z;
}

bool sameHemisphere(vec3 wo, vec3 wi) {
    return wo.z * wi.z > 0.0;
}

float absCosTheta(vec3 w) {
    return abs(w.z);
}

float cosTheta(vec3 w) {
    return w.z;
}

float cos2Theta(vec3 w) {
    return sqr(w.z);
}

float sin2Theta(vec3 w) {
    return max(0.0, 1.0 - cos2Theta(w));
}

float sinTheta(vec3 w) {
    return safeSqrt(sin2Theta(w));
}

float tan2Theta(vec3 w) {
    return sin2Theta(w) / cos2Theta(w);
}

float cosPhi(vec3 w) {
    float sint = sinTheta(w);
    return sint == 0 ? 1 : clamp(w.x / sint, -1.0, 1.0);
}

float sinPhi(vec3 w) {
    float sint = sinTheta(w);
    return sint == 0 ? 0 : clamp(w.y / sint, -1.0, 1.0);
}

/* 
 * GLSL reflect function expects the incident vector to point towards the surface
 * We do the opposite and so this method exists
 */
vec3 mReflect(vec3 wo, vec3 n) {
    return -wo + 2 * dot(wo, n) * n;
}

vec3 mRefract(vec3 wi, vec3 normal, float eta, inout float etap) {
    float cosTheta_i = dot(normal, wi);

    if (cosTheta_i < 0.0) {
        eta = 1.0 / eta;
        cosTheta_i = -cosTheta_i;
        normal = -normal;
    }

    // Snell's law
    float sin2Theta_i = max(0.0, 1.0 - (cosTheta_i * cosTheta_i));
    float sin2Theta_t = sin2Theta_i / sqr(eta);

    float cosTheta_t = sqrt(1.0 - sin2Theta_t);

    etap = eta;
    return -wi / eta + (cosTheta_i / eta - cosTheta_t) * normal;
}