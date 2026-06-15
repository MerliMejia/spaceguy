#include "worldSystem.h"
#include <vector>

#include "../behaviors/wizardBehavior.h"

static std::vector<WizardBehavior> wizardBehaviors;

WorldContext worldContext;

static glm::vec4 debugColorForWizardState(WizardState state) {
  switch (state) {
  case WizardState::Thinking:
    return glm::vec4{0.25f, 0.45f, 1.0f, 1.0f};
  case WizardState::Moving:
    return glm::vec4{0.0f, 0.85f, 1.0f, 1.0f};
  case WizardState::Attacking:
    return glm::vec4{1.0f, 0.05f, 0.05f, 1.0f};
  case WizardState::Kicking:
    return glm::vec4{1.0f, 0.75f, 0.05f, 1.0f};
  }

  return glm::vec4{1.0f, 1.0f, 1.0f, 1.0f};
}

static void drawWizardDebug(const Object3D &object,
                            const WizardBehavior &behavior) {
  const glm::vec3 position = glm::vec3(object.model[3]);
  const bool someoneIsClose = behavior.someoneIsClose();
  const bool someoneIsSuperClose = behavior.someoneIsSuperClose();

  const glm::vec4 radiusColor = someoneIsClose
                                    ? glm::vec4{1.0f, 0.05f, 0.05f, 1.0f}
                                    : glm::vec4{0.25f, 0.45f, 1.0f, 1.0f};
  const glm::vec4 superCloseRadiusColor =
      someoneIsSuperClose ? glm::vec4{1.0f, 0.05f, 0.05f, 1.0f}
                          : glm::vec4{0.25f, 0.45f, 1.0f, 1.0f};

  addDebugDiskXY(position, CHECK_RADIUS, radiusColor);
  addDebugDiskXY(position, SUPER_CLOSE_RADIUS, superCloseRadiusColor);

  const glm::vec3 forward = glm::normalize(
      glm::vec3(object.model * glm::vec4{0.0f, -1.0f, 0.0f, 0.0f}));
  addDebugLine(position, position + forward * 2.0f,
               glm::vec4{1.0f, 0.0f, 0.85f, 1.0f});

  if (behavior.state == WizardState::Moving) {
    addDebugCube(behavior.nextMovePoint, 0.75f,
                 glm::vec4{1.0f, 0.45f, 0.0f, 1.0f});

    addDebugLine(position, behavior.nextMovePoint,
                 glm::vec4{1.0f, 0.45f, 0.0f, 1.0f});
  }

  glm::vec3 closestWizardPosition{};
  if (findClosestWizardInRange(object, closestWizardPosition)) {
    addDebugLine(position, closestWizardPosition,
                 glm::vec4{1.0f, 0.0f, 0.0f, 1.0f});
  }

  addDebugLine(position, position + glm::vec3(0, 0, 5),
               debugColorForWizardState(behavior.state));
}

void initializeBehaviors() {
  std::size_t wizardCount = 0;

  for (const Object3D &object : vulkanRendererContext.objects) {
    if (object.worldKind == ObjectWorldKind::Wizard) {
      wizardCount++;
    }
  }

  wizardBehaviors.reserve(wizardCount);

  for (Object3D &object : vulkanRendererContext.objects) {
    if (object.worldKind != ObjectWorldKind::Wizard) {
      continue;
    }

    WizardBehavior &behavior = wizardBehaviors.emplace_back();
    initializeWizardDecisionTree(object, behavior);
  }
}

void updateBehaviors() {
  std::size_t wizardIndex = 0;
  for (int i = 0; i < vulkanRendererContext.objects.size(); i++) {
    Object3D &object = vulkanRendererContext.objects[i];

    switch (object.worldKind) {
    case ObjectWorldKind::None:
      break;
    case ObjectWorldKind::Floor:
      break;
    case ObjectWorldKind::Wizard: {
      WizardBehavior &behavior = wizardBehaviors[wizardIndex++];
      behaveLikeWizzard(object, behavior);
      if (vulkanRendererContext.isDebug) {
        drawWizardDebug(object, behavior);
      }
    }
    }
  }
}
