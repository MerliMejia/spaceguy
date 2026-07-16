#pragma once

#define VULKAN_HPP_NO_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
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

struct AttachmentAnimationClipGpuAttachmentKeyPose {
  int blenderFrame = 0;
  glm::vec3 location{};
  glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
  glm::vec3 scale{1.0f};
};

struct AnimationClipGpuAttachment {
  std::vector<AttachmentAnimationClipGpuAttachmentKeyPose> keyPoses;
};

struct AnimationClipGpu {
  std::string name;
  int startFrame = 0;
  int endFrame = 0;
  uint32_t firstKeyPose = 0;
  uint32_t keyPoseCount = 0;
  bool loop = false;
  std::vector<AnimationClipGpuAttachment> attachments;
};

struct AnimatedMesh {
  Mesh mesh;
  float fps = 60.0f;
  std::vector<AnimationClipGpu> animations;
  std::vector<AnimationKeyPoseGpu> keyPoses;
};

enum WizardAnimationMapping : uint32_t {
  Attacking = 0,
  Iddle = 1,
  Kicking = 2,
  Running = 3,
};

struct TransformAnimationKeyPoseGPU {
  glm::vec3 location{};
  glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
  glm::vec3 scale{1.0f};
  int blenderFrame = 0;
};

struct TransformAnimatedMesh {
  Mesh mesh;
  float fps = 60.0f;
  std::vector<AnimationClipGpu> animations;
  std::vector<TransformAnimationKeyPoseGPU> keyPoses;
};

struct ParticleGpu {
  glm::vec4 positionLifeTime; // xyz = position, w = lifetime
  glm::vec4 velocitySize;     // xyz = velocity, w = quad size
  glm::vec4 color;
  glm::vec4 state; // x = initial lifetime, yzw = spare
  glm::uvec4 meta; // x = emitter index, y = active, z = seed, w = spare
};

struct ParticleSimUbo {
  float deltaTime;
  uint32_t particleCount;
  uint32_t emitterCount;
  uint32_t frameIndex;
};

enum class ParticleEmitterShape : uint32_t { Cone = 0, Sphere = 1 };

struct ParticleEmitterGpu {
  glm::vec4 position;
  glm::vec4 direction;
  glm::vec4 worldVelocitySpawnRate; // xyz = emitter velocity, w = spawn/sec
  glm::vec4
      config; // x = lifetime, y = start size, z = speed, w = max color speed
  glm::vec4 sizeConfig; // x = end size, yzw = spare
  glm::vec4 lifeColorStart;
  glm::vec4 lifeColorEnd;
  glm::vec4 speedColorSlow;
  glm::vec4 speedColorFast;
  glm::uvec4 rangeActive; // x = first, y = max, z = active, w = spawn budget
  glm::uvec4 shape;       // x = ParticleEmitterShape, yzw = spare
};
