#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

struct Transform {
  glm::vec3 position;
  glm::vec3 scale;
  glm::quat rotation;
};

glm::mat4 transformToModel(glm::vec3 position, glm::vec3 rotation,
                           glm::vec3 scale);
Transform modelToTransform(glm::mat4 &model);
glm::vec3 randomPointInCircleXY(const glm::vec3 &center, float radius);

inline constexpr float CHECK_RADIUS = 4.0f;
inline constexpr float RADIUS_SQ = CHECK_RADIUS * CHECK_RADIUS;
inline constexpr float SUPER_CLOSE_RADIUS = 1.1f;
inline constexpr float SUPER_CLOSE_RADIUS_SQ =
    SUPER_CLOSE_RADIUS * SUPER_CLOSE_RADIUS;
