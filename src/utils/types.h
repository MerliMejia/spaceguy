#pragma once

#include "glm/fwd.hpp"
#define VULKAN_HPP_NO_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#include "../engine/renderer/bufferUtils.h"
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
  glm::vec3 normal;

  static vk::VertexInputBindingDescription getBindingDescription() {
    return {.binding = 0, .stride = sizeof(Vertex), .inputRate = vk::VertexInputRate::eVertex};
  }
  static std::array<vk::VertexInputAttributeDescription, 3> getAttributeDescriptions() {
    return {{
        {.location = 0, .binding = 0, .format = vk::Format::eR32G32B32Sfloat, .offset = offsetof(Vertex, pos)},
        {.location = 1, .binding = 0, .format = vk::Format::eR32G32B32Sfloat, .offset = offsetof(Vertex, color)},
        {.location = 2, .binding = 0, .format = vk::Format::eR32G32B32Sfloat, .offset = offsetof(Vertex, normal)},
    }};
  }
};

struct AnimatedVertex {
  glm::vec3 color;
  glm::vec3 normal;
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

namespace Renderer {
namespace Types {
struct Mesh {
  Renderer::BufferAllocation vertexAllocations;
  Renderer::BufferAllocation indexAllocations;
  uint32_t vertexCount = 0;
  uint32_t indexCount = 0;
};

struct AnimatedMesh {
  float fps = 60.0f;
  std::vector<AnimationClipGpu> animations;
  std::vector<AnimationKeyPoseGpu> keyPoses;
  Renderer::Types::Mesh mesh;
};

struct Particle {
  glm::vec3 position = glm::vec3(0.0f);
  float speed = 1.0f;
  glm::vec3 dir = glm::vec3(0.0f);
  float maxDistance = 10.0f;
  float distance = 0.0f;
  int isTooFar = 0;
  glm::vec2 _pad1 = glm::vec2(0.0f);
};
} // namespace Types
} // namespace Renderer

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

enum class ParticleEmitterShape { Cone };
