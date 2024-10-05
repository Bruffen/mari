#version 450

struct PointLight {
    vec4 position;
    vec4 color;
};

layout (set = 0, binding = 0) uniform GlobalUbo {
    mat4 projection;
    mat4 view;
    mat4 invView;
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
    vec3 normal = normalize(fragNormalWorld);
    vec3 ambientLight = ubo.ambientLightColor.rgb * ubo.ambientLightColor.a;
    vec3 diffuseLight = vec3(0.0);
    vec3 specularLight = vec3(0.0);

    vec3 cameraPosWorld = ubo.invView[3].xyz;
    vec3 viewDirection = normalize(cameraPosWorld - fragPosWorld);

    for (int i = 0; i < ubo.numLights; i++) {
        PointLight pl = ubo.pointLights[i];

        vec3  lightDir = pl.position.xyz - fragPosWorld;
        float attenuation = 1.0 / dot(lightDir, lightDir);
        lightDir = normalize(lightDir);
        float ndotl = max(dot(normal, lightDir), 0.0);
        vec3  intensity = pl.color.rgb * pl.color.a * attenuation;

        diffuseLight += intensity * ndotl;

        vec3 halfVector = normalize(lightDir + viewDirection);
        float ndoth = dot(normal, halfVector);
        ndoth = clamp(ndoth, 0, 1);
        ndoth = pow(ndoth, 32);
        specularLight += intensity * ndoth;
    }

    outColor = vec4(ambientLight + diffuseLight * fragColor + specularLight * fragColor, 1.0);
}