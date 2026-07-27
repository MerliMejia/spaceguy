#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragWorldPosition;
layout(location = 2) in vec3 fragWorldNormal;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform ObjectPushConstants {
    mat4 model;
    uint unlit;
} objectData;

struct PointLight {
    vec4 position;
    vec4 colorIntensity;
    vec4 attenuation;
};

layout(binding = 0) uniform SceneBufferObject {
    mat4 view;
    mat4 proj;
    vec4 viewPosition;
    vec4 sunDirection;
    vec4 sunColorIntensity;
    uvec4 lightCounts;
    PointLight pointLights[32];
} scene;

float toonDiffuse(float ndotl) {
    float width = max(fwidth(ndotl) * 1.5, 0.01);

    return mix(
        0.38,
        1.0,
        smoothstep(
            0.42 - width,
            0.42 + width,
            ndotl
        )
    );
}

void main() {
    vec3 N = normalize(fragWorldNormal);

    if (!gl_FrontFacing) {
        N = -N;
    }

    float skyFactor = clamp(
            -N.z * 0.5 + 0.5,
            0.0,
            1.0
        );

    vec3 groundAmbient = vec3(0.10, 0.12, 0.16);
    vec3 skyAmbient = vec3(0.28, 0.34, 0.42);

    vec3 lighting = mix(
            groundAmbient,
            skyAmbient,
            skyFactor
        );

    vec3 pointDiffuse = vec3(0.0);
    vec3 pointGlow = vec3(0.0);

    if (scene.sunColorIntensity.a > 0.0) {
        vec3 L = normalize(
                -scene.sunDirection.xyz
            );

        float ndotl = dot(N, L);
        float diffuseBand = toonDiffuse(ndotl);

        lighting += diffuseBand *
                scene.sunColorIntensity.rgb *
                scene.sunColorIntensity.a;
    }

    // Point lights use a softer band and an explicit artistic radius.
    for (uint i = 0; i < scene.lightCounts.x; ++i) {
        PointLight light = scene.pointLights[i];

        if (light.colorIntensity.a <= 0.0) {
            continue;
        }

        vec3 toLight =
            light.position.xyz - fragWorldPosition;

        float distanceToLight = length(toLight);

        if (distanceToLight <= 0.0001) {
            continue;
        }

        vec3 L = toLight / distanceToLight;

        float attenuation =
            1.0 / (
                light.attenuation.x +
                    light.attenuation.y * distanceToLight +
                    light.attenuation.z * distanceToLight * distanceToLight
                );

        float ndotl = max(dot(N, L), 0.0);

        float pointBand = toonDiffuse(ndotl);

        vec3 attenuatedLight =
            light.colorIntensity.rgb *
                light.colorIntensity.a *
                attenuation;

        pointDiffuse += pointBand * attenuatedLight;
    }

    vec3 finalColor = fragColor * (lighting + (pointDiffuse * 5));

    outColor = vec4(finalColor, 1.0);
}
