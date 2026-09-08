#pragma once

#include "glm/fwd.hpp"
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <cstdint>
#include <filesystem>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <vector>
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

namespace Blender::V2 {

struct Vertex {
  glm::vec3 pos{};
  glm::vec2 uv{};
  glm::vec3 normal{};

  static vk::VertexInputBindingDescription getBindingDescription() {
    return {.binding = 0,
            .stride = sizeof(Vertex),
            .inputRate = vk::VertexInputRate::eVertex};
  }
  static std::array<vk::VertexInputAttributeDescription, 3>
  getAttributeDescriptions() {
    return {{
        {.location = 0,
         .binding = 0,
         .format = vk::Format::eR32G32B32Sfloat,
         .offset = offsetof(Vertex, pos)},
        {.location = 1,
         .binding = 0,
         .format = vk::Format::eR32G32Sfloat,
         .offset = offsetof(Vertex, uv)},
        {.location = 2,
         .binding = 0,
         .format = vk::Format::eR32G32B32Sfloat,
         .offset = offsetof(Vertex, normal)},
    }};
  }
};

struct AnimationKeyPose {
  int blenderFrame = 0;
  std::vector<glm::vec3> positions;
  std::vector<glm::vec3> normals;
};

struct TransformAnimationKeyPose {
  int blenderFrame = 0;
  glm::vec3 location{};
  glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
  glm::vec3 scale{1.0f};
};

struct AttachmentAnimationKeyPose {
  int blenderFrame = 0;
  glm::vec3 location{};
  glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
  glm::vec3 scale{1.0f};
};

struct AnimationAttachment {
  std::string objectName;
  std::string boneName;
  std::vector<AttachmentAnimationKeyPose> keyPoses;
};

enum class AnimationKind { Vertex, Transform };

struct AnimationClip {
  std::string name;
  AnimationKind kind = AnimationKind::Vertex;
  int startFrame = 0;
  int endFrame = 0;
  std::vector<AnimationKeyPose> keyPoses;
  std::vector<TransformAnimationKeyPose> transformKeyPoses;
  std::vector<AnimationAttachment> attachments;
  bool loop = false;
};

struct BlenderModel {
  std::string name;
  std::filesystem::path texturePath;
  float fps = 24.0f;
  std::vector<Vertex> vertices;
  std::vector<std::uint32_t> indices;
  std::vector<AnimationClip> animations;
};

struct ImporterTransform {
  glm::vec3 position;
  glm::vec3 rotation;
  glm::vec3 scale;
};

struct Camera {
  ImporterTransform transform;
  glm::vec3 direction;
  float fovY = glm::radians(45.0f);
  float clipStart = 0.1f;
  float clipEnd = 100.0f;
};

struct Wizards {
  int count;
  std::vector<glm::vec3> positions;
};

struct Ogres {
  int count;
  std::vector<glm::vec3> positions;
};

struct TransformAnimationClip {
  std::string name;
  int startFrame = 0;
  int endFrame = 0;
  std::vector<TransformAnimationKeyPose> keyPoses;
  bool loop = false;
};

struct BlenderTransformModel {
  std::string name;
  std::filesystem::path texturePath;
  float fps = 24.0f;
  std::vector<Vertex> vertices;
  std::vector<std::uint32_t> indices;
  std::vector<TransformAnimationClip> animations;
};

BlenderModel loadModel(const std::string &path);
BlenderTransformModel loadTransformModel(const std::string &path);
}; // namespace Blender::V2
