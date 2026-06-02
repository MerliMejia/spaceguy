#pragma once
#define VULKAN_HPP_NO_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

// Different graphic pipelines.
extern void DEFAULT_GRAPHICS_PIPELINE();
extern void ANIMATED_GRAPHICS_PIPELINE();

// Different uniform buffers and push constants
struct CameraBufferObject
{
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};

struct ObjectPushConstants
{
    glm::mat4 model;
};

struct AnimatedObjectPushConstants
{
    glm::mat4 model;
    uint32_t animationPositionOffset;
    uint32_t vertexCount;
    uint32_t _pad0;
    uint32_t _pad1;
};

// Different command buffer recordings.
