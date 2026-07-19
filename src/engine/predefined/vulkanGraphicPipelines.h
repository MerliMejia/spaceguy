#pragma once
#include <cstdint>
#define VULKAN_HPP_NO_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

// Different graphic pipelines.
extern void PARTICLE_COMPUTE_GRAPHICS_PIPELINE();
extern void PARTICLE_GRAPHICS_PIPELINE();
extern void DEFAULT_GRAPHICS_PIPELINE();
extern void ANIMATED_GRAPHICS_PIPELINE();
extern void DEBUG_GRAPHICS_PIPELINE();

// Different uniform buffers and push constants
constexpr uint32_t MAX_POINT_LIGHTS = 32;

struct alignas(16) PointLightGpu {
  glm::vec4 position;
  glm::vec4 colorIntensity;
  glm::vec4 attenuation;
};

struct SceneBufferObject {
  alignas(16) glm::mat4 view;
  alignas(16) glm::mat4 proj;
  alignas(16) glm::vec4 viewPosition;
  alignas(16) glm::vec4 sunDirection;
  alignas(16) glm::vec4 sunColorIntensity;
  alignas(16) glm::uvec4 lightCounts;
  PointLightGpu pointLights[MAX_POINT_LIGHTS];
};

static_assert(sizeof(PointLightGpu) == 48);
static_assert(sizeof(SceneBufferObject) % 16 == 0);

struct ObjectPushConstants {
  glm::mat4 model;
  uint32_t unlit;
};

struct AnimatedObjectPushConstants {
  glm::mat4 model;
  uint32_t unlit;
  uint32_t previousPositionOffset;
  uint32_t nextPositionOffset;
  float interpolation;
  uint32_t vertexCount;
};

// Different command buffer recordings.
