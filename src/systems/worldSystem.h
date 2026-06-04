#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct WorldContext {
  // Camera
  glm::vec3 cameraPosition;
  glm::vec3 cameraLookAt;
  float cameraFovY = glm::radians(45.0f);
  float cameraClipStart = 0.1f;
  float cameraClipEnd = 100.0f;
};

extern WorldContext worldContext;

void updateBehaviors();
