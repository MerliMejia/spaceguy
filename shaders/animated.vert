#version 450

layout(location = 0) in vec3 inColor;
layout(location = 0) out vec3 fragColor;

layout(binding = 0) uniform CameraBufferObject {
    mat4 view;
    mat4 proj;
} camera;

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

    gl_Position = camera.proj * camera.view * objectData.model * vec4(pos, 1.0);
    fragColor = inColor;
}