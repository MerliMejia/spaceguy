#pragma once
#include "../../../utils/types.h"
#include "../bufferUtils.h"
#include "../shaders/shaders.h"
#include "glm/fwd.hpp"
#include <array>
#include <glm/glm.hpp>
#include <vector>
#include <vulkan/vulkan_raii.hpp>

namespace Renderer {
namespace RenderGraph {

struct Context {

  struct GlobalUniformBankBuffer {
    Shaders::UniformBank::Data data;
  };

  struct ParticleRange {
    size_t from = 0;
    size_t to = 0;
  };

  using VerAnimSSBank = std::array<glm::vec4, SG_STORAGE_ANIMATION_SLOTS>;
  using ParticlesData = std::array<Renderer::Types::Particle, MAX_PARTICLES>;

  // Data needed
  // Uniforms
  GlobalUniformBankBuffer globalUniformBufferData{};
  std::vector<BufferAllocationWithMapped> globalUniformBankBuffers;
  // Animations
  std::vector<glm::vec4> vertAnimSSBankData;
  BufferAllocation vertAnimSBBankAllocations;
  // Particles
  ParticlesData particlesData;
  BufferAllocation particlesAllocations;
  std::vector<BufferAllocationWithMapped> particlesStagingBuffers;
  std::vector<ParticleRange> particlesDirtyRanges;
  std::vector<vk::BufferCopy> particlesCopyRegions;

  // Descriptors
  vk::raii::DescriptorSetLayout defaultDescriptorSetLayout = nullptr;
  vk::raii::DescriptorSetLayout computeDescriptorSetLayout = nullptr;
  vk::raii::DescriptorPool defaultDescriptorPool = nullptr;
  std::vector<vk::raii::DescriptorSet> defaultDescriptorSets;
  std::vector<vk::raii::DescriptorSet> computeDescriptorSets;

  // Banks
  Shaders::PushConstantsBank::PushConstantData pushConstantBank{};
};
} // namespace RenderGraph
} // namespace Renderer
