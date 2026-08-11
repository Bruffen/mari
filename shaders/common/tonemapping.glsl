/**
 * ACESFilm tone mapping apr
 */
vec3 tone_ACES(vec3 linear_color, float exposure) {
    linear_color *= exposure;

    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    linear_color = clamp((linear_color*(a*linear_color+b))/(linear_color*(c*linear_color+d)+e), 0.0, 1.0);
    vec3 gamma_color = pow(linear_color, vec3(1.0/2.2));
    return gamma_color;
}

/**
 * Minimal AgX implementation
 * From https://iolite-engine.com/blog_posts/minimal_agx_implementation
 */

// 0: Default, 1: Golden, 2: Punchy
#define AGX_LOOK 0

// Mean error^2: 3.6705141e-06
vec3 agx_default_contrast_approx(vec3 x) {
vec3 x2 = x * x;
vec3 x4 = x2 * x2;

return  + 15.5     * x4 * x2
        - 40.14    * x4 * x
        + 31.96    * x4
        - 6.868    * x2 * x
        + 0.4298   * x2
        + 0.1191   * x
        - 0.00232;
}

vec3 agx(vec3 val) {
    const mat3 agx_mat = mat3(
        0.842479062253094,  0.0423282422610123, 0.0423756549057051,
        0.0784335999999992, 0.878468636469772,  0.0784336,
        0.0792237451477643, 0.0791661274605434, 0.879142973793104
    );
        
    const float min_ev = -12.47393;
    const float max_ev = 4.026069;

    // Input transform (inset)
    val = agx_mat * val;

    // Log2 space encoding
    val = clamp(log2(val), min_ev, max_ev);
    val = (val - min_ev) / (max_ev - min_ev);

    // Apply sigmoid function approximation
    val = agx_default_contrast_approx(val);

    return val;
}

    vec3 agx_eotf(vec3 val) {
    const mat3 agx_mat_inv = mat3(
        1.19687900512017, -0.0528968517574562, -0.0529716355144438,
        -0.0980208811401368, 1.15190312990417, -0.0980434501171241,
        -0.0990297440797205, -0.0989611768448433, 1.15107367264116
    );
        
    // Inverse input transform (outset)
    val = agx_mat_inv * val;

    // sRGB IEC 61966-2-1 2.2 Exponent Reference EOTF Display
    // NOTE: We're linearizing the output here. Comment/adjust when
    // *not* using a sRGB render target
    //val = pow(val, vec3(2.2));

    return val;
}

vec3 agx_look(vec3 val) {
    const vec3 lw = vec3(0.2126, 0.7152, 0.0722);
    float luma = dot(val, lw);

    // Default
    vec3 offset = vec3(0.0);
    vec3 slope = vec3(1.0);
    vec3 power = vec3(1.0);
    float sat = 1.0;

    #if AGX_LOOK == 1
    // Golden
    slope = vec3(1.0, 0.9, 0.5);
    power = vec3(0.8);
    sat = 0.8;
    #elif AGX_LOOK == 2
    // Punchy
    slope = vec3(1.0);
    power = vec3(1.35, 1.35, 1.35);
    sat = 1.4;
    #endif

    // ASC CDL
    val = pow(val * slope + offset, power);
    return luma + sat * (val - luma);
}

vec3 tone_AgX(vec3 linear_color, float exposure) {
    vec3 gamma_color = agx(linear_color * exposure);
    gamma_color = agx_look(gamma_color); // Optional
    gamma_color = agx_eotf(gamma_color);
    return gamma_color;
}

vec3 apply_tonemapping(int tonemapper, vec3 color, float exposure) {
    vec3 tonemapped_color = color;
    switch (tonemapper) {
        case 1: tonemapped_color = pow(color * exposure, vec3(1.0/2.2)); break;
        case 2: tonemapped_color = tone_ACES(color, exposure);           break;
        case 3: tonemapped_color = tone_AgX(color, exposure);            break;
    }
    return tonemapped_color;
}