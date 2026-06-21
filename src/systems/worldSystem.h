#pragma once

#include <vector>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "unordered_map"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "../behaviors/wizards/wizardBehavior.h"

struct WizardShootEffect {
  int wizardEntity;
  int effectEntity;
};

struct WorldContext {
  // Camera
  glm::vec3 cameraPosition;
  glm::vec3 cameraLookAt;
  float cameraFovY = glm::radians(45.0f);
  float cameraClipStart = 0.1f;
  float cameraClipEnd = 100.0f;

  // Wizards
  std::vector<WizardBehavior> wizardBehaviors;
  std::unordered_map<int, int> wizzardAttacking;
  std::vector<WizardShootEffect> wizardShootingEffects;
};

extern WorldContext worldContext;

void updateBehaviors();
void updateWizardEffects();
void initializeBehaviors();
