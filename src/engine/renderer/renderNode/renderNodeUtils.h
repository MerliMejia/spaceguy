#pragma once

#include <functional>
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <glm/glm.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace Renderer {
namespace RenderNodeUtils {
inline vk::raii::ShaderModule createShaderModule(const std::vector<char> &code,
                                                 vk::raii::Device &device) {
  vk::ShaderModuleCreateInfo createInfo{
      .codeSize = code.size(),
      .pCode = reinterpret_cast<const uint32_t *>(code.data())};

  return vk::raii::ShaderModule{device, createInfo};
}

constexpr int MAX_FRAMES_IN_FLIGHT = 2;

template <typename T>
concept VertexType = requires {
  T::getBindingDescription();
  T::getAttributeDescriptions();
};

struct DefaultVertex {
  glm::vec2 pos;
  glm::vec3 color;
  glm::vec2 uv;

  static vk::VertexInputBindingDescription getBindingDescription() {
    return {.binding = 0,
            .stride = sizeof(DefaultVertex),
            .inputRate = vk::VertexInputRate::eVertex};
  }
  static std::array<vk::VertexInputAttributeDescription, 3>
  getAttributeDescriptions() {
    return {{
        {.location = 0,
         .binding = 0,
         .format = vk::Format::eR32G32Sfloat,
         .offset = offsetof(DefaultVertex, pos)},
        {.location = 1,
         .binding = 0,
         .format = vk::Format::eR32G32B32Sfloat,
         .offset = offsetof(DefaultVertex, color)},
        {.location = 2,
         .binding = 0,
         .format = vk::Format::eR32G32Sfloat,
         .offset = offsetof(DefaultVertex, uv)},
    }};
  }
};

enum class ShaderType {
  None,
  Vertex,
  Fragment,
};

struct ShaderCreateInfo {
  ShaderType type = ShaderType::None;
  std::string name = "";
};

struct RenderCall {
  vk::Buffer vertexBuffer{};
  vk::Buffer indexBuffer{};
  uint32_t indexCount = 0;
  std::function<void()> updatePushConstants;
};
} // namespace RenderNodeUtils
} // namespace Renderer
