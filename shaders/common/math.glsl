/**
 * Helper functions
 */

#ifndef _MATH_GLSL_
#define _MATH_GLSL_

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

vec3 rotateAroundAxis(vec3 v, vec3 axis, float theta) {
    float cosTheta = cos(theta);
    float sinTheta = sin(theta);

    return (v * cosTheta) + (cross(axis, v) * sinTheta) + (axis * dot(axis, v)) * (1.0f - cosTheta);
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

vec3 equalAreaSquareToSphere(vec2 p) {
    float u = 2.0 * p.x - 1.0;
    float v = 2.0 * p.y - 1.0;
    float up = abs(u);
    float vp = abs(v);

    float signedDistance = 1.0 - (up + vp);
    float d = abs(signedDistance);
    float r = 1.0 - d;

    float phi = (r == 0 ? 1 : (vp - up) / r + 1) * M_PI_4;
    float z = abs(1.0 - sqr(r)) * sign(signedDistance);

    float cosPhi = abs(cos(phi)) * sign(u);
    float sinPhi = abs(sin(phi)) * sign(v);

    float x = cosPhi * r * safeSqrt(2.0 - sqr(r));
    float y = sinPhi * r * safeSqrt(2.0 - sqr(r));
    return vec3(x, y, z);
}

vec2 equalAreaSphereToSquare(vec3 d) {
    float x = abs(d.x);
    float y = abs(d.y);
    float z = abs(d.z);

    float r = safeSqrt(1.0 - z);

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

vec2 equalAreaWrapSquare(vec2 uv) {
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