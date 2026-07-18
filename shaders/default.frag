#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragWorldPosition;
layout(location = 2) in vec3 fragWorldNormal;

layout(location = 0) out vec4 outColor;

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

float toonSpecular(vec3 N, vec3 V, vec3 L) {
    float ndotl = max(dot(N, L), 0.0);

    if (ndotl <= 0.0) {
        return 0.0;
    }

    vec3 H = normalize(L + V);
    float highlight = pow(max(dot(N, H), 0.0), 48.0);
    float width = max(fwidth(highlight) * 1.5, 0.002);

    return smoothstep(
        0.72 - width,
        0.78 + width,
        highlight
    );
}

float pointLightAttenuation(float distanceToLight, float radius) {
    float normalizedDistance = distanceToLight / radius;
    float attenuation = clamp(
            1.0 - normalizedDistance,
            0.0,
            1.0
        );

    return attenuation * attenuation;
}

void main() {
    vec3 V = normalize(
            scene.viewPosition.xyz - fragWorldPosition
        );

    vec3 N = normalize(fragWorldNormal);

    if (!gl_FrontFacing) {
        N = -N;
    }

    // Cool hemispheric ambient lighting.
    float skyFactor = clamp(
            N.z * 0.5 + 0.5,
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

    vec3 specular = vec3(0.0);
    vec3 pointDiffuse = vec3(0.0);
    vec3 pointGlow = vec3(0.0);

    // Primary toon-shaded directional light.
    if (scene.sunColorIntensity.a > 0.0) {
        vec3 L = normalize(
                -scene.sunDirection.xyz
            );

        float ndotl = dot(N, L);
        float diffuseBand = toonDiffuse(ndotl);

        lighting += diffuseBand *
                scene.sunColorIntensity.rgb *
                scene.sunColorIntensity.a;

        float specularBand = toonSpecular(N, V, L);

        specular += specularBand *
                vec3(1.0, 0.88, 0.65) *
                0.12;
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

        float radius = 10.0;

        // float attenuation = pointLightAttenuation(distanceToLight, radius);

        // float toonAttenuation =
        //     attenuation > 0.35 ? 1.0 :
        //     attenuation > 0.08 ? 0.45 :
        //     0.0;
        //
        float attenuation =
            1.0 / (
                light.attenuation.x +
                    light.attenuation.y * distanceToLight +
                    light.attenuation.z * distanceToLight * distanceToLight
                );

        float ndotl = max(dot(N, L), 0.0);

        // Softer than the directional-light toon boundary.
        float pointBand = smoothstep(
                0.05,
                0.45,
                ndotl
            );

        vec3 attenuatedLight =
            light.colorIntensity.rgb *
                light.colorIntensity.a *
                attenuation;

        pointDiffuse += pointBand * attenuatedLight;

        // Preserve a visible radial pool on dark or strongly tinted surfaces.
        // This remains distance-based while the diffuse response above still
        // uses the actual surface normal.
        pointGlow += attenuatedLight;
    }

    vec3 finalColor =
        fragColor * (lighting + pointDiffuse) +
            pointGlow * 0.35 +
            specular;

    outColor = vec4(finalColor, 1.0);
}
