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
uint animationPositionOffset;
uint vertexCount;
uint _pad0;
uint _pad1;
}
objectData;

void main() {
vec3 pos = animationData.positions[objectData.animationPositionOffset + uint(gl_VertexIndex)].xyz;

gl_Position = camera.proj * camera.view * objectData.model * vec4(pos, 1.0);
fragColor = inColor;
}