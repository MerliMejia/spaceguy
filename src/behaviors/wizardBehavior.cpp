#include "wizardBehavior.h"
#include "../systems/animationSystem.h"
#include "../utils/time.h"
#include "./decisionTree.h"

static void wizardAttackExecute(Object3D &object, WizardBehavior &behavior) {
  if (behavior.state != WizardState::Attacking) {
    object.activeAnimation = 0;
    object.animationTimeSeconds = 0;
    behavior.state = WizardState::Attacking;
  }

  if (hasActiveAnimationEnded(object)) {
    object.activeAnimation = 1;
    behavior.state = WizardState::Thinking;
    behavior.tick = 0;
    behavior.attacks++;
    behavior.moves = 0;
  }
}
static void wizardMoveExecute(Object3D &object, WizardBehavior &behavior) {

  auto transform = modelToTransform(object.model);

  if (behavior.state != WizardState::Moving) {
    // The center should be "the position 0,0 of the current scene when the
    // camera is not moving"
    behavior.nextMovePoint =
        randomPointInCircleXY(glm::vec3(0.0f), behavior.moveToRadius);

    behavior.state = WizardState::Moving;
    object.activeAnimation = 3;
  }

  glm::vec2 a{transform.position.x, transform.position.y};
  glm::vec2 b{behavior.nextMovePoint.x, behavior.nextMovePoint.y};
  glm::vec2 delta = b - a;

  float distance = glm::length(delta);

  if (distance > 0.000001f) {
    glm::vec2 direction = delta / distance;

    float targetYaw = std::atan2(direction.y, direction.x);
    float yawDelta = targetYaw - behavior.initialForwardYaw;

    float step = glm::min(behavior.speed * timeState.deltaTime, distance);
    a += direction * step;

    transform.position.x = a.x;
    transform.position.y = a.y;

    glm::mat4 model{1.0f};
    model = glm::translate(model, transform.position);
    model = glm::rotate(model, yawDelta, glm::vec3{0.0f, 0.0f, 1.0f});
    model = model * behavior.initialRotation;
    model = glm::scale(model, transform.scale);

    object.model = model;
  } else {
    behavior.tick = 0.0f;
    behavior.state = WizardState::Thinking;
    object.activeAnimation = 1;
    behavior.moves++;
    behavior.attacks = 0;
  }
}
static void wizardThinkingExecute(Object3D &object, WizardBehavior &behavior) {
  behavior.state = WizardState::Thinking;
  // Should definetly map animations
  object.activeAnimation = 1;
}

static bool wizardIsSomeoneClose(const Object3D &self) {
  const glm::vec3 selfPosition = glm::vec3(self.model[3]);

  for (const Object3D &other : vulkanRendererContext.objects) {
    if (&other == &self) {
      continue;
    }

    if (other.worldKind != ObjectWorldKind::Wizard) {
      continue;
    }

    const glm::vec3 otherPosition = glm::vec3(other.model[3]);

    const glm::vec2 delta{
        otherPosition.x - selfPosition.x,
        otherPosition.y - selfPosition.y,
    };

    const float distanceSquared = glm::dot(delta, delta);

    if (distanceSquared < RADIUS_SQ) {
      return true;
    }
  }

  return false;
}

void initializeWizardDecisionTree(Object3D &object, WizardBehavior &behavior) {

  behavior.attackNode.execute = [&object, &behavior]() {
    wizardAttackExecute(object, behavior);
  };

  behavior.moveNode.execute = [&object, &behavior]() {
    wizardMoveExecute(object, behavior);
  };

  behavior.thinkingNode.execute = [&object, &behavior]() {
    wizardThinkingExecute(object, behavior);
  };

  behavior.tooManyMovesNode.conditions = [&behavior]() {
    return behavior.moves >= 2;
  };

  behavior.tooManyAttacksNode.conditions = [&behavior]() {
    return behavior.attacks >= 2;
  };

  behavior.someoneIsCloseNode.conditions = [&object]() {
    return wizardIsSomeoneClose(object);
  };

  behavior.decisionTree.conditions = [&behavior]() {
    return behavior.tick > behavior.thinkingTime;
  };

  behavior.tooManyMovesNode.yes = &behavior.attackNode;
  behavior.tooManyMovesNode.no = &behavior.moveNode;

  behavior.tooManyAttacksNode.yes = &behavior.moveNode;
  behavior.tooManyAttacksNode.no = &behavior.attackNode;

  behavior.someoneIsCloseNode.yes = &behavior.tooManyMovesNode;
  behavior.someoneIsCloseNode.no = &behavior.tooManyAttacksNode;

  behavior.decisionTree.yes = &behavior.someoneIsCloseNode;
  behavior.decisionTree.no = &behavior.thinkingNode;
}

void behaveLikeWizzard(Object3D &object, WizardBehavior &currentBehavior) {

  currentBehavior.tick += timeState.deltaTime;

  if (!currentBehavior.hasInitialRotation) {
    glm::mat4 initialRotation{1.0f};

    initialRotation[0] =
        glm::vec4(glm::normalize(glm::vec3(object.model[0])), 0.0f);
    initialRotation[1] =
        glm::vec4(glm::normalize(glm::vec3(object.model[1])), 0.0f);
    initialRotation[2] =
        glm::vec4(glm::normalize(glm::vec3(object.model[2])), 0.0f);
    initialRotation[3] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

    glm::vec3 localForward{0.0f, -1.0f, 0.0f}; // -Y
    glm::vec3 initialForward = glm::normalize(
        glm::vec3(initialRotation * glm::vec4(localForward, 0.0f)));

    currentBehavior.initialRotation = initialRotation;
    currentBehavior.initialForwardYaw =
        std::atan2(initialForward.y, initialForward.x);
    currentBehavior.hasInitialRotation = true;
  }

  evaluateDecisionTree(&currentBehavior.decisionTree);
}
