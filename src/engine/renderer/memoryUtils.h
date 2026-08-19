#pragma once

#include "shaders.h"
#include <cstdint>
#include <glm/glm.hpp>

namespace Renderer {
namespace Memory {
inline void updateMat4ByIndex(
    uint32_t index,
    std::array<glm::vec4, Renderer::Shaders::UniformBank::TOTAL_SLOTS> &bank,
    const glm::mat4 &data) {
  const uint32_t firstSlot =
      Renderer::Shaders::UniformBank::MAT4_FROM + index * 4;

  for (uint32_t row = 0; row < 4; ++row) {
    bank[firstSlot + row] = glm::vec4{
        data[0][row],
        data[1][row],
        data[2][row],
        data[3][row],
    };
  }
}

inline glm::mat4 getMat4ByIndex(
    uint32_t index,
    const std::array<glm::vec4, Renderer::Shaders::UniformBank::TOTAL_SLOTS>
        &bank) {

  assert(index < Renderer::Shaders::UniformBank::MAT4_COUNT);

  const uint32_t firstSlot =
      Renderer::Shaders::UniformBank::MAT4_FROM + index * 4;

  glm::mat4 result{};

  for (uint32_t row = 0; row < 4; ++row) {
    for (uint32_t column = 0; column < 4; ++column) {
      result[column][row] = bank[firstSlot + row][column];
    }
  }

  return result;
}
} // namespace Memory
} // namespace Renderer
