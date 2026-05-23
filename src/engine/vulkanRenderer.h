#pragma once

#define VULKAN_HPP_NO_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vector>

#include "vulkanGlobals.h"

struct Vertex
{
    glm::vec2 pos;
};

struct Mesh
{
    vk::raii::Buffer vertexBuffer = nullptr;
    vk::raii::DeviceMemory vertexDeviceMemory = nullptr;
    uint32_t vertexCount;
};

struct Object3D
{
    const Mesh *mesh;
    glm::mat4 model;
};

extern Mesh triangleMesh;

extern std::vector<Object3D> objects;

// Will store/handle every vulkan stuff that can change depending of how we decide that the renderer
// will render stuff.

struct VulkanRendererContext
{
    // Per "way of rendering" / shader / contract. Set normally once.
    vk::raii::DescriptorSetLayout descriptorSetLayout = nullptr;
    vk::raii::DescriptorPool descriptorPool = nullptr;
    std::vector<vk::raii::DescriptorSet> descriptorSets;
    vk::raii::PipelineLayout pipelineLayout{nullptr};
    vk::raii::Pipeline graphicsPipeline{nullptr};

    // Per frame?
    std::vector<vk::raii::CommandBuffer> commandBuffers;
    std::vector<vk::raii::Buffer> uniformBuffers;
    std::vector<vk::raii::DeviceMemory> uniformBuffersMemory;
    std::vector<void *> uniformBuffersMapped;

    // Sync objects
    std::vector<vk::raii::Semaphore> imageAvailableSemaphores;
    std::vector<vk::raii::Semaphore> renderFinishedSemaphores;
    std::vector<vk::raii::Fence> inFlightFences;
    uint32_t currentFrame = 0;
};

extern VulkanRendererContext vulkanRendererContext;

void setupRenderer();
void drawFrame();