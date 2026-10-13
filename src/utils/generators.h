#pragma once

#include "../engine/blender/v2/importer.h"
#include "../engine/renderer/renderNode/renderNodeUtils.h"
#include "glm/fwd.hpp"
#include "types.h"
#include <vector>

namespace Renderer {
namespace Generators {

template <RenderNodeUtils::VertexType T>
Renderer::Types::Mesh
generateMesh(const std::vector<T> vertices, const std::vector<uint32_t> indices,
             vk::raii::CommandPool &commandPool, VDevice &vDevice) {

  Renderer::Types::Mesh mesh{};

  BufferAllocation vertexAllocation = createDeviceLocalBuffer(
      vDevice, commandPool, std::span<const T>{vertices},
      vk::BufferUsageFlagBits::eVertexBuffer);

  BufferAllocation indexAllocation = createDeviceLocalBuffer(
      vDevice, commandPool, std::span<const uint32_t>{indices},
      vk::BufferUsageFlagBits::eIndexBuffer);

  mesh.vertexAllocations = std::move(vertexAllocation);
  mesh.indexAllocations = std::move(indexAllocation);
  mesh.vertexCount = vertices.size();
  mesh.indexCount = indices.size();

  return mesh;
}

inline Renderer::Types::Mesh quad(vk::raii::CommandPool &commandPool,
                                  VDevice &vDevice) {
  // Unit quad centered on the origin in local XY, facing +Z.
  std::vector<Blender::V2::Vertex> vertices{
      Blender::V2::Vertex{
          .pos = glm::vec3(-0.5f, -0.5f, 0.0f),
          .uv = glm::vec2(0.0f, 0.0f),
          .normal = glm::vec3(0.0f, 0.0f, 1.0f),
      },
      Blender::V2::Vertex{
          .pos = glm::vec3(-0.5f, 0.5f, 0.0f),
          .uv = glm::vec2(0.0f, 1.0f),
          .normal = glm::vec3(0.0f, 0.0f, 1.0f),
      },
      Blender::V2::Vertex{
          .pos = glm::vec3(0.5f, 0.5f, 0.0f),
          .uv = glm::vec2(1.0f, 1.0f),
          .normal = glm::vec3(0.0f, 0.0f, 1.0f),
      },
      Blender::V2::Vertex{
          .pos = glm::vec3(0.5f, -0.5f, 0.0f),
          .uv = glm::vec2(1.0f, 0.0f),
          .normal = glm::vec3(0.0f, 0.0f, 1.0f),
      }};

  // Counter-clockwise when seen from +Z (toward the camera in view space).
  std::vector<uint32_t> indices{0, 2, 1, 0, 3, 2};

  return generateMesh(vertices, indices, commandPool, vDevice);
};

inline Renderer::Types::AnimatedMesh
generateAnimatedMesh(const Blender::V2::BlenderModel &model,
                     uint32_t firstGlobalPositionOffset,
                     vk::raii::CommandPool &commandPool, VDevice &vDevice) {

  Renderer::Types::AnimatedMesh animated{};
  animated.fps = model.fps;

  std::vector<Blender::V2::AnimatedVertex> animatedVertices;

  for (const auto v : model.vertices) {
    animatedVertices.push_back(Blender::V2::AnimatedVertex{.uv = v.uv});
  }

  BufferAllocation vertexAllocation = createDeviceLocalBuffer(
      vDevice, commandPool,
      std::span<const Blender::V2::AnimatedVertex>{animatedVertices},
      vk::BufferUsageFlagBits::eVertexBuffer);

  BufferAllocation indexAllocation = createDeviceLocalBuffer(
      vDevice, commandPool, std::span<const uint32_t>{model.indices},
      vk::BufferUsageFlagBits::eIndexBuffer);

  animated.mesh.vertexAllocations = std::move(vertexAllocation);
  animated.mesh.indexAllocations = std::move(indexAllocation);
  animated.mesh.vertexCount = model.vertices.size();
  animated.mesh.indexCount = model.indices.size();

  uint32_t runningKeyPoseIndex = 0;
  uint32_t runningPositionOffset = firstGlobalPositionOffset;

  for (const auto &clip : model.animations) {

    std::vector<AnimationClipGpuAttachment> attachments;
    attachments.reserve(clip.attachments.size());

    for (auto clipAttachment : clip.attachments) {
      std::vector<AttachmentAnimationClipGpuAttachmentKeyPose>
          attachmentKeyPoses;
      attachmentKeyPoses.reserve(clipAttachment.keyPoses.size());

      for (auto attachmentKeyPose : clipAttachment.keyPoses) {
        attachmentKeyPoses.push_back(
            AttachmentAnimationClipGpuAttachmentKeyPose{
                .blenderFrame = attachmentKeyPose.blenderFrame,
                .location = attachmentKeyPose.location,
                .rotation = attachmentKeyPose.rotation,
                .scale = attachmentKeyPose.scale});
      }

      attachments.push_back(
          AnimationClipGpuAttachment{.keyPoses = attachmentKeyPoses});
    }

    AnimationClipGpu gpuClip{.name = clip.name,
                             .startFrame = clip.startFrame,
                             .endFrame = clip.endFrame,
                             .firstKeyPose = runningKeyPoseIndex,
                             .keyPoseCount =
                                 static_cast<uint32_t>(clip.keyPoses.size()),
                             .loop = clip.loop,
                             .attachments = attachments};

    animated.animations.push_back(gpuClip);

    for (const auto &keyPose : clip.keyPoses) {
      animated.keyPoses.push_back(AnimationKeyPoseGpu{
          .positionOffset = runningPositionOffset,
          .blenderFrame = keyPose.blenderFrame,
      });

      runningPositionOffset +=
          static_cast<uint32_t>(keyPose.positions.size() * 2);
      runningKeyPoseIndex++;
    }
  }

  return animated;
}

} // namespace Generators
} // namespace Renderer
