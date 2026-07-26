#include "gravitySystem.h"
#include "../utils/math.h"
#include "../utils/time.h"
#include "resourceManagementSystem.h"
#include <iostream>
#include <unordered_map>

constexpr float FIXED_MS = 0.01666; // ~60FPS
constexpr float G = -27.0f;
constexpr glm::vec3 GRAVITY = {0, 0, G};
constexpr float DAMPING_FACTOR = 0.95f;
static float msAccumulator = 0;

std::unordered_map<int, float> initialZPositions;

static void update() {
  for (GravityComponent &gravity : resources.gravities) {
    TransformComponent &tc = getTransform(gravity.entity);
    Transform transform = modelToTransform(tc.model);

    float initialZPosition = initialZPositions[gravity.entity];

    gravity.velocity += GRAVITY * FIXED_MS;
    glm::vec3 candidatePosition =
        transform.position + gravity.velocity * FIXED_MS;

    if (candidatePosition.z <= initialZPosition) {
      candidatePosition.z = initialZPosition;
      if (gravity.velocity.z < 0) {
        gravity.velocity.z = 0;
      }
    }

    transform.position = candidatePosition;

    gravity.velocity = gravity.velocity * DAMPING_FACTOR;

    tc.model = transformToModel(transform.position, transform.rotation,
                                transform.scale);
  }
}

void updateGravities() {
  msAccumulator += timeState.deltaTime;
  if (msAccumulator >= FIXED_MS) {
    update();
    msAccumulator -= FIXED_MS;
  }
}

void initGravities() {
  for (GravityComponent &gravity : resources.gravities) {
    TransformComponent &tc = getTransform(gravity.entity);
    Transform transform = modelToTransform(tc.model);

    initialZPositions[gravity.entity] = transform.position.z;
  }
}
