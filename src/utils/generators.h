#pragma once

#include "./buffers.h"
#include "../engine/blender/importer.h"

#include <cstring>

Mesh generateMesh(const std::vector<Vertex> &vertices, const std::vector<uint16_t> &indices)
{
    Mesh mesh{};

    // Vertices
    vk::DeviceSize verticesBufferSize = sizeof(vertices[0]) * vertices.size();

    auto vertexBuffer = createBuffer(verticesBufferSize,
                                     vk::BufferUsageFlagBits::eVertexBuffer,
                                     vk::MemoryPropertyFlagBits::eHostVisible |
                                         vk::MemoryPropertyFlagBits::eHostCoherent);

    void *verticesData = vertexBuffer.memory.mapMemory(0, verticesBufferSize);
    memcpy(verticesData, vertices.data(), static_cast<size_t>(verticesBufferSize));
    vertexBuffer.memory.unmapMemory();

    mesh.vertexBuffer = std::move(vertexBuffer.buffer);
    mesh.vertexDeviceMemory = std::move(vertexBuffer.memory);
    mesh.vertexCount = static_cast<uint32_t>(vertices.size());

    // Indices
    vk::DeviceSize indicesBufferSize = sizeof(indices[0]) * indices.size();

    auto indexBuffer = createBuffer(indicesBufferSize,
                                    vk::BufferUsageFlagBits::eIndexBuffer,
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

AnimatedMesh generateAnimatedMesh(
    const BlenderModel &model,
    uint32_t firstGlobalPositionOffset)
{
    AnimatedMesh animated{};
    animated.fps = model.fps;

    std::vector<AnimatedVertex> vertices;
    vertices.reserve(model.vertices.size());

    for (const Vertex &vertex : model.vertices)
    {
        vertices.push_back(AnimatedVertex{
            .color = vertex.color,
        });
    }

    vk::DeviceSize verticesBufferSize = sizeof(vertices[0]) * vertices.size();

    auto vertexBuffer = createBuffer(
        verticesBufferSize,
        vk::BufferUsageFlagBits::eVertexBuffer,
        vk::MemoryPropertyFlagBits::eHostVisible |
            vk::MemoryPropertyFlagBits::eHostCoherent);

    void *verticesData = vertexBuffer.memory.mapMemory(0, verticesBufferSize);
    memcpy(verticesData, vertices.data(), static_cast<size_t>(verticesBufferSize));
    vertexBuffer.memory.unmapMemory();

    animated.mesh.vertexBuffer = std::move(vertexBuffer.buffer);
    animated.mesh.vertexDeviceMemory = std::move(vertexBuffer.memory);
    animated.mesh.vertexCount = static_cast<uint32_t>(vertices.size());

    vk::DeviceSize indicesBufferSize = sizeof(model.indices[0]) * model.indices.size();

    auto indexBuffer = createBuffer(
        indicesBufferSize,
        vk::BufferUsageFlagBits::eIndexBuffer,
        vk::MemoryPropertyFlagBits::eHostVisible |
            vk::MemoryPropertyFlagBits::eHostCoherent);

    void *indicesData = indexBuffer.memory.mapMemory(0, indicesBufferSize);
    memcpy(indicesData, model.indices.data(), static_cast<size_t>(indicesBufferSize));
    indexBuffer.memory.unmapMemory();

    animated.mesh.indexBuffer = std::move(indexBuffer.buffer);
    animated.mesh.indexDeviceMemory = std::move(indexBuffer.memory);
    animated.mesh.indexCount = static_cast<uint32_t>(model.indices.size());

    uint32_t runningKeyPoseIndex = 0;
    uint32_t runningPositionOffset = firstGlobalPositionOffset;

    for (const AnimationClip &clip : model.animations)
    {
        AnimationClipGpu gpuClip{
            .name = clip.name,
            .startFrame = clip.startFrame,
            .endFrame = clip.endFrame,
            .firstKeyPose = runningKeyPoseIndex,
            .keyPoseCount = static_cast<uint32_t>(clip.keyPoses.size())};

        animated.animations.push_back(gpuClip);

        for (const AnimationKeyPose &keyPose : clip.keyPoses)
        {
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