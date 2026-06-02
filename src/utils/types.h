#pragma once

#define VULKAN_HPP_NO_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cstdint>
#include <string>
#include <vector>

struct Vertex
{
    glm::vec3 pos;
    glm::vec3 color;
};

struct AnimatedVertex
{
    glm::vec3 color;
};

struct Mesh
{
    vk::raii::Buffer vertexBuffer = nullptr;
    vk::raii::DeviceMemory vertexDeviceMemory = nullptr;
    vk::raii::Buffer indexBuffer = nullptr;
    vk::raii::DeviceMemory indexDeviceMemory = nullptr;
    uint32_t vertexCount = 0;
    uint32_t indexCount = 0;
};

struct AnimationFrameGpu
{
    uint32_t positionOffset = 0;
    int blenderFrame = 0;
};

struct AnimationClipGpu
{
    std::string name;
    uint32_t firstFrame = 0;
    uint32_t frameCount = 0;
};

struct AnimatedMesh
{
    Mesh mesh;
    std::vector<AnimationClipGpu> animations;
    std::vector<AnimationFrameGpu> frames;
};