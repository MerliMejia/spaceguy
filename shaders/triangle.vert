#version 450

layout(binding = 0) uniform CameraBufferObject {
    mat4 view;
    mat4 proj;
} camera;

layout(push_constant) uniform ObjectPushConstants {
    mat4 model;
} objectData;

vec2 positions[3] = vec2[](vec2(0.0, -0.5), vec2(0.5, 0.5), vec2(-0.5, 0.5));

void main() {
    gl_Position = camera.proj *
        camera.view *
        objectData.model *
        vec4(positions[gl_VertexIndex], 0.0, 1.0);
}