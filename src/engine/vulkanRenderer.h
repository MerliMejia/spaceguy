#pragma once

#define VULKAN_HPP_NO_CONSTRUCTORS
#include "vulkan/vulkan.hpp"
#include <cstdint>
#include <vulkan/vulkan_raii.hpp>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cstring>
#include <vector>

#include "../utils/types.h"
#include "vulkanGlobals.h"

struct RendererConfig {
  float renderScale = 1.0f;
  vk::SampleCountFlagBits preferredMsaaSamples = vk::SampleCountFlagBits::e4;
  vk::Filter upscaleFilter = vk::Filter::eCubicEXT;
};

// Will store/handle every vulkan stuff that can change depending of how we
// decide that the renderer will render stuff.

struct VulkanRendererContext {
  RendererConfig config{};
  vk::Extent2D renderExtent{};

  // Particles. At some point should be more like "compute stuff".
  vk::raii::DescriptorSetLayout particleDescriptorSetLayout = nullptr;
  vk::raii::DescriptorPool particleDescriptorPool = nullptr;
  std::vector<vk::raii::DescriptorSet> particleDescriptorSets;

  vk::raii::PipelineLayout particleComputePipelineLayout = nullptr;
  vk::raii::Pipeline particleComputePipeline = nullptr;

  vk::raii::PipelineLayout particleGraphicsPipelineLayout = nullptr;
  vk::raii::Pipeline particleGraphicsPipeline = nullptr;

  vk::raii::Buffer particleBuffer = nullptr;
  vk::raii::DeviceMemory particleBufferMemory = nullptr;
  void *particleBufferMapped = nullptr;
  std::vector<vk::raii::Buffer> particleEmitterBuffers;
  std::vector<vk::raii::DeviceMemory> particleEmitterMemory;
  std::vector<void *> particleEmitterMapped;
  std::vector<vk::raii::Buffer> particleSpawnCounterBuffers;
  std::vector<vk::raii::DeviceMemory> particleSpawnCounterMemory;
  std::vector<void *> particleSpawnCounterMapped;
  std::vector<vk::raii::Buffer> particleSimParamsBuffers;
  std::vector<vk::raii::DeviceMemory> particleSimParamsMemory;
  std::vector<void *> particleSimParamsMapped;

  uint32_t particleCount = 0;
  uint32_t particleSimulationFrame = 0;
  Mesh particleQuadMesh;

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
  // Debug
  bool isDebug = false;
  vk::raii::PipelineLayout debugPipelineLayout = nullptr;
  vk::raii::Pipeline debugGraphicsPipeline = nullptr;
  std::vector<DebugFrameData> debugFrames;
  std::vector<DebugVertex> debugVertices;

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
  std::vector<vk::raii::Image> depthImages;
  std::vector<vk::raii::DeviceMemory> depthImageMemory;
  std::vector<vk::raii::ImageView> depthImageViews;

  // Multi-sampling
  vk::SampleCountFlagBits msaaSamples = vk::SampleCountFlagBits::e1;
  std::vector<vk::raii::Image> colorImages;
  std::vector<vk::raii::DeviceMemory> colorImageMemory;
  std::vector<vk::raii::ImageView> colorImageViews;
  std::vector<vk::raii::Image> resolveImages;
  std::vector<vk::raii::DeviceMemory> resolveImageMemory;
  std::vector<vk::raii::ImageView> resolveImageViews;

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

void clearDebugShapes();
void addDebugDiskXY(glm::vec3 center, float radius, glm::vec4 color);
void addDebugLine(glm::vec3 a, glm::vec3 b, glm::vec4 color);
void addDebugCube(glm::vec3 center, float radius, glm::vec4 color);
void addDebugSphere(glm::vec3 center, float radius, glm::vec4 color);
void addDebugCellXY(glm::vec3 origin, float width, float height,
                    glm::vec4 color);
void addDebugGridCellsXY(glm::vec3 origin, uint32_t width, uint32_t height,
                         float cellWidth, float cellHeight, glm::vec4 color);
