#pragma once

#include "../engine/blender/importer.h"
#include "../systems/resourceManagementSystem.h"
#include "./buffers.h"

#include <cstring>

inline Mesh generateMesh(const std::vector<Vertex> &vertices,
                         const std::vector<uint16_t> &indices) {
  Mesh mesh{};

  // Vertices
  vk::DeviceSize verticesBufferSize = sizeof(vertices[0]) * vertices.size();

  auto vertexBuffer =
      createBuffer(verticesBufferSize, vk::BufferUsageFlagBits::eVertexBuffer,
                   vk::MemoryPropertyFlagBits::eHostVisible |
                       vk::MemoryPropertyFlagBits::eHostCoherent);

  void *verticesData = vertexBuffer.memory.mapMemory(0, verticesBufferSize);
  memcpy(verticesData, vertices.data(),
         static_cast<size_t>(verticesBufferSize));
  vertexBuffer.memory.unmapMemory();

  mesh.vertexBuffer = std::move(vertexBuffer.buffer);
  mesh.vertexDeviceMemory = std::move(vertexBuffer.memory);
  mesh.vertexCount = static_cast<uint32_t>(vertices.size());

  // Indices
  vk::DeviceSize indicesBufferSize = sizeof(indices[0]) * indices.size();

  auto indexBuffer =
      createBuffer(indicesBufferSize, vk::BufferUsageFlagBits::eIndexBuffer,
                   vk::MemoryPropertyFlagBits::eHostVisible |
                       vk::MemoryPropertyFlagBits::eHostCoherent);

  void *indicesData = indexBuffer.memory.mapMemory(0, indicesBufferSize);
  memcpy(indicesData, indices.data(), static_cast<size_t>(indicesBufferSize));
  indexBuffer.memory.unmapMemory();

  mesh.indexBuffer = std::move(indexBuffer.buffer);
  mesh.indexDeviceMemory = std::move(indexBuffer.memory);
  mesh.indexCount = static_cast<uint32_t>(indices.size());

  return mesh;
}

inline AnimatedMesh generateAnimatedMesh(const BlenderModel &model,
                                         uint32_t firstGlobalPositionOffset) {
  AnimatedMesh animated{};
  animated.fps = model.fps;

  std::vector<AnimatedVertex> vertices;
  vertices.reserve(model.vertices.size());

  for (const Vertex &vertex : model.vertices) {
    vertices.push_back(AnimatedVertex{
        .color = vertex.color,
    });
  }

  vk::DeviceSize verticesBufferSize = sizeof(vertices[0]) * vertices.size();

  auto vertexBuffer =
      createBuffer(verticesBufferSize, vk::BufferUsageFlagBits::eVertexBuffer,
                   vk::MemoryPropertyFlagBits::eHostVisible |
                       vk::MemoryPropertyFlagBits::eHostCoherent);

  void *verticesData = vertexBuffer.memory.mapMemory(0, verticesBufferSize);
  memcpy(verticesData, vertices.data(),
         static_cast<size_t>(verticesBufferSize));
  vertexBuffer.memory.unmapMemory();

  animated.mesh.vertexBuffer = std::move(vertexBuffer.buffer);
  animated.mesh.vertexDeviceMemory = std::move(vertexBuffer.memory);
  animated.mesh.vertexCount = static_cast<uint32_t>(vertices.size());

  vk::DeviceSize indicesBufferSize =
      sizeof(model.indices[0]) * model.indices.size();

  auto indexBuffer =
      createBuffer(indicesBufferSize, vk::BufferUsageFlagBits::eIndexBuffer,
                   vk::MemoryPropertyFlagBits::eHostVisible |
                       vk::MemoryPropertyFlagBits::eHostCoherent);

  void *indicesData = indexBuffer.memory.mapMemory(0, indicesBufferSize);
  memcpy(indicesData, model.indices.data(),
         static_cast<size_t>(indicesBufferSize));
  indexBuffer.memory.unmapMemory();

  animated.mesh.indexBuffer = std::move(indexBuffer.buffer);
  animated.mesh.indexDeviceMemory = std::move(indexBuffer.memory);
  animated.mesh.indexCount = static_cast<uint32_t>(model.indices.size());

  uint32_t runningKeyPoseIndex = 0;
  uint32_t runningPositionOffset = firstGlobalPositionOffset;

  for (const AnimationClip &clip : model.animations) {
    AnimationClipGpu gpuClip{.name = clip.name,
                             .startFrame = clip.startFrame,
                             .endFrame = clip.endFrame,
                             .firstKeyPose = runningKeyPoseIndex,
                             .keyPoseCount =
                                 static_cast<uint32_t>(clip.keyPoses.size()),
                             .loop = clip.loop};

    animated.animations.push_back(gpuClip);

    for (const AnimationKeyPose &keyPose : clip.keyPoses) {
      animated.keyPoses.push_back(AnimationKeyPoseGpu{
          .positionOffset = runningPositionOffset,
          .blenderFrame = keyPose.blenderFrame,
      });

      runningPositionOffset += static_cast<uint32_t>(keyPose.positions.size());
      runningKeyPoseIndex++;
    }
  }

  return animated;
}

inline TransformAnimatedMesh
generateTransformAnimatedMesh(const BlenderTransformModel &model) {
  TransformAnimatedMesh animated{};
  animated.fps = model.fps;
  animated.mesh = generateMesh(model.vertices, model.indices);

  uint32_t runningKeyPoseIndex = 0;

  for (const TransformAnimationClip &clip : model.animations) {
    animated.animations.push_back(AnimationClipGpu{
        .name = clip.name,
        .startFrame = clip.startFrame,
        .endFrame = clip.endFrame,
        .firstKeyPose = runningKeyPoseIndex,
        .keyPoseCount = static_cast<uint32_t>(clip.keyPoses.size()),
        .loop = clip.loop,
    });

    for (const TransformAnimationKeyPose &keyPose : clip.keyPoses) {
      animated.keyPoses.push_back(TransformAnimationKeyPoseGPU{
          .location = keyPose.location,
          .rotation = keyPose.rotation,
          .scale = keyPose.scale,
          .blenderFrame = keyPose.blenderFrame,
      });
      runningKeyPoseIndex++;
    }
  }

  return animated;
}

inline Mesh generateQuadMesh() {
  std::vector<Vertex> vertices{
      Vertex{
          .pos = {-0.5f, -0.5f, 0.0f},
          .color = {1.0f, 1.0f, 1.0f},
      },
      Vertex{
          .pos = {0.5f, -0.5f, 0.0f},
          .color = {1.0f, 1.0f, 1.0f},
      },
      Vertex{
          .pos = {0.5f, 0.5f, 0.0f},
          .color = {1.0f, 1.0f, 1.0f},
      },
      Vertex{
          .pos = {-0.5f, 0.5f, 0.0f},
          .color = {1.0f, 1.0f, 1.0f},
      },
  };

  std::vector<uint16_t> indices{
      0, 1, 2, 2, 3, 0,
  };

  return generateMesh(vertices, indices);
}

inline void
generateLongTrailParticleEmitter(ParticleEmitterCpuComponent &emitter,
                                 glm::vec3 position, glm::vec3 direction) {
  emitter.maxParticles = 1600;
  emitter.active = true;
  emitter.spawnRate = 260.0f;
  emitter.particleLifetime = 2.0f;
  emitter.particleStartSize = 1.0f;
  emitter.particleEndSize = 0.01f;
  emitter.spawnSpeed = 1.4f;
  emitter.maxColorSpeed = 3.0f;
  emitter.lifeColorStart = glm::vec4{1.0f};
  emitter.lifeColorEnd = glm::vec4{1.0f, 1.0f, 1.0f, 0.0f};
  emitter.speedColorSlow = glm::vec4{1.0f};
  emitter.speedColorFast = glm::vec4{1.0f};
  emitter.position = position;
  emitter.direction = -direction;
  emitter.shape = ParticleEmitterShape::Cone;
}

inline void
generateExplosionParticleEmitter(ParticleEmitterCpuComponent &emitter,
                                 glm::vec3 position, float radius) {
  const float lifetime = 0.55f;

  emitter.maxParticles = 420;
  emitter.active = true;
  emitter.spawnRate = 420.0f;
  emitter.particleLifetime = lifetime;
  emitter.particleStartSize = radius * 0.2f;
  emitter.particleEndSize = 0.01f;
  emitter.spawnSpeed = (radius / lifetime) * 2;
  emitter.maxColorSpeed = emitter.spawnSpeed;
  emitter.lifeColorStart = glm::vec4{1.0f, 0.7f, 0.18f, 1.0f};
  emitter.lifeColorEnd = glm::vec4{0.9f, 0.08f, 0.02f, 0.0f};
  emitter.speedColorSlow = glm::vec4{0.9f, 0.08f, 0.02f, 1.0f};
  emitter.speedColorFast = glm::vec4{1.0f, 0.9f, 0.2f, 1.0f};
  emitter.position = position;
  emitter.shape = ParticleEmitterShape::Sphere;
}
