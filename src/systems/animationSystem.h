#pragma once

#include "resourceManagementSystem.h"
#include "../utils/time.h"

#include <cstdint>
#include <vector>

struct AnimationDataFromObject {
  uint32_t previousPositionOffset = 0;
  uint32_t nextPositionOffset = 0;
  float interpolation = 0.0f;
};

struct TransformAnimationDataFromObject {
  glm::vec3 location{};
  glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
  glm::vec3 scale{1.0f};
};

void updateAnimations();

AnimationDataFromObject getAnimationDataFromEntity(int entity);
bool hasActiveAnimationEnded(int entity);
