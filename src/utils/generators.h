#pragma once

#include "../engine/blender/v2/importer.h"
#include "../engine/renderer/renderNode/renderNodeUtils.h"
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
