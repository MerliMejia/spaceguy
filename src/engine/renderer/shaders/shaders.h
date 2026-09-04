#pragma once

#include "../../../shaders/v2/banks_shared.h"

#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <glm/glm.hpp>

namespace Renderer {
namespace Shaders {

namespace UniformBank {

using Data = std::array<glm::vec4, SG_UNIFORM_TOTAL_SLOTS>;

inline void setFloat(Data &bank, uint32_t index, float value) {
  const uint32_t slot = SG_UNIFORM_FLOAT_FROM + index / 4;
  const uint32_t component = index % 4;

  bank[slot][component] = value;
}

inline float getFloat(const Data &bank, uint32_t index) {
  const uint32_t slot = SG_UNIFORM_FLOAT_FROM + index / 4;
  const uint32_t component = index % 4;

  return bank[slot][component];
}

inline void setFloat2(Data &bank, uint32_t index, const glm::vec2 &value) {
  const uint32_t slot = SG_UNIFORM_VEC2_FROM + index / 2;
  const uint32_t firstComponent = (index % 2) * 2;

  bank[slot][firstComponent] = value.x;
  bank[slot][firstComponent + 1] = value.y;
}

inline glm::vec2 getFloat2(const Data &bank, uint32_t index) {
  const uint32_t slot = SG_UNIFORM_VEC2_FROM + index / 2;
  const uint32_t firstComponent = (index % 2) * 2;

  return {
      bank[slot][firstComponent],
      bank[slot][firstComponent + 1],
  };
}

inline void setFloat3(Data &bank, uint32_t index, const glm::vec3 &value) {
  const uint32_t slot = SG_UNIFORM_VEC3_FROM + index;

  bank[slot].x = value.x;
  bank[slot].y = value.y;
  bank[slot].z = value.z;
}

inline glm::vec3 getFloat3(const Data &bank, uint32_t index) {
  const uint32_t slot = SG_UNIFORM_VEC3_FROM + index;
  return glm::vec3{bank[slot]};
}

inline void setFloat4(Data &bank, uint32_t index, const glm::vec4 &value) {
  const uint32_t slot = SG_UNIFORM_VEC4_FROM + index;

  bank[slot].x = value.x;
  bank[slot].y = value.y;
  bank[slot].z = value.z;
  bank[slot].w = value.w;
}

inline glm::vec4 getFloat4(const Data &bank, uint32_t index) {
  const uint32_t slot = SG_UNIFORM_VEC4_FROM + index;
  return glm::vec4{bank[slot]};
}

inline void setFloat4x4(Data &bank, uint32_t index, const glm::mat4 &value) {
  const uint32_t firstSlot = SG_UNIFORM_MAT4_FROM + index * 4;

  // Store the matrix as four row vectors, matching banks.slang.
  for (uint32_t row = 0; row < 4; ++row) {
    bank[firstSlot + row] = {
        value[0][row],
        value[1][row],
        value[2][row],
        value[3][row],
    };
  }
}

inline glm::mat4 getFloat4x4(const Data &bank, uint32_t index) {
  const uint32_t firstSlot = SG_UNIFORM_MAT4_FROM + index * 4;
  glm::mat4 result{};

  for (uint32_t row = 0; row < 4; ++row) {
    for (uint32_t column = 0; column < 4; ++column) {
      result[column][row] = bank[firstSlot + row][column];
    }
  }

  return result;
}

} // namespace UniformBank

namespace PushConstantsBank {

struct alignas(16) PushConstantData {
  std::array<glm::vec4, SG_PUSH_INT_FROM> floatData{};
  std::array<glm::uvec4, SG_PUSH_INT_SLOTS> integerData{};
};

inline void setFloat(PushConstantData &bank, uint32_t index, float value) {
  const uint32_t slot = SG_PUSH_FLOAT_FROM + index / 4;
  const uint32_t component = index % 4;

  bank.floatData[slot][component] = value;
}

inline float getFloat(const PushConstantData &bank, uint32_t index) {
  const uint32_t slot = SG_PUSH_FLOAT_FROM + index / 4;
  const uint32_t component = index % 4;

  return bank.floatData[slot][component];
}

inline void setFloat2(PushConstantData &bank, uint32_t index,
                      const glm::vec2 &value) {
  const uint32_t slot = SG_PUSH_VEC2_FROM + index / 2;
  const uint32_t firstComponent = (index % 2) * 2;

  bank.floatData[slot][firstComponent] = value.x;
  bank.floatData[slot][firstComponent + 1] = value.y;
}

inline glm::vec2 getFloat2(const PushConstantData &bank, uint32_t index) {
  const uint32_t slot = SG_PUSH_VEC2_FROM + index / 2;
  const uint32_t firstComponent = (index % 2) * 2;

  return {
      bank.floatData[slot][firstComponent],
      bank.floatData[slot][firstComponent + 1],
  };
}

inline void setFloat3(PushConstantData &bank, uint32_t index,
                      const glm::vec3 &value) {
  const uint32_t slot = SG_PUSH_VEC3_FROM + index;

  bank.floatData[slot].x = value.x;
  bank.floatData[slot].y = value.y;
  bank.floatData[slot].z = value.z;
}

inline glm::vec3 getFloat3(const PushConstantData &bank, uint32_t index) {
  const uint32_t slot = SG_PUSH_VEC3_FROM + index;
  return glm::vec3{bank.floatData[slot]};
}

inline void setUInt(PushConstantData &bank, uint32_t index, uint32_t value) {
  const uint32_t slot = index / 4;
  const uint32_t component = index % 4;

  bank.integerData[slot][component] = value;
}

inline uint32_t getUInt(const PushConstantData &bank, uint32_t index) {
  const uint32_t slot = index / 4;
  const uint32_t component = index % 4;

  return bank.integerData[slot][component];
}

inline void setInt(PushConstantData &bank, uint32_t index, int32_t value) {
  setUInt(bank, index, std::bit_cast<uint32_t>(value));
}

inline int32_t getInt(const PushConstantData &bank, uint32_t index) {
  return std::bit_cast<int32_t>(getUInt(bank, index));
}

} // namespace PushConstantsBank
} // namespace Shaders
} // namespace Renderer
