#pragma once

#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <glm/glm.hpp>

namespace Renderer {
namespace Shaders {
namespace UniformBank {
constexpr uint32_t FLOAT_COUNT = 100;
constexpr uint32_t VEC2_COUNT = 100;
constexpr uint32_t VEC3_COUNT = 100;
constexpr uint32_t MAT4_COUNT = 100;

constexpr uint32_t FLOAT_SLOTS = (FLOAT_COUNT + 3) / 4;
constexpr uint32_t VEC2_SLOTS = (VEC2_COUNT + 1) / 2;
constexpr uint32_t VEC3_SLOTS = VEC3_COUNT;
constexpr uint32_t MAT4_SLOTS = MAT4_COUNT * 4;

constexpr uint32_t FLOAT_FROM = 0;
constexpr uint32_t VEC2_FROM = FLOAT_FROM + FLOAT_SLOTS;
constexpr uint32_t VEC3_FROM = VEC2_FROM + VEC2_SLOTS;
constexpr uint32_t MAT4_FROM = VEC3_FROM + VEC3_SLOTS;

constexpr uint32_t TOTAL_SLOTS = MAT4_FROM + MAT4_SLOTS;

// Shared matrices occupy the final two logical mat4 entries.
constexpr uint32_t projIndex = MAT4_COUNT - 2;
constexpr uint32_t viewIndex = MAT4_COUNT - 1;

using Data = std::array<glm::vec4, TOTAL_SLOTS>;

inline void setFloat(Data &bank, uint32_t index, float value) {
  const uint32_t slot = FLOAT_FROM + index / 4;
  const uint32_t component = index % 4;

  bank[slot][component] = value;
}

inline float getFloat(const Data &bank, uint32_t index) {
  const uint32_t slot = FLOAT_FROM + index / 4;
  const uint32_t component = index % 4;

  return bank[slot][component];
}

inline void setFloat2(Data &bank, uint32_t index, const glm::vec2 &value) {
  const uint32_t slot = VEC2_FROM + index / 2;
  const uint32_t firstComponent = (index % 2) * 2;

  bank[slot][firstComponent] = value.x;
  bank[slot][firstComponent + 1] = value.y;
}

inline glm::vec2 getFloat2(const Data &bank, uint32_t index) {
  const uint32_t slot = VEC2_FROM + index / 2;
  const uint32_t firstComponent = (index % 2) * 2;

  return {
      bank[slot][firstComponent],
      bank[slot][firstComponent + 1],
  };
}

inline void setFloat3(Data &bank, uint32_t index, const glm::vec3 &value) {
  const uint32_t slot = VEC3_FROM + index;

  bank[slot].x = value.x;
  bank[slot].y = value.y;
  bank[slot].z = value.z;
}

inline glm::vec3 getFloat3(const Data &bank, uint32_t index) {
  const uint32_t slot = VEC3_FROM + index;
  return glm::vec3{bank[slot]};
}

inline void setFloat4x4(Data &bank, uint32_t index, const glm::mat4 &value) {
  const uint32_t firstSlot = MAT4_FROM + index * 4;

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
  const uint32_t firstSlot = MAT4_FROM + index * 4;
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

constexpr uint32_t FLOAT_COUNT = 4;
constexpr uint32_t VEC2_COUNT = 4;
constexpr uint32_t VEC3_COUNT = 1;
constexpr uint32_t INT_COUNT = 16;

constexpr uint32_t FLOAT_SLOTS = (FLOAT_COUNT + 3) / 4;
constexpr uint32_t VEC2_SLOTS = (VEC2_COUNT + 1) / 2;
constexpr uint32_t VEC3_SLOTS = VEC3_COUNT;
constexpr uint32_t INT_SLOTS = (INT_COUNT + 3) / 4;

constexpr uint32_t FLOAT_FROM = 0;
constexpr uint32_t VEC2_FROM = FLOAT_FROM + FLOAT_SLOTS;
constexpr uint32_t VEC3_FROM = VEC2_FROM + VEC2_SLOTS;
constexpr uint32_t INT_FROM = VEC3_FROM + VEC3_SLOTS;

constexpr uint32_t TOTAL_SLOTS = INT_FROM + INT_SLOTS;
constexpr uint32_t TOTAL_BYTES = TOTAL_SLOTS * 16;

struct alignas(16) PushConstantData {
  std::array<glm::vec4, PushConstantsBank::INT_FROM> floatData{};
  std::array<glm::uvec4, PushConstantsBank::INT_SLOTS> integerData{};
};

inline void setFloat(PushConstantData &bank, uint32_t index, float value) {
  const uint32_t slot = FLOAT_FROM + index / 4;
  const uint32_t component = index % 4;

  bank.floatData[slot][component] = value;
}

inline float getFloat(const PushConstantData &bank, uint32_t index) {
  const uint32_t slot = FLOAT_FROM + index / 4;
  const uint32_t component = index % 4;

  return bank.floatData[slot][component];
}

inline void setFloat2(PushConstantData &bank, uint32_t index,
                      const glm::vec2 &value) {
  const uint32_t slot = VEC2_FROM + index / 2;
  const uint32_t firstComponent = (index % 2) * 2;

  bank.floatData[slot][firstComponent] = value.x;
  bank.floatData[slot][firstComponent + 1] = value.y;
}

inline glm::vec2 getFloat2(const PushConstantData &bank, uint32_t index) {
  const uint32_t slot = VEC2_FROM + index / 2;
  const uint32_t firstComponent = (index % 2) * 2;

  return {
      bank.floatData[slot][firstComponent],
      bank.floatData[slot][firstComponent + 1],
  };
}

inline void setFloat3(PushConstantData &bank, uint32_t index,
                      const glm::vec3 &value) {
  const uint32_t slot = VEC3_FROM + index;

  bank.floatData[slot].x = value.x;
  bank.floatData[slot].y = value.y;
  bank.floatData[slot].z = value.z;
}

inline glm::vec3 getFloat3(const PushConstantData &bank, uint32_t index) {
  const uint32_t slot = VEC3_FROM + index;
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
