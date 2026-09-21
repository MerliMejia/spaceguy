#pragma once
#include <array>
#include <vector>
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include "bufferUtils.h"
#include "shaders/shaders.h"
#include <glm/glm.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace Renderer {
namespace RenderGraph {

struct Context {

  struct GlobalUniformBankBuffer {
    Shaders::UniformBank::Data data;
  };

  using VerAnimSSBank = std::array<glm::vec4, SG_STORAGE_ANIMATION_SLOTS>;

  // Data needed
  GlobalUniformBankBuffer globalUniformBufferData{};
  std::vector<BufferAllocationWithMapped> globalUniformBankBuffers;
  std::vector<glm::vec4> vertAnimSSBankData;
  BufferAllocation vertAnimSBBankAllocations;

  // Descriptors
  vk::raii::DescriptorSetLayout defaultDescriptorSetLayout = nullptr;
  vk::raii::DescriptorPool defaultDescriptorPool = nullptr;
  std::vector<vk::raii::DescriptorSet> defaultDescriptorSets;

  Shaders::PushConstantsBank::PushConstantData pushConstantBank{};
};
} // namespace RenderGraph
} // namespace Renderer
