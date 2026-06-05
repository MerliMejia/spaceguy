#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "../engine/vulkanRenderer.h"
#include "../utils/math.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

enum class WizardState { Thinking, Moving, Attacking };

struct WizardBehavior {
  float speed = 5.0f;
  float moveToRadius = 10.0f;
  float thinkingTime = 3.0f;
  float tick = 0.0f;

  glm::vec3 nextMovePoint;
  WizardState state = WizardState::Thinking;

  bool hasInitialRotation = false;
  glm::mat4 initialRotation{1.0f};
  float initialForwardYaw = 0.0f;

  // Accomulators
  int moves = 0;
  int attacks = 0;
};

void behaveLikeWizzard(Object3D &object, WizardBehavior &currentBehavior);
