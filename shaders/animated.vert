#version 450

layout(location = 0) in vec3 inColor;
layout(location = 1) in vec3 inNormal;
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

layout(std430, binding = 1) readonly buffer AnimationPositions {
    vec4 positions[];
}
animationData;

layout(push_constant) uniform AnimatedObjectPushConstants {
    mat4 model;
    uint previousPositionOffset;
    uint nextPositionOffset;
    float interpolation;
    uint vertexCount;
} objectData;

void main() {

    uint vertexIndex = uint(gl_VertexIndex);

    vec3 previousPos =
        animationData.positions[objectData.previousPositionOffset + vertexIndex].xyz;

    vec3 nextPos =
        animationData.positions[objectData.nextPositionOffset + vertexIndex].xyz;

    vec3 pos = mix(previousPos, nextPos, objectData.interpolation);

    vec4 worldPosition = objectData.model * vec4(pos, 1.0);
    fragWorldPosition = worldPosition.xyz;
    fragWorldNormal = normalize(
        transpose(inverse(mat3(objectData.model))) * inNormal
    );
    gl_Position = scene.proj * scene.view * worldPosition;
    fragColor = inColor;
}
