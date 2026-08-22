#pragma once
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include "bufferUtils.h"
#include "shaders.h"
#include <glm/glm.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace Renderer {
namespace RenderGraph {

struct Context {

  struct GlobalUniformBankBuffer {
    std::array<glm::vec4, Renderer::Shaders::UniformBank::TOTAL_SLOTS> data;
  };

  // Everything we need for a global uniform bank
  vk::raii::DescriptorSetLayout globalUniformBankDescriptorSetLayout = nullptr;
  GlobalUniformBankBuffer globalUniformBufferData{};
  std::vector<BufferAllocationWithMapped> globalUniformBankBuffers;
  vk::raii::DescriptorPool globalUniformBankDescriptorPool = nullptr;
  std::vector<vk::raii::DescriptorSet> globalUniformBankDescriptorSets;
};
} // namespace RenderGraph
} // namespace Renderer
