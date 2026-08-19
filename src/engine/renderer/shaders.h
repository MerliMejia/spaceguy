#pragma once

#include <cstdint>

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

} // namespace UniformBank
} // namespace Shaders
} // namespace Renderer
