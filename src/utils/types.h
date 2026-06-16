#pragma once

#define VULKAN_HPP_NO_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include <vector>

struct DebugVertex {
  glm::vec3 position;
  glm::vec4 color;
};

struct DebugFrameData {
  vk::raii::Buffer vertexBuffer{nullptr};
  vk::raii::DeviceMemory vertexMemory{nullptr};
  void *mapped = nullptr;
  uint32_t vertexCount = 0;
};

struct Vertex {
  glm::vec3 pos;
  glm::vec3 color;
};

struct AnimatedVertex {
  glm::vec3 color;
};

struct Mesh {
  vk::raii::Buffer vertexBuffer = nullptr;
  vk::raii::DeviceMemory vertexDeviceMemory = nullptr;
  vk::raii::Buffer indexBuffer = nullptr;
  vk::raii::DeviceMemory indexDeviceMemory = nullptr;
  uint32_t vertexCount = 0;
  uint32_t indexCount = 0;
};

struct AnimationKeyPoseGpu {
  uint32_t positionOffset = 0;
  int blenderFrame = 0;
};

struct AnimationClipGpu {
  std::string name;
  int startFrame = 0;
  int endFrame = 0;
  uint32_t firstKeyPose = 0;
  uint32_t keyPoseCount = 0;
  bool loop = false;
};

struct AnimatedMesh {
  Mesh mesh;
  float fps = 60.0f;
  std::vector<AnimationClipGpu> animations;
  std::vector<AnimationKeyPoseGpu> keyPoses;
};

enum WizardAnimationMapping : uint32_t {
  Attacking = 0,
  BeingAttacked = 1,
  Iddle = 2,
  Kicking = 3,
  Running = 4
};
