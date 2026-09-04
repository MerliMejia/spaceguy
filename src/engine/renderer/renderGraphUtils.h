#pragma once
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include "bufferUtils.h"
#include "shaders/shaders.h"
#include <glm/glm.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace Renderer {
namespace RenderGraph {

struct Context {

  struct GlobalUniformBankBuffer {
    std::array<glm::vec4, SG_UNIFORM_TOTAL_SLOTS> data;
  };

  // Data needed
  GlobalUniformBankBuffer globalUniformBufferData{};
  std::vector<BufferAllocationWithMapped> globalUniformBankBuffers;

  // Descriptors
  vk::raii::DescriptorSetLayout defaultDescriptorSetLayout = nullptr;
  vk::raii::DescriptorPool defaultDescriptorPool = nullptr;
  std::vector<vk::raii::DescriptorSet> defaultDescriptorSets;

  Shaders::PushConstantsBank::PushConstantData pushConstantBank{};
};
} // namespace RenderGraph
} // namespace Renderer
