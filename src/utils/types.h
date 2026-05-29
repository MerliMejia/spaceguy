#pragma once

#define VULKAN_HPP_NO_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct Vertex
{
    glm::vec3 pos;
    glm::vec3 color;
};

struct Mesh
{
    vk::raii::Buffer vertexBuffer = nullptr;
    vk::raii::DeviceMemory vertexDeviceMemory = nullptr;
    vk::raii::Buffer indexBuffer = nullptr;
    vk::raii::DeviceMemory indexDeviceMemory = nullptr;
    uint32_t vertexCount;
    uint32_t indexCount;
};