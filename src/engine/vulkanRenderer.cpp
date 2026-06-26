#include "vulkanRenderer.h"
#include "../systems/animationSystem.h"
#include "../systems/resourceManagementSystem.h"
#include "../systems/sceneContext.h"
#include "../utils/buffers.h"
#include "../utils/generators.h"
#include "./predefined/vulkanDescriptorSetLayouts.h"
#include "./predefined/vulkanGraphicPipelines.h"
#include "./vulkanBackend.h"
#include "vulkan/vulkan.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <limits>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>
#include <vulkan/vulkan.hpp>

VulkanRendererContext vulkanRendererContext{};

constexpr uint32_t WORKGROUP_SIZE = 256;
constexpr uint32_t MAX_PARTICLE_EMITTERS = 128;
constexpr uint32_t MAX_PARTICLES = 50000;
constexpr uint32_t INVALID_PARTICLE_EMITTER_SLOT =
    std::numeric_limits<uint32_t>::max();

struct ParticleRange {
  uint32_t first = 0;
  uint32_t count = 0;
};

struct ParticleEmitterAllocation {
  uint32_t gpuEmitterIndex = INVALID_PARTICLE_EMITTER_SLOT;
  uint32_t firstParticle = 0;
  uint32_t maxParticles = 0;
  bool seenThisFrame = false;
};

static std::unordered_map<int, ParticleEmitterAllocation>
    particleEmitterAllocations;
static std::vector<uint32_t> freeParticleEmitterSlots;
static std::vector<ParticleRange> freeParticleRanges;

void createDescriptorSetLayout() {
  PARTICLE_COMPUTE_DESCRIPTOR_SET_LAYOUT(
      vulkanRendererContext.particleDescriptorSetLayout, vulkanContext.device);
  DEFAULT_DESCRIPTOR_SET_LAYOUT(vulkanRendererContext.descriptorSetLayout,
                                vulkanContext.device);
  ANIMATED_DESCRIPTOR_SET_LAYOUT(
      vulkanRendererContext.animatedDescriptorSetLayout, vulkanContext.device);
}

void createDescriptorPool() {
  PARTICLE_COMPUTE_DESCRIPTOR_POOL(vulkanRendererContext.particleDescriptorPool,
                                   vulkanContext.device);
  DEFAULT_DESCRIPTOR_POOL(vulkanRendererContext.descriptorPool,
                          vulkanContext.device);
  ANIMATED_DESCRIPTOR_POOL(vulkanRendererContext.animatedDescriptorPool,
                           vulkanContext.device);
}

void createParticleDescriptorSets() {
  std::vector<vk::DescriptorSetLayout> layouts(
      MAX_FRAMES_IN_FLIGHT, *vulkanRendererContext.particleDescriptorSetLayout);

  vk::DescriptorSetAllocateInfo allocInfo{
      .descriptorPool = *vulkanRendererContext.particleDescriptorPool,
      .descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT),
      .pSetLayouts = layouts.data(),
  };

  vulkanRendererContext.particleDescriptorSets =
      vk::raii::DescriptorSets(vulkanContext.device, allocInfo);

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    vk::DescriptorBufferInfo cameraBufferInfo{
        .buffer = *vulkanRendererContext.uniformBuffers[i],
        .offset = 0,
        .range = sizeof(CameraBufferObject),
    };

    vk::DescriptorBufferInfo particleBufferInfo{
        .buffer = *vulkanRendererContext.particleBuffer,
        .offset = 0,
        .range = VK_WHOLE_SIZE,
    };

    vk::DescriptorBufferInfo simParamsBufferInfo{
        .buffer = *vulkanRendererContext.particleSimParamsBuffers[i],
        .offset = 0,
        .range = sizeof(ParticleSimUbo),
    };

    vk::DescriptorBufferInfo emitterBufferInfo{
        .buffer = *vulkanRendererContext.particleEmitterBuffers[i],
        .offset = 0,
        .range = VK_WHOLE_SIZE,
    };

    vk::DescriptorBufferInfo spawnCounterBufferInfo{
        .buffer = *vulkanRendererContext.particleSpawnCounterBuffers[i],
        .offset = 0,
        .range = VK_WHOLE_SIZE,
    };

    std::array<vk::WriteDescriptorSet, 5> descriptorWrites{
        vk::WriteDescriptorSet{
            .dstSet = *vulkanRendererContext.particleDescriptorSets[i],
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eUniformBuffer,
            .pBufferInfo = &cameraBufferInfo,
        },
        vk::WriteDescriptorSet{
            .dstSet = *vulkanRendererContext.particleDescriptorSets[i],
            .dstBinding = 1,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eStorageBuffer,
            .pBufferInfo = &particleBufferInfo,
        },
        vk::WriteDescriptorSet{
            .dstSet = *vulkanRendererContext.particleDescriptorSets[i],
            .dstBinding = 2,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eUniformBuffer,
            .pBufferInfo = &simParamsBufferInfo,
        },
        vk::WriteDescriptorSet{
            .dstSet = *vulkanRendererContext.particleDescriptorSets[i],
            .dstBinding = 3,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eStorageBuffer,
            .pBufferInfo = &emitterBufferInfo,
        },
        vk::WriteDescriptorSet{
            .dstSet = *vulkanRendererContext.particleDescriptorSets[i],
            .dstBinding = 4,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eStorageBuffer,
            .pBufferInfo = &spawnCounterBufferInfo,
        },
    };

    vulkanContext.device.updateDescriptorSets(descriptorWrites, nullptr);
  }
}

void createStaticDescriptorSets() {
  std::vector<vk::DescriptorSetLayout> layouts(
      MAX_FRAMES_IN_FLIGHT, *vulkanRendererContext.descriptorSetLayout);

  vk::DescriptorSetAllocateInfo allocInfo{
      .descriptorPool = *vulkanRendererContext.descriptorPool,
      .descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT),
      .pSetLayouts = layouts.data()};

  vulkanRendererContext.descriptorSets =
      vk::raii::DescriptorSets(vulkanContext.device, allocInfo);

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    vk::DescriptorBufferInfo cameraBufferInfo{
        .buffer = *vulkanRendererContext.uniformBuffers[i],
        .offset = 0,
        .range = sizeof(CameraBufferObject)};

    vk::WriteDescriptorSet descriptorWrite{
        .dstSet = *vulkanRendererContext.descriptorSets[i],
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = vk::DescriptorType::eUniformBuffer,
        .pBufferInfo = &cameraBufferInfo};

    vulkanContext.device.updateDescriptorSets(descriptorWrite, nullptr);
  }
}

void createAnimatedDescriptorSets() {
  std::vector<vk::DescriptorSetLayout> layouts(
      MAX_FRAMES_IN_FLIGHT, *vulkanRendererContext.animatedDescriptorSetLayout);

  vk::DescriptorSetAllocateInfo allocInfo{
      .descriptorPool = *vulkanRendererContext.animatedDescriptorPool,
      .descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT),
      .pSetLayouts = layouts.data()};

  vulkanRendererContext.animatedDescriptorSets =
      vk::raii::DescriptorSets(vulkanContext.device, allocInfo);

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    vk::DescriptorBufferInfo cameraBufferInfo{
        .buffer = *vulkanRendererContext.uniformBuffers[i],
        .offset = 0,
        .range = sizeof(CameraBufferObject)};

    vk::DescriptorBufferInfo animationBufferInfo{
        .buffer = *vulkanRendererContext.animationPositionsBuffer,
        .offset = 0,
        .range = VK_WHOLE_SIZE};

    std::array<vk::WriteDescriptorSet, 2> descriptorWrites{
        vk::WriteDescriptorSet{
            .dstSet = *vulkanRendererContext.animatedDescriptorSets[i],
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eUniformBuffer,
            .pBufferInfo = &cameraBufferInfo},
        vk::WriteDescriptorSet{
            .dstSet = *vulkanRendererContext.animatedDescriptorSets[i],
            .dstBinding = 1,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eStorageBuffer,
            .pBufferInfo = &animationBufferInfo}};

    vulkanContext.device.updateDescriptorSets(descriptorWrites, nullptr);
  }
}

void createGraphicsPipeline() {
  PARTICLE_COMPUTE_GRAPHICS_PIPELINE();
  PARTICLE_GRAPHICS_PIPELINE();
  DEFAULT_GRAPHICS_PIPELINE();
  ANIMATED_GRAPHICS_PIPELINE();
  DEBUG_GRAPHICS_PIPELINE();
}

void createDebugBuffers() {
  constexpr uint32_t maxDebugVertices = 65536;
  vk::DeviceSize bufferSize = sizeof(DebugVertex) * maxDebugVertices;

  vulkanRendererContext.debugFrames.clear();
  vulkanRendererContext.debugFrames.reserve(MAX_FRAMES_IN_FLIGHT);

  for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    BufferWithMemory bufferWithMemory =
        createBuffer(bufferSize, vk::BufferUsageFlagBits::eVertexBuffer,
                     vk::MemoryPropertyFlagBits::eHostVisible |
                         vk::MemoryPropertyFlagBits::eHostCoherent);

    DebugFrameData frame{};
    frame.vertexBuffer = std::move(bufferWithMemory.buffer);
    frame.vertexMemory = std::move(bufferWithMemory.memory);
    frame.mapped = frame.vertexMemory.mapMemory(0, bufferSize);
    frame.vertexCount = 0;

    vulkanRendererContext.debugFrames.emplace_back(std::move(frame));
  }
}

void createUniformBuffers() {
  vk::DeviceSize bufferSize = sizeof(CameraBufferObject);

  vulkanRendererContext.uniformBuffers.clear();
  vulkanRendererContext.uniformBuffersMemory.clear();
  vulkanRendererContext.uniformBuffersMapped.clear();

  vulkanRendererContext.uniformBuffers.reserve(MAX_FRAMES_IN_FLIGHT);
  vulkanRendererContext.uniformBuffersMemory.reserve(MAX_FRAMES_IN_FLIGHT);
  vulkanRendererContext.uniformBuffersMapped.reserve(MAX_FRAMES_IN_FLIGHT);

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    BufferWithMemory bufferWithMemory =
        createBuffer(bufferSize, vk::BufferUsageFlagBits::eUniformBuffer,
                     vk::MemoryPropertyFlagBits::eHostVisible |
                         vk::MemoryPropertyFlagBits::eHostCoherent);

    vulkanRendererContext.uniformBuffers.emplace_back(
        std::move(bufferWithMemory.buffer));
    vulkanRendererContext.uniformBuffersMemory.emplace_back(
        std::move(bufferWithMemory.memory));

    vulkanRendererContext.uniformBuffersMapped.push_back(
        vulkanRendererContext.uniformBuffersMemory.back().mapMemory(
            0, bufferSize));
  }
}

void createParticleSimParamsBuffers() {
  vk::DeviceSize bufferSize = sizeof(ParticleSimUbo);

  vulkanRendererContext.particleSimParamsBuffers.clear();
  vulkanRendererContext.particleSimParamsMemory.clear();
  vulkanRendererContext.particleSimParamsMapped.clear();

  vulkanRendererContext.particleSimParamsBuffers.reserve(MAX_FRAMES_IN_FLIGHT);
  vulkanRendererContext.particleSimParamsMemory.reserve(MAX_FRAMES_IN_FLIGHT);
  vulkanRendererContext.particleSimParamsMapped.reserve(MAX_FRAMES_IN_FLIGHT);

  for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    BufferWithMemory bufferWithMemory =
        createBuffer(bufferSize, vk::BufferUsageFlagBits::eUniformBuffer,
                     vk::MemoryPropertyFlagBits::eHostVisible |
                         vk::MemoryPropertyFlagBits::eHostCoherent);

    vulkanRendererContext.particleSimParamsBuffers.emplace_back(
        std::move(bufferWithMemory.buffer));
    vulkanRendererContext.particleSimParamsMemory.emplace_back(
        std::move(bufferWithMemory.memory));

    vulkanRendererContext.particleSimParamsMapped.push_back(
        vulkanRendererContext.particleSimParamsMemory.back().mapMemory(
            0, bufferSize));
  }
}

void createParticleEmitterBuffers() {
  vk::DeviceSize bufferSize =
      sizeof(ParticleEmitterGpu) * MAX_PARTICLE_EMITTERS;

  vulkanRendererContext.particleEmitterBuffers.clear();
  vulkanRendererContext.particleEmitterMemory.clear();
  vulkanRendererContext.particleEmitterMapped.clear();

  vulkanRendererContext.particleEmitterBuffers.reserve(MAX_FRAMES_IN_FLIGHT);
  vulkanRendererContext.particleEmitterMemory.reserve(MAX_FRAMES_IN_FLIGHT);
  vulkanRendererContext.particleEmitterMapped.reserve(MAX_FRAMES_IN_FLIGHT);

  for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    BufferWithMemory bufferWithMemory =
        createBuffer(bufferSize, vk::BufferUsageFlagBits::eStorageBuffer,
                     vk::MemoryPropertyFlagBits::eHostVisible |
                         vk::MemoryPropertyFlagBits::eHostCoherent);

    vulkanRendererContext.particleEmitterBuffers.emplace_back(
        std::move(bufferWithMemory.buffer));
    vulkanRendererContext.particleEmitterMemory.emplace_back(
        std::move(bufferWithMemory.memory));
    vulkanRendererContext.particleEmitterMapped.push_back(
        vulkanRendererContext.particleEmitterMemory.back().mapMemory(
            0, bufferSize));
  }
}

void createParticleSpawnCounterBuffers() {
  vk::DeviceSize bufferSize = sizeof(uint32_t) * MAX_PARTICLE_EMITTERS;

  vulkanRendererContext.particleSpawnCounterBuffers.clear();
  vulkanRendererContext.particleSpawnCounterMemory.clear();
  vulkanRendererContext.particleSpawnCounterMapped.clear();

  vulkanRendererContext.particleSpawnCounterBuffers.reserve(
      MAX_FRAMES_IN_FLIGHT);
  vulkanRendererContext.particleSpawnCounterMemory.reserve(
      MAX_FRAMES_IN_FLIGHT);
  vulkanRendererContext.particleSpawnCounterMapped.reserve(
      MAX_FRAMES_IN_FLIGHT);

  for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    BufferWithMemory bufferWithMemory =
        createBuffer(bufferSize, vk::BufferUsageFlagBits::eStorageBuffer,
                     vk::MemoryPropertyFlagBits::eHostVisible |
                         vk::MemoryPropertyFlagBits::eHostCoherent);

    vulkanRendererContext.particleSpawnCounterBuffers.emplace_back(
        std::move(bufferWithMemory.buffer));
    vulkanRendererContext.particleSpawnCounterMemory.emplace_back(
        std::move(bufferWithMemory.memory));
    vulkanRendererContext.particleSpawnCounterMapped.push_back(
        vulkanRendererContext.particleSpawnCounterMemory.back().mapMemory(
            0, bufferSize));
  }
}

void createCommandBuffers() {
  vulkanRendererContext.commandBuffers.clear();

  vk::CommandBufferAllocateInfo allocInfo{
      .commandPool = *vulkanContext.commandPool,
      .level = vk::CommandBufferLevel::ePrimary,
      .commandBufferCount = MAX_FRAMES_IN_FLIGHT};

  vulkanRendererContext.commandBuffers =
      vk::raii::CommandBuffers{vulkanContext.device, allocInfo};
}

void createSyncObjects() {
  vulkanRendererContext.imageAvailableSemaphores.clear();
  vulkanRendererContext.renderFinishedSemaphores.clear();
  vulkanRendererContext.inFlightFences.clear();

  vulkanRendererContext.imageAvailableSemaphores.reserve(MAX_FRAMES_IN_FLIGHT);
  vulkanRendererContext.renderFinishedSemaphores.reserve(
      vulkanContext.swapchainImages.size());
  vulkanRendererContext.inFlightFences.reserve(MAX_FRAMES_IN_FLIGHT);

  vk::SemaphoreCreateInfo semaphoreInfo{};
  vk::FenceCreateInfo fenceInfo{.flags = vk::FenceCreateFlagBits::eSignaled};

  for (size_t i = 0; i < vulkanContext.swapchainImages.size(); i++) {
    vulkanRendererContext.renderFinishedSemaphores.emplace_back(
        vulkanContext.device, semaphoreInfo);
  }

  for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    vulkanRendererContext.imageAvailableSemaphores.emplace_back(
        vulkanContext.device, semaphoreInfo);
    vulkanRendererContext.inFlightFences.emplace_back(vulkanContext.device,
                                                      fenceInfo);
  }
}

void createDepthResources() {

  vk::ImageCreateInfo imageInfo{
      .imageType = vk::ImageType::e2D,
      .format = vulkanRendererContext.depthFormat,
      .extent = vk::Extent3D{.width = vulkanContext.swapchainExtent.width,
                             .height = vulkanContext.swapchainExtent.height,
                             .depth = 1},
      .mipLevels = 1,
      .arrayLayers = 1,
      .samples = vk::SampleCountFlagBits::e1,
      .tiling = vk::ImageTiling::eOptimal,
      .usage = vk::ImageUsageFlagBits::eDepthStencilAttachment,
      .sharingMode = vk::SharingMode::eExclusive,
      .initialLayout = vk::ImageLayout::eUndefined};

  vulkanRendererContext.depthImage =
      vk::raii::Image{vulkanContext.device, imageInfo};

  vk::MemoryRequirements memRequirements =
      vulkanRendererContext.depthImage.getMemoryRequirements();

  vk::MemoryAllocateInfo allocInfo{
      .allocationSize = memRequirements.size,
      .memoryTypeIndex =
          findMemoryType(memRequirements.memoryTypeBits,
                         vk::MemoryPropertyFlagBits::eDeviceLocal),
  };

  vulkanRendererContext.depthImageMemory =
      vk::raii::DeviceMemory{vulkanContext.device, allocInfo};

  vulkanRendererContext.depthImage.bindMemory(
      *vulkanRendererContext.depthImageMemory, 0);

  vk::ImageViewCreateInfo createInfo{
      .image = *vulkanRendererContext.depthImage,
      .viewType = vk::ImageViewType::e2D,
      .format = vulkanRendererContext.depthFormat,
      .subresourceRange = vk::ImageSubresourceRange{
          .aspectMask = vk::ImageAspectFlagBits::eDepth,
          .baseMipLevel = 0,
          .levelCount = 1,
          .baseArrayLayer = 0,
          .layerCount = 1}};

  vulkanRendererContext.depthImageView =
      vk::raii::ImageView{vulkanContext.device, createInfo};
}

void initializeParticleAllocator() {
  vulkanRendererContext.particleCount = MAX_PARTICLES;

  freeParticleEmitterSlots.clear();
  freeParticleRanges.clear();
  particleEmitterAllocations.clear();

  freeParticleEmitterSlots.reserve(MAX_PARTICLE_EMITTERS);
  for (uint32_t i = MAX_PARTICLE_EMITTERS; i > 0; i--) {
    freeParticleEmitterSlots.push_back(i - 1);
  }

  freeParticleRanges.push_back(
      ParticleRange{.first = 0, .count = MAX_PARTICLES});
}

static ParticleGpu inactiveParticle(uint32_t gpuEmitterIndex,
                                    const ParticleEmitterCpuComponent &emitter,
                                    uint32_t particleIndex) {
  return ParticleGpu{
      .positionLifeTime = glm::vec4{0.0f, 0.0f, 0.0f, 0.0f},
      .velocitySize = glm::vec4{0.0f, 0.0f, 0.0f, emitter.particleStartSize},
      .color = glm::vec4{1.0f, 1.0f, 1.0f, 0.0f},
      .state = glm::vec4{emitter.particleLifetime, 0.0f, 0.0f, 0.0f},
      .meta = glm::uvec4{gpuEmitterIndex, 0u, particleIndex * 9781u, 0u},
  };
}

static void
writeParticleRangeInactive(const ParticleEmitterAllocation &allocation,
                           const ParticleEmitterCpuComponent &emitter) {
  if (vulkanRendererContext.particleBufferMapped == nullptr) {
    return;
  }

  ParticleGpu *particles =
      static_cast<ParticleGpu *>(vulkanRendererContext.particleBufferMapped);

  for (uint32_t offset = 0; offset < allocation.maxParticles; offset++) {
    const uint32_t particleIndex = allocation.firstParticle + offset;
    particles[particleIndex] =
        inactiveParticle(allocation.gpuEmitterIndex, emitter, particleIndex);
  }
}

static void
writeParticleRangeInvalid(const ParticleEmitterAllocation &allocation) {
  if (vulkanRendererContext.particleBufferMapped == nullptr) {
    return;
  }

  ParticleGpu *particles =
      static_cast<ParticleGpu *>(vulkanRendererContext.particleBufferMapped);

  for (uint32_t offset = 0; offset < allocation.maxParticles; offset++) {
    const uint32_t particleIndex = allocation.firstParticle + offset;
    particles[particleIndex] = ParticleGpu{
        .positionLifeTime = glm::vec4{0.0f},
        .velocitySize = glm::vec4{0.0f},
        .color = glm::vec4{0.0f},
        .state = glm::vec4{0.0f},
        .meta = glm::uvec4{INVALID_PARTICLE_EMITTER_SLOT, 0u,
                           particleIndex * 9781u, 0u},
    };
  }
}

static std::optional<ParticleRange> allocateParticleRange(uint32_t count) {
  for (size_t i = 0; i < freeParticleRanges.size(); i++) {
    ParticleRange &range = freeParticleRanges[i];
    if (range.count < count) {
      continue;
    }

    ParticleRange allocated{.first = range.first, .count = count};
    range.first += count;
    range.count -= count;

    if (range.count == 0) {
      freeParticleRanges.erase(freeParticleRanges.begin() + i);
    }

    return allocated;
  }

  return std::nullopt;
}

static void releaseParticleRange(ParticleRange released) {
  if (released.count == 0) {
    return;
  }

  freeParticleRanges.push_back(released);
  std::sort(freeParticleRanges.begin(), freeParticleRanges.end(),
            [](const ParticleRange &a, const ParticleRange &b) {
              return a.first < b.first;
            });

  std::vector<ParticleRange> merged;
  merged.reserve(freeParticleRanges.size());

  for (const ParticleRange &range : freeParticleRanges) {
    if (merged.empty() ||
        merged.back().first + merged.back().count < range.first) {
      merged.push_back(range);
      continue;
    }

    uint32_t mergedEnd = std::max(merged.back().first + merged.back().count,
                                  range.first + range.count);
    merged.back().count = mergedEnd - merged.back().first;
  }

  freeParticleRanges = std::move(merged);
}

static ParticleEmitterAllocation *
ensureParticleEmitterAllocation(ParticleEmitterCpuComponent &emitter) {
  auto existing = particleEmitterAllocations.find(emitter.entity);
  if (existing != particleEmitterAllocations.end()) {
    ParticleEmitterAllocation &allocation = existing->second;
    allocation.seenThisFrame = true;
    emitter.firstParticle = allocation.firstParticle;
    return &allocation;
  }

  if (emitter.maxParticles == 0 || freeParticleEmitterSlots.empty()) {
    emitter.active = false;
    return nullptr;
  }

  std::optional<ParticleRange> range =
      allocateParticleRange(emitter.maxParticles);
  if (!range.has_value()) {
    emitter.active = false;
    return nullptr;
  }

  uint32_t gpuEmitterIndex = freeParticleEmitterSlots.back();
  freeParticleEmitterSlots.pop_back();

  ParticleEmitterAllocation allocation{
      .gpuEmitterIndex = gpuEmitterIndex,
      .firstParticle = range->first,
      .maxParticles = range->count,
      .seenThisFrame = true,
  };

  auto [it, _] = particleEmitterAllocations.emplace(emitter.entity, allocation);
  emitter.firstParticle = allocation.firstParticle;
  writeParticleRangeInactive(it->second, emitter);

  return &it->second;
}

static void releaseMissingParticleEmitterAllocations() {
  for (auto it = particleEmitterAllocations.begin();
       it != particleEmitterAllocations.end();) {
    ParticleEmitterAllocation &allocation = it->second;
    if (allocation.seenThisFrame) {
      ++it;
      continue;
    }

    if (allocation.gpuEmitterIndex != INVALID_PARTICLE_EMITTER_SLOT) {
      freeParticleEmitterSlots.push_back(allocation.gpuEmitterIndex);
    }

    writeParticleRangeInvalid(allocation);

    releaseParticleRange(ParticleRange{.first = allocation.firstParticle,
                                       .count = allocation.maxParticles});

    it = particleEmitterAllocations.erase(it);
  }
}

void createParticleBuffer() {
  vk::DeviceSize bufferSize = sizeof(ParticleGpu) * MAX_PARTICLES;

  BufferWithMemory buffer =
      createBuffer(bufferSize, vk::BufferUsageFlagBits::eStorageBuffer,
                   vk::MemoryPropertyFlagBits::eHostVisible |
                       vk::MemoryPropertyFlagBits::eHostCoherent);

  vulkanRendererContext.particleBuffer = std::move(buffer.buffer);
  vulkanRendererContext.particleBufferMemory = std::move(buffer.memory);
  vulkanRendererContext.particleBufferMapped =
      vulkanRendererContext.particleBufferMemory.mapMemory(0, bufferSize);

  std::vector<ParticleGpu> particles(MAX_PARTICLES);
  for (uint32_t i = 0; i < MAX_PARTICLES; i++) {
    particles[i] = ParticleGpu{
        .positionLifeTime = glm::vec4{0.0f},
        .velocitySize = glm::vec4{0.0f},
        .color = glm::vec4{0.0f},
        .state = glm::vec4{0.0f},
        .meta = glm::uvec4{INVALID_PARTICLE_EMITTER_SLOT, 0u, i * 9781u, 0u},
    };
  }

  memcpy(vulkanRendererContext.particleBufferMapped, particles.data(),
         static_cast<size_t>(bufferSize));
}

void setupRendererCore() {
  createCommandBuffers();
  createDepthResources();
  createUniformBuffers();
  // At some point this should be something we set depending of the emitter
  // config.
  BlenderModel wizardProjectile = loadModel("assets/Wizard_Projectile.3d");
  vulkanRendererContext.particleQuadMesh =
      generateMesh(wizardProjectile.vertices, wizardProjectile.indices);
  initializeParticleAllocator();
  createParticleSimParamsBuffers();
  createParticleEmitterBuffers();
  createParticleSpawnCounterBuffers();
  createDebugBuffers();
  createParticleBuffer();
  createSyncObjects();
}

void setupRendererAfterAssetsLoaded() {
  createDescriptorSetLayout();
  createGraphicsPipeline();
  createDescriptorPool();
  createStaticDescriptorSets();
  if (vulkanRendererContext.animationPositionCount > 0) {
    createAnimatedDescriptorSets();
  }
  createParticleDescriptorSets();
}

void transitionImageLayout(vk::raii::CommandBuffer const &commandBuffer,
                           vk::Image image, vk::ImageLayout oldLayout,
                           vk::ImageLayout newLayout,
                           vk::PipelineStageFlags srcStage,
                           vk::AccessFlags srcAccess,
                           vk::PipelineStageFlags dstStage,
                           vk::AccessFlags dstAccess,
                           vk::ImageAspectFlags aspectMask) {
  vk::ImageMemoryBarrier barrier{
      .srcAccessMask = srcAccess,
      .dstAccessMask = dstAccess,
      .oldLayout = oldLayout,
      .newLayout = newLayout,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = image,
      .subresourceRange = vk::ImageSubresourceRange{.aspectMask = aspectMask,
                                                    .baseMipLevel = 0,
                                                    .levelCount = 1,
                                                    .baseArrayLayer = 0,
                                                    .layerCount = 1}};

  commandBuffer.pipelineBarrier(srcStage, dstStage, {}, nullptr, nullptr,
                                barrier);
}

void recordCommandBuffer(uint32_t frameIndex, uint32_t imageIndex) {
  auto &commandBuffer = vulkanRendererContext.commandBuffers[frameIndex];

  commandBuffer.begin(vk::CommandBufferBeginInfo{});

  if (vulkanRendererContext.particleCount > 0) {
    commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute,
                               *vulkanRendererContext.particleComputePipeline);

    commandBuffer.bindDescriptorSets(
        vk::PipelineBindPoint::eCompute,
        *vulkanRendererContext.particleComputePipelineLayout, 0,
        *vulkanRendererContext.particleDescriptorSets[frameIndex], nullptr);

    uint32_t groupCount =
        (vulkanRendererContext.particleCount + WORKGROUP_SIZE - 1) /
        WORKGROUP_SIZE;

    commandBuffer.dispatch(groupCount, 1, 1);

    // Wait for the compute pipeline to finish.
    vk::BufferMemoryBarrier particleBarrier{
        .srcAccessMask = vk::AccessFlagBits::eShaderWrite,
        .dstAccessMask = vk::AccessFlagBits::eShaderRead,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .buffer = *vulkanRendererContext.particleBuffer,
        .offset = 0,
        .size = VK_WHOLE_SIZE,
    };

    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader,
                                  vk::PipelineStageFlagBits::eVertexShader, {},
                                  nullptr, particleBarrier, nullptr);
  }

  transitionImageLayout(
      commandBuffer, vulkanContext.swapchainImages[imageIndex],
      vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal,
      vk::PipelineStageFlagBits::eTopOfPipe, {},
      vk::PipelineStageFlagBits::eColorAttachmentOutput,
      vk::AccessFlagBits::eColorAttachmentWrite,
      vk::ImageAspectFlagBits::eColor);

  vk::ClearValue clearColor{
      .color = vk::ClearColorValue{
          .float32 = std::array<float, 4>{0.02f, 0.02f, 0.04f, 1.0f}}};

  vk::RenderingAttachmentInfo colorAttachment{
      .imageView = *vulkanContext.swapchainImageViews[imageIndex],
      .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
      .loadOp = vk::AttachmentLoadOp::eClear,
      .storeOp = vk::AttachmentStoreOp::eStore,
      .clearValue = clearColor};

  vk::ClearValue clearDepth{
      .depthStencil =
          vk::ClearDepthStencilValue{
              .depth = 1.0f,
              .stencil = 0,
          },
  };

  vk::RenderingAttachmentInfo depthAttachment{
      .imageView = *vulkanRendererContext.depthImageView,
      .imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
      .loadOp = vk::AttachmentLoadOp::eClear,
      .storeOp = vk::AttachmentStoreOp::eDontCare,
      .clearValue = clearDepth,
  };

  vk::RenderingInfo renderingInfo{
      .renderArea = vk::Rect2D{.offset = vk::Offset2D{0, 0},
                               .extent = vulkanContext.swapchainExtent},
      .layerCount = 1,
      .colorAttachmentCount = 1,
      .pColorAttachments = &colorAttachment,
      .pDepthAttachment = &depthAttachment};

  transitionImageLayout(commandBuffer, *vulkanRendererContext.depthImage,
                        vk::ImageLayout::eUndefined,
                        vk::ImageLayout::eDepthAttachmentOptimal,
                        vk::PipelineStageFlagBits::eTopOfPipe, {},
                        vk::PipelineStageFlagBits::eEarlyFragmentTests,
                        vk::AccessFlagBits::eDepthStencilAttachmentWrite,
                        vk::ImageAspectFlagBits::eDepth);

  commandBuffer.beginRendering(renderingInfo);

  vk::Viewport viewport{
      .x = 0.0f,
      .y = 0.0f,
      .width = static_cast<float>(vulkanContext.swapchainExtent.width),
      .height = static_cast<float>(vulkanContext.swapchainExtent.height),
      .minDepth = 0.0f,
      .maxDepth = 1.0f};

  vk::Rect2D scissor{.offset = vk::Offset2D{0, 0},
                     .extent = vulkanContext.swapchainExtent};

  commandBuffer.setViewport(0, viewport);
  commandBuffer.setScissor(0, scissor);

  for (const Renderable &renderable : resources.renderables) {
    if (!isEntityAlive(renderable.entity) || !renderable.visible)
      continue;

    const TransformComponent &transform = getTransform(renderable.entity);

    if (renderable.renderKind == ObjectRenderKind::Animated) {
      commandBuffer.bindPipeline(
          vk::PipelineBindPoint::eGraphics,
          *vulkanRendererContext.animatedGraphicsPipeline);

      commandBuffer.bindDescriptorSets(
          vk::PipelineBindPoint::eGraphics,
          *vulkanRendererContext.animatedPipelineLayout, 0,
          *vulkanRendererContext.animatedDescriptorSets[frameIndex], nullptr);

      AnimationDataFromObject animationData =
          getAnimationDataFromEntity(renderable.entity);

      AnimatedObjectPushConstants pushConstants{
          .model = transform.model,
          .previousPositionOffset = animationData.previousPositionOffset,
          .nextPositionOffset = animationData.nextPositionOffset,
          .interpolation = animationData.interpolation,
          .vertexCount = renderable.animatedMesh->mesh.vertexCount,
      };

      commandBuffer.pushConstants<AnimatedObjectPushConstants>(
          *vulkanRendererContext.animatedPipelineLayout,
          vk::ShaderStageFlagBits::eVertex, 0, pushConstants);

      vk::Buffer vertexBuffers[] = {
          *renderable.animatedMesh->mesh.vertexBuffer};
      vk::DeviceSize offsets[] = {0};

      commandBuffer.bindVertexBuffers(0, vertexBuffers, offsets);
      commandBuffer.bindIndexBuffer(*renderable.animatedMesh->mesh.indexBuffer,
                                    0, vk::IndexType::eUint16);

      commandBuffer.drawIndexed(renderable.animatedMesh->mesh.indexCount, 1, 0,
                                0, 0);
    } else if (renderable.renderKind == ObjectRenderKind::TransformAnimated) {
      commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                 *vulkanRendererContext.graphicsPipeline);

      commandBuffer.bindDescriptorSets(
          vk::PipelineBindPoint::eGraphics,
          *vulkanRendererContext.pipelineLayout, 0,
          *vulkanRendererContext.descriptorSets[frameIndex], nullptr);

      ObjectPushConstants pushConstants{
          .model = transform.model,
      };

      commandBuffer.pushConstants<ObjectPushConstants>(
          *vulkanRendererContext.pipelineLayout,
          vk::ShaderStageFlagBits::eVertex, 0, pushConstants);

      vk::Buffer vertexBuffers[] = {
          *renderable.transformAnimatedMesh->mesh.vertexBuffer,
      };
      vk::DeviceSize offsets[] = {0};

      commandBuffer.bindVertexBuffers(0, vertexBuffers, offsets);
      commandBuffer.bindIndexBuffer(
          *renderable.transformAnimatedMesh->mesh.indexBuffer, 0,
          vk::IndexType::eUint16);

      commandBuffer.drawIndexed(
          renderable.transformAnimatedMesh->mesh.indexCount, 1, 0, 0, 0);
    } else {
      commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                 *vulkanRendererContext.graphicsPipeline);

      commandBuffer.bindDescriptorSets(
          vk::PipelineBindPoint::eGraphics,
          *vulkanRendererContext.pipelineLayout, 0,
          *vulkanRendererContext.descriptorSets[frameIndex], nullptr);

      ObjectPushConstants pushConstants{.model = transform.model};

      commandBuffer.pushConstants<ObjectPushConstants>(
          *vulkanRendererContext.pipelineLayout,
          vk::ShaderStageFlagBits::eVertex, 0, pushConstants);

      vk::Buffer vertexBuffers[] = {*renderable.mesh->vertexBuffer};
      vk::DeviceSize offsets[] = {0};

      commandBuffer.bindVertexBuffers(0, vertexBuffers, offsets);
      commandBuffer.bindIndexBuffer(*renderable.mesh->indexBuffer, 0,
                                    vk::IndexType::eUint16);

      commandBuffer.drawIndexed(renderable.mesh->indexCount, 1, 0, 0, 0);
    }
  }

  if (vulkanRendererContext.particleCount > 0) {
    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                               *vulkanRendererContext.particleGraphicsPipeline);

    commandBuffer.bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics,
        *vulkanRendererContext.particleGraphicsPipelineLayout, 0,
        *vulkanRendererContext.particleDescriptorSets[frameIndex], nullptr);

    vk::Buffer vertexBuffers[] = {
        *vulkanRendererContext.particleQuadMesh.vertexBuffer,
    };
    vk::DeviceSize offsets[] = {0};

    commandBuffer.bindVertexBuffers(0, vertexBuffers, offsets);
    commandBuffer.bindIndexBuffer(
        *vulkanRendererContext.particleQuadMesh.indexBuffer, 0,
        vk::IndexType::eUint16);

    commandBuffer.drawIndexed(vulkanRendererContext.particleQuadMesh.indexCount,
                              vulkanRendererContext.particleCount, 0, 0, 0);
  }

  if (vulkanRendererContext.isDebug) {
    const DebugFrameData &debugFrame =
        vulkanRendererContext.debugFrames[frameIndex];

    if (debugFrame.vertexCount > 0) {
      commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                 *vulkanRendererContext.debugGraphicsPipeline);

      CameraBufferObject camera{};
      camera.view =
          glm::lookAt(sceneContext.cameraPosition,
                      sceneContext.cameraPosition + sceneContext.cameraLookAt,
                      glm::vec3{0.0f, 0.0f, 1.0f});

      camera.proj = glm::perspective(
          sceneContext.cameraFovY,
          static_cast<float>(vulkanContext.swapchainExtent.width) /
              static_cast<float>(vulkanContext.swapchainExtent.height),
          sceneContext.cameraClipStart, sceneContext.cameraClipEnd);

      camera.proj[1][1] *= -1.0f;

      glm::mat4 viewProj = camera.proj * camera.view;

      commandBuffer.pushConstants<glm::mat4>(
          *vulkanRendererContext.debugPipelineLayout,
          vk::ShaderStageFlagBits::eVertex, 0, viewProj);

      vk::Buffer vertexBuffers[] = {*debugFrame.vertexBuffer};
      vk::DeviceSize offsets[] = {0};

      commandBuffer.bindVertexBuffers(0, vertexBuffers, offsets);
      commandBuffer.draw(debugFrame.vertexCount, 1, 0, 0);
    }
  }

  commandBuffer.endRendering();

  transitionImageLayout(
      commandBuffer, vulkanContext.swapchainImages[imageIndex],
      vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR,
      vk::PipelineStageFlagBits::eColorAttachmentOutput,
      vk::AccessFlagBits::eColorAttachmentWrite,
      vk::PipelineStageFlagBits::eBottomOfPipe, {},
      vk::ImageAspectFlagBits::eColor);

  commandBuffer.end();
}

// Need to update this
void updateUniformBuffer(uint32_t currentImage) {
  CameraBufferObject camera{};

  camera.view =
      glm::lookAt(sceneContext.cameraPosition,
                  sceneContext.cameraPosition + sceneContext.cameraLookAt,
                  glm::vec3{0.0f, 0.0f, 1.0f});

  camera.proj = glm::perspective(
      sceneContext.cameraFovY,
      static_cast<float>(vulkanContext.swapchainExtent.width) /
          static_cast<float>(vulkanContext.swapchainExtent.height),
      sceneContext.cameraClipStart, sceneContext.cameraClipEnd);

  camera.proj[1][1] *= -1.0f;

  memcpy(vulkanRendererContext.uniformBuffersMapped[currentImage], &camera,
         sizeof(camera));
}

void updateParticleSimParams(uint32_t frameIndex) {
  ParticleSimUbo sim{
      .deltaTime = timeState.deltaTime,
      .particleCount = vulkanRendererContext.particleCount,
      .emitterCount = MAX_PARTICLE_EMITTERS,
      .frameIndex = vulkanRendererContext.particleSimulationFrame,
  };

  memcpy(vulkanRendererContext.particleSimParamsMapped[frameIndex], &sim,
         sizeof(sim));
}

void updateParticleEmitters(uint32_t frameIndex) {
  for (auto &[_, allocation] : particleEmitterAllocations) {
    allocation.seenThisFrame = false;
  }

  std::vector<ParticleEmitterGpu> gpuEmitters(MAX_PARTICLE_EMITTERS);
  std::vector<uint32_t> spawnCounters(MAX_PARTICLE_EMITTERS, 0);

  for (uint32_t i = 0; i < resources.particleEmitterCpuComponents.size(); i++) {
    ParticleEmitterCpuComponent &emitter =
        resources.particleEmitterCpuComponents[i];

    if (!isEntityAlive(emitter.entity)) {
      continue;
    }

    ParticleEmitterAllocation *allocation =
        ensureParticleEmitterAllocation(emitter);
    if (allocation == nullptr) {
      continue;
    }

    uint32_t particlesToSpawn = 0;
    bool active = emitter.active;
    if (active) {
      emitter.spawnAccumulator += emitter.spawnRate * timeState.deltaTime;
      particlesToSpawn =
          static_cast<uint32_t>(std::floor(emitter.spawnAccumulator));
      emitter.spawnAccumulator -= static_cast<float>(particlesToSpawn);
      particlesToSpawn = std::min(particlesToSpawn, emitter.maxParticles);
    } else {
      emitter.spawnAccumulator = 0.0f;
    }

    gpuEmitters[allocation->gpuEmitterIndex] = ParticleEmitterGpu{
        .position = glm::vec4{emitter.position, 1.0f},
        .direction = glm::vec4{emitter.direction, 0.0f},
        .worldVelocitySpawnRate =
            glm::vec4{0.0f, 0.0f, 0.0f, emitter.spawnRate},
        .config = glm::vec4{emitter.particleLifetime, emitter.particleStartSize,
                            emitter.spawnSpeed, emitter.maxColorSpeed},
        .sizeConfig = glm::vec4{emitter.particleEndSize, 0.0f, 0.0f, 0.0f},
        .lifeColorStart = emitter.lifeColorStart,
        .lifeColorEnd = emitter.lifeColorEnd,
        .speedColorSlow = emitter.speedColorSlow,
        .speedColorFast = emitter.speedColorFast,
        .rangeActive =
            glm::uvec4{allocation->firstParticle, allocation->maxParticles,
                       active ? 1u : 0u, particlesToSpawn},
        .shape = glm::uvec4{static_cast<uint32_t>(emitter.shape), 0u, 0u, 0u},
    };
  }

  releaseMissingParticleEmitterAllocations();

  memcpy(vulkanRendererContext.particleEmitterMapped[frameIndex],
         gpuEmitters.data(), sizeof(ParticleEmitterGpu) * gpuEmitters.size());

  memcpy(vulkanRendererContext.particleSpawnCounterMapped[frameIndex],
         spawnCounters.data(), sizeof(uint32_t) * spawnCounters.size());
}

void clearDebugShapes() { vulkanRendererContext.debugVertices.clear(); }

void addDebugLine(glm::vec3 a, glm::vec3 b, glm::vec4 color) {
  vulkanRendererContext.debugVertices.push_back(DebugVertex{
      .position = a,
      .color = color,
  });

  vulkanRendererContext.debugVertices.push_back(DebugVertex{
      .position = b,
      .color = color,
  });
}

void addDebugDiskXY(glm::vec3 center, float radius, glm::vec4 color) {
  constexpr int segments = 64;

  for (int i = 0; i < segments; i++) {
    float a0 = glm::two_pi<float>() * static_cast<float>(i) /
               static_cast<float>(segments);
    float a1 = glm::two_pi<float>() * static_cast<float>(i + 1) /
               static_cast<float>(segments);

    glm::vec3 p0{
        center.x + glm::cos(a0) * radius,
        center.y + glm::sin(a0) * radius,
        center.z,
    };

    glm::vec3 p1{
        center.x + glm::cos(a1) * radius,
        center.y + glm::sin(a1) * radius,
        center.z,
    };

    addDebugLine(p0, p1, color);
  }
}

void addDebugCube(glm::vec3 center, float radius, glm::vec4 color) {
  glm::vec3 min = center - glm::vec3{radius};
  glm::vec3 max = center + glm::vec3{radius};

  glm::vec3 p000{min.x, min.y, min.z};
  glm::vec3 p001{min.x, min.y, max.z};
  glm::vec3 p010{min.x, max.y, min.z};
  glm::vec3 p011{min.x, max.y, max.z};
  glm::vec3 p100{max.x, min.y, min.z};
  glm::vec3 p101{max.x, min.y, max.z};
  glm::vec3 p110{max.x, max.y, min.z};
  glm::vec3 p111{max.x, max.y, max.z};

  addDebugLine(p000, p100, color);
  addDebugLine(p100, p110, color);
  addDebugLine(p110, p010, color);
  addDebugLine(p010, p000, color);

  addDebugLine(p001, p101, color);
  addDebugLine(p101, p111, color);
  addDebugLine(p111, p011, color);
  addDebugLine(p011, p001, color);

  addDebugLine(p000, p001, color);
  addDebugLine(p100, p101, color);
  addDebugLine(p110, p111, color);
  addDebugLine(p010, p011, color);
}

void addDebugSphere(glm::vec3 center, float radius, glm::vec4 color) {
  constexpr int segments = 32;

  for (int i = 0; i < segments; i++) {
    float a0 = glm::two_pi<float>() * static_cast<float>(i) /
               static_cast<float>(segments);
    float a1 = glm::two_pi<float>() * static_cast<float>(i + 1) /
               static_cast<float>(segments);

    float c0 = glm::cos(a0);
    float s0 = glm::sin(a0);
    float c1 = glm::cos(a1);
    float s1 = glm::sin(a1);

    // XY circle
    addDebugLine(center + glm::vec3{c0 * radius, s0 * radius, 0.0f},
                 center + glm::vec3{c1 * radius, s1 * radius, 0.0f}, color);

    // XZ circle
    addDebugLine(center + glm::vec3{c0 * radius, 0.0f, s0 * radius},
                 center + glm::vec3{c1 * radius, 0.0f, s1 * radius}, color);

    // YZ circle
    addDebugLine(center + glm::vec3{0.0f, c0 * radius, s0 * radius},
                 center + glm::vec3{0.0f, c1 * radius, s1 * radius}, color);
  }
}

void addDebugCellXY(glm::vec3 origin, float width, float height,
                    glm::vec4 color) {
  float x0 = origin.x;
  float y0 = origin.y;
  float x1 = x0 + width;
  float y1 = y0 + height;
  float z = origin.z;

  addDebugLine({x0, y0, z}, {x1, y0, z}, color);
  addDebugLine({x1, y0, z}, {x1, y1, z}, color);
  addDebugLine({x1, y1, z}, {x0, y1, z}, color);
  addDebugLine({x0, y1, z}, {x0, y0, z}, color);

  addDebugLine({x0, y0, z}, {x1, y1, z}, color);
  addDebugLine({x1, y0, z}, {x0, y1, z}, color);
}

void addDebugGridCellsXY(glm::vec3 origin, uint32_t width, uint32_t height,
                         float cellWidth, float cellHeight, glm::vec4 color) {
  for (uint32_t y = 0; y < height; y++) {
    for (uint32_t x = 0; x < width; x++) {
      float x0 = origin.x + static_cast<float>(x) * cellWidth;
      float y0 = origin.y + static_cast<float>(y) * cellHeight;
      float x1 = x0 + cellWidth;
      float y1 = y0 + cellHeight;
      float z = origin.z;

      addDebugLine({x0, y0, z}, {x1, y0, z}, color);
      addDebugLine({x1, y0, z}, {x1, y1, z}, color);
      addDebugLine({x1, y1, z}, {x0, y1, z}, color);
      addDebugLine({x0, y1, z}, {x0, y0, z}, color);
    }
  }
}

void drawFrame() {
  vulkanContext.device.waitForFences(
      *vulkanRendererContext.inFlightFences[vulkanRendererContext.currentFrame],
      vk::True, UINT64_MAX);

  uint32_t imageIndex =
      vulkanContext.swapchain
          .acquireNextImage(UINT64_MAX,
                            *vulkanRendererContext.imageAvailableSemaphores
                                 [vulkanRendererContext.currentFrame],
                            nullptr)
          .value;

  updateUniformBuffer(vulkanRendererContext.currentFrame);
  updateParticleSimParams(vulkanRendererContext.currentFrame);
  updateParticleEmitters(vulkanRendererContext.currentFrame);

  if (vulkanRendererContext.isDebug) {
    DebugFrameData &debugFrame =
        vulkanRendererContext.debugFrames[vulkanRendererContext.currentFrame];

    debugFrame.vertexCount =
        static_cast<uint32_t>(vulkanRendererContext.debugVertices.size());

    if (debugFrame.vertexCount > 0) {
      memcpy(debugFrame.mapped, vulkanRendererContext.debugVertices.data(),
             sizeof(DebugVertex) * debugFrame.vertexCount);
    }
  } else {
    vulkanRendererContext.debugFrames[vulkanRendererContext.currentFrame]
        .vertexCount = 0;
  }

  vulkanContext.device.resetFences(
      *vulkanRendererContext
           .inFlightFences[vulkanRendererContext.currentFrame]);

  vulkanRendererContext.commandBuffers[vulkanRendererContext.currentFrame]
      .reset();
  recordCommandBuffer(vulkanRendererContext.currentFrame, imageIndex);

  vk::Semaphore waitSemaphores[] = {
      *vulkanRendererContext
           .imageAvailableSemaphores[vulkanRendererContext.currentFrame]};

  vk::PipelineStageFlags waitStages[] = {
      vk::PipelineStageFlagBits::eColorAttachmentOutput};

  vk::Semaphore signalSemaphores[] = {
      *vulkanRendererContext.renderFinishedSemaphores[imageIndex]};

  vk::CommandBuffer commandBuffer =
      *vulkanRendererContext.commandBuffers[vulkanRendererContext.currentFrame];

  vk::SubmitInfo submitInfo{.waitSemaphoreCount = 1,
                            .pWaitSemaphores = waitSemaphores,
                            .pWaitDstStageMask = waitStages,
                            .commandBufferCount = 1,
                            .pCommandBuffers = &commandBuffer,
                            .signalSemaphoreCount = 1,
                            .pSignalSemaphores = signalSemaphores};

  vulkanContext.graphicsQueue.submit(
      submitInfo, *vulkanRendererContext
                       .inFlightFences[vulkanRendererContext.currentFrame]);

  vk::SwapchainKHR swapchains[] = {*vulkanContext.swapchain};

  vk::PresentInfoKHR presentInfo{.waitSemaphoreCount = 1,
                                 .pWaitSemaphores = signalSemaphores,
                                 .swapchainCount = 1,
                                 .pSwapchains = swapchains,
                                 .pImageIndices = &imageIndex};

  vulkanContext.presentQueue.presentKHR(presentInfo);

  vulkanRendererContext.currentFrame =
      (vulkanRendererContext.currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
  vulkanRendererContext.particleSimulationFrame++;
}

void uploadAnimationPositions(const std::vector<glm::vec4> &positions) {
  vk::DeviceSize bufferSize = sizeof(glm::vec4) * positions.size();

  BufferWithMemory buffer =
      createBuffer(bufferSize, vk::BufferUsageFlagBits::eStorageBuffer,
                   vk::MemoryPropertyFlagBits::eHostVisible |
                       vk::MemoryPropertyFlagBits::eHostCoherent);

  void *data = buffer.memory.mapMemory(0, bufferSize);
  memcpy(data, positions.data(), static_cast<size_t>(bufferSize));
  buffer.memory.unmapMemory();

  vulkanRendererContext.animationPositionsBuffer = std::move(buffer.buffer);
  vulkanRendererContext.animationPositionsMemory = std::move(buffer.memory);
  vulkanRendererContext.animationPositionCount =
      static_cast<uint32_t>(positions.size());
}
