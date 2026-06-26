#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec4 fragColor;

layout(set = 0, binding = 0) uniform CameraBufferObject {
    mat4 view;
    mat4 proj;
} camera;

struct Particle {
    vec4 positionLifetime;
    vec4 velocitySize;
    vec4 color;
    vec4 state;
    uvec4 meta;
};

layout(std430, set = 0, binding = 1) readonly buffer Particles {
    Particle particles[];
};

void main() {
    Particle particle = particles[gl_InstanceIndex];

    if (particle.meta.y == 0u || particle.positionLifetime.w <= 0.0) {
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
        fragColor = vec4(0.0);
        return;
    }

    vec3 center = particle.positionLifetime.xyz;
    float size = particle.velocitySize.w;

    vec3 cameraRight = vec3(camera.view[0][0], camera.view[1][0], camera.view[2][0]);
    vec3 cameraUp = vec3(camera.view[0][1], camera.view[1][1], camera.view[2][1]);

    vec3 worldOffset =
        cameraRight * inPosition.x * size +
            cameraUp * inPosition.y * size;

    vec3 worldPosition = center + worldOffset;

    gl_Position = camera.proj * camera.view * vec4(worldPosition, 1.0);

    fragColor = particle.color * vec4(inColor, 1.0);
}
