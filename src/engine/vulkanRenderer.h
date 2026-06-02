#pragma once

#define VULKAN_HPP_NO_CONSTRUCTORS
#include <vulkan/vulkan_raii.hpp>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vector>
#include <cstring>

#include "vulkanGlobals.h"
#include "../utils/types.h"

enum class ObjectRenderKind
{
    Static,
    Animated
};

struct Object3D
{
    ObjectRenderKind renderKind = ObjectRenderKind::Static;
    const Mesh *mesh = nullptr;
    const AnimatedMesh *animatedMesh = nullptr;
    glm::mat4 model;

    uint32_t activeAnimation = 0;
    uint32_t activeFrame = 0;

    float animationTimeSeconds = 0.0f;
    float animationPlaySpeed = 1.0f;
};

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
    // Animated
    vk::raii::DescriptorSetLayout animatedDescriptorSetLayout = nullptr;
    vk::raii::DescriptorPool animatedDescriptorPool = nullptr;
    std::vector<vk::raii::DescriptorSet> animatedDescriptorSets;
    vk::raii::PipelineLayout animatedPipelineLayout = nullptr;
    vk::raii::Pipeline animatedGraphicsPipeline = nullptr;

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

    // Depth testing
    vk::Format depthFormat = vk::Format::eD32Sfloat;
    vk::raii::Image depthImage = nullptr;
    vk::raii::DeviceMemory depthImageMemory = nullptr;
    vk::raii::ImageView depthImageView = nullptr;

    // To actually draw
    std::vector<Object3D> objects;

    // Animations
    vk::raii::Buffer animationPositionsBuffer = nullptr;
    vk::raii::DeviceMemory animationPositionsMemory = nullptr;
    uint32_t animationPositionCount = 0;
};

extern VulkanRendererContext vulkanRendererContext;

void setupRendererCore();
void setupRendererAfterAssetsLoaded();
void drawFrame();

void uploadAnimationPositions(const std::vector<glm::vec4> &positions);