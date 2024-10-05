#version 450

struct PointLight {
    vec4 position;
    vec4 color;
};

layout (set = 0, binding = 0) uniform GlobalUbo {
    mat4 projection;
    mat4 view;
    vec4 ambientLightColor;
    PointLight pointLights[10]; // TODO look at specialization constants
    int numLights;
} ubo;


layout(push_constant) uniform Push {
    mat4 modelMatrix;
    mat4 normalMatrix;
} push;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragPosWorld;
layout(location = 2) in vec3 fragNormalWorld;

layout (location = 0) out vec4 outColor;

void main() {
    vec3 light = ubo.ambientLightColor.rgb * ubo.ambientLightColor.a;
    vec3 normal = normalize(fragNormalWorld);

    for (int i = 0; i < ubo.numLights; i++) {
        PointLight pl = ubo.pointLights[i];

        vec3  lightDir = pl.position.xyz - fragPosWorld.xyz;
        float attenuation = 1.0 / dot(lightDir, lightDir);
        float ndotl = max(dot(normal, normalize(lightDir)), 0.0);
        vec3  intensity = pl.color.rgb * pl.color.a * attenuation;

        light += intensity * ndotl;
    }

    outColor = vec4(light * fragColor, 1.0);
}