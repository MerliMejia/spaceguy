#pragma once

#include "./buffers.h"

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