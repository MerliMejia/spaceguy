#pragma once
#define VULKAN_HPP_NO_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

// Different graphic pipelines.
extern void CREATE_SIMPLE_CAMERA_MODEL_GP();

// Different uniform buffers and push constants
struct CameraBufferObject
{
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};

struct ObjectPushConstants
{
    alignas(16) glm::mat4 model;
};

// Different command buffer recordings.
