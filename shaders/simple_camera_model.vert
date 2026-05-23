#version 450
layout(location = 0) in vec2 inPosition;

layout(binding = 0) uniform CameraBufferObject {
    mat4 view;
    mat4 proj;
} camera;

layout(push_constant) uniform ObjectPushConstants {
    mat4 model;
} objectData;

void main() {
    gl_Position = camera.proj *
        camera.view *
        objectData.model *
        vec4(inPosition, 0.0, 1.0);
}