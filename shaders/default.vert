#version 450
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec3 inNormal;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec3 fragWorldPosition;
layout(location = 2) out vec3 fragWorldNormal;

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

layout(push_constant) uniform ObjectPushConstants {
    mat4 model;
} objectData;

void main() {
    vec4 worldPosition = objectData.model * vec4(inPosition, 1.0);

    fragColor = inColor;
    fragWorldPosition = worldPosition.xyz;
    fragWorldNormal = normalize(
        transpose(inverse(mat3(objectData.model))) * inNormal
    );
    gl_Position = scene.proj * scene.view * worldPosition;
}
