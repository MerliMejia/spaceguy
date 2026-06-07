#include "wizardBehavior.h"
#include "../systems/animationSystem.h"
#include "../utils/time.h"
#include "./decisionTree.h"

static constexpr int MAX_CONSECUTIVE_MOVES = 2;
static constexpr int MAX_CONSECUTIVE_ATTACKS = 2;
static constexpr int MAX_CONSECUTIVE_KICKS = 2;
static constexpr float MOVE_TARGET_EPSILON = 0.000001f;

static bool isActionInProgress(const WizardBehavior &behavior) {
  return behavior.state == WizardState::Moving ||
         behavior.state == WizardState::Attacking ||
         behavior.state == WizardState::Kicking;
}

static glm::vec3 chooseMovePoint(const WizardBehavior &behavior) {
  // Movement currently stays inside the play area centered at world origin.
  return randomPointInCircleXY(glm::vec3{0.0f}, behavior.moveToRadius);
}

static glm::mat4 captureInitialRotation(const Object3D &object) {
  glm::mat4 initialRotation{1.0f};

  initialRotation[0] =
      glm::vec4(glm::normalize(glm::vec3(object.model[0])), 0.0f);
  initialRotation[1] =
      glm::vec4(glm::normalize(glm::vec3(object.model[1])), 0.0f);
  initialRotation[2] =
      glm::vec4(glm::normalize(glm::vec3(object.model[2])), 0.0f);
  initialRotation[3] = glm::vec4{0.0f, 0.0f, 0.0f, 1.0f};

  return initialRotation;
}

static float getForwardYaw(const glm::mat4 &rotation) {
  glm::vec3 localForward{0.0f, -1.0f, 0.0f};
  glm::vec3 forward =
      glm::normalize(glm::vec3(rotation * glm::vec4(localForward, 0.0f)));

  return std::atan2(forward.y, forward.x);
}

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
    behavior.kicks = 0;
  }
}

bool findClosestWizardInRange(const Object3D &self, glm::vec3 &closestPosition,
                              float closestDistanceSquared) {
  const glm::vec3 selfPosition = glm::vec3(self.model[3]);
  bool foundWizard = false;

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

    if (distanceSquared <= closestDistanceSquared) {
      closestPosition = otherPosition;
      closestDistanceSquared = distanceSquared;
      foundWizard = true;
    }
  }

  return foundWizard;
}

static bool wizardIsSomeoneClose(const Object3D &self) {
  glm::vec3 closestPosition{};
  return findClosestWizardInRange(self, closestPosition);
}

static void wizardMoveExecute(Object3D &object, WizardBehavior &behavior) {
  auto transform = modelToTransform(object.model);

  if (behavior.state != WizardState::Moving) {
    behavior.nextMovePoint = chooseMovePoint(behavior);
    behavior.state = WizardState::Moving;
    object.activeAnimation = 3;
  }

  glm::vec2 a{transform.position.x, transform.position.y};
  glm::vec2 b{behavior.nextMovePoint.x, behavior.nextMovePoint.y};
  glm::vec2 delta = b - a;

  float distance = glm::length(delta);

  if (distance > MOVE_TARGET_EPSILON) {
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
    if (wizardIsSomeoneClose(object)) {
      behavior.nextMovePoint = chooseMovePoint(behavior);
      return;
    }

    behavior.tick = 0.0f;
    behavior.state = WizardState::Thinking;
    object.activeAnimation = 1;
    behavior.moves++;
    behavior.attacks = 0;
    behavior.kicks = 0;
  }
}

static void wizardThinkingExecute(Object3D &object, WizardBehavior &behavior) {
  behavior.state = WizardState::Thinking;
  // Should definetly map animations
  object.activeAnimation = 1;
}

static void wizardKickExecute(Object3D &object, WizardBehavior &behavior) {
  if (behavior.state != WizardState::Kicking) {
    object.activeAnimation = 2;
    const AnimationClipGpu &clip =
        object.animatedMesh->animations[object.activeAnimation];

    const int halfFrame = 45;
    object.animationTimeSeconds =
        static_cast<float>(halfFrame - clip.startFrame) /
        object.animatedMesh->fps;
    behavior.state = WizardState::Kicking;
  }
  auto transform = modelToTransform(object.model);

  glm::vec2 a{transform.position.x, transform.position.y};
  glm::vec2 b{behavior.kickingObject.x, behavior.kickingObject.y};

  glm::vec2 delta = b - a;

  float distance = glm::length(delta);

  if (distance > MOVE_TARGET_EPSILON) {
    glm::vec2 direction = delta / distance;
    float targetYaw = std::atan2(direction.y, direction.x);
    float yawDelta = targetYaw - behavior.initialForwardYaw;

    glm::mat4 model{1.0f};
    model = glm::translate(model, transform.position);
    model = glm::rotate(model, yawDelta, glm::vec3{0.0f, 0.0f, 1.0f});
    model = model * behavior.initialRotation;
    model = glm::scale(model, transform.scale);

    object.model = model;
  }

  if (hasActiveAnimationEnded(object)) {
    object.activeAnimation = 1;
    behavior.state = WizardState::Thinking;
    behavior.tick = 0;
    behavior.kicks++;
    behavior.moves = 0;
    behavior.attacks = 0;
  }
}

static void continueCurrentAction(Object3D &object, WizardBehavior &behavior) {
  switch (behavior.state) {
  case WizardState::Moving:
    wizardMoveExecute(object, behavior);
    break;
  case WizardState::Attacking:
    wizardAttackExecute(object, behavior);
    break;
  case WizardState::Thinking:
    wizardThinkingExecute(object, behavior);
    break;
  case WizardState::Kicking:
    wizardKickExecute(object, behavior);
    break;
  }
}

static bool wizardSomeoneIsSuperClose(const Object3D &self,
                                      WizardBehavior &behavior) {
  glm::vec3 closestPosition{};
  bool isInRange =
      findClosestWizardInRange(self, closestPosition, SUPER_CLOSE_RADIUS_SQ);

  if (isInRange) {
    behavior.kickingObject = closestPosition;
    return true;
  }

  return false;
}

static void initializeActionNodes(Object3D &object, WizardBehavior &behavior) {
  behavior.attackNode.execute = [&object, &behavior]() {
    wizardAttackExecute(object, behavior);
  };

  behavior.kickNode.execute = [&object, &behavior]() {
    wizardKickExecute(object, behavior);
  };

  behavior.moveNode.execute = [&object, &behavior]() {
    wizardMoveExecute(object, behavior);
  };

  behavior.thinkingNode.execute = [&object, &behavior]() {
    wizardThinkingExecute(object, behavior);
  };

  behavior.continueActionNode.execute = [&object, &behavior]() {
    continueCurrentAction(object, behavior);
  };
}

static void initializeConditionNodes(Object3D &object,
                                     WizardBehavior &behavior) {
  behavior.someoneIsClose = [&object]() {
    return wizardIsSomeoneClose(object);
  };

  behavior.someoneIsSuperClose = [&object, &behavior]() {
    return wizardSomeoneIsSuperClose(object, behavior);
  };

  behavior.actionInProgressNode.conditions = [&behavior]() {
    return isActionInProgress(behavior);
  };

  behavior.thoughtEnoughNode.conditions = [&behavior]() {
    return behavior.tick > behavior.thinkingTime;
  };

  behavior.someoneIsCloseNode.conditions = behavior.someoneIsClose;

  behavior.someoneIsSuperCloseNode.conditions = behavior.someoneIsSuperClose;

  behavior.tooManyMovesNode.conditions = [&behavior]() {
    return behavior.moves >= MAX_CONSECUTIVE_MOVES;
  };

  behavior.tooManyAttacksNode.conditions = [&behavior]() {
    return behavior.attacks >= MAX_CONSECUTIVE_ATTACKS;
  };

  behavior.tooManyKicksNode.conditions = [&behavior]() {
    return behavior.kicks >= MAX_CONSECUTIVE_KICKS;
  };
}

static void connectDecisionTree(WizardBehavior &behavior) {
  behavior.someoneIsSuperCloseNode.yes = &behavior.tooManyKicksNode;
  behavior.someoneIsSuperCloseNode.no = &behavior.actionInProgressNode;

  behavior.tooManyKicksNode.yes = &behavior.moveNode;
  behavior.tooManyKicksNode.no = &behavior.kickNode;

  behavior.actionInProgressNode.yes = &behavior.continueActionNode;
  behavior.actionInProgressNode.no = &behavior.thoughtEnoughNode;

  behavior.thoughtEnoughNode.yes = &behavior.someoneIsCloseNode;
  behavior.thoughtEnoughNode.no = &behavior.thinkingNode;

  behavior.someoneIsCloseNode.yes = &behavior.tooManyMovesNode;
  behavior.someoneIsCloseNode.no = &behavior.tooManyAttacksNode;

  behavior.tooManyMovesNode.yes = &behavior.attackNode;
  behavior.tooManyMovesNode.no = &behavior.moveNode;

  behavior.tooManyAttacksNode.yes = &behavior.moveNode;
  behavior.tooManyAttacksNode.no = &behavior.attackNode;
}

void initializeWizardDecisionTree(Object3D &object, WizardBehavior &behavior) {
  initializeActionNodes(object, behavior);
  initializeConditionNodes(object, behavior);
  connectDecisionTree(behavior);
}

void behaveLikeWizzard(Object3D &object, WizardBehavior &currentBehavior) {
  currentBehavior.tick += timeState.deltaTime;

  if (!currentBehavior.hasInitialRotation) {
    currentBehavior.initialRotation = captureInitialRotation(object);
    currentBehavior.initialForwardYaw =
        getForwardYaw(currentBehavior.initialRotation);
    currentBehavior.hasInitialRotation = true;
  }

  evaluateDecisionTree(&currentBehavior.someoneIsSuperCloseNode);
}
