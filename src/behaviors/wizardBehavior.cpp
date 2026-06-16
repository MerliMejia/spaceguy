#include "wizardBehavior.h"
#include "../systems/animationSystem.h"
#include "../systems/worldSystem.h"
#include "../utils/time.h"
#include "./decisionTree.h"
#include <cstdint>
#include <utility>

static constexpr int MAX_CONSECUTIVE_MOVES = 2;
static constexpr int MAX_CONSECUTIVE_ATTACKS = 2;
static constexpr int MAX_CONSECUTIVE_KICKS = 2;
static constexpr float MOVE_TARGET_EPSILON = 0.000001f;

static bool isActionInProgress(const WizardBehavior &behavior) {
  return behavior.state == WizardState::Moving ||
         behavior.state == WizardState::Attacking ||
         behavior.state == WizardState::Kicking;
}

static bool wizardLookAtPoint(Object3D &object, const WizardBehavior &behavior,
                              const glm::vec3 &targetPoint) {
  auto transform = modelToTransform(object.model);

  glm::vec2 a{transform.position.x, transform.position.y};
  glm::vec2 b{targetPoint.x, targetPoint.y};
  glm::vec2 delta = b - a;

  float distance = glm::length(delta);

  if (distance <= MOVE_TARGET_EPSILON) {
    return false;
  }

  glm::vec2 direction = delta / distance;
  float targetYaw = std::atan2(direction.y, direction.x);
  float yawDelta = targetYaw - behavior.initialForwardYaw;

  glm::mat4 model{1.0f};
  model = glm::translate(model, transform.position);
  model = glm::rotate(model, yawDelta, glm::vec3{0.0f, 0.0f, 1.0f});
  model = model * behavior.initialRotation;
  model = glm::scale(model, transform.scale);

  object.model = model;
  return true;
}

static void applyWizardTransformFacing(Object3D &object,
                                       const WizardBehavior &behavior,
                                       const Transform &transform,
                                       const glm::vec3 &targetPoint) {
  glm::vec2 a{transform.position.x, transform.position.y};
  glm::vec2 b{targetPoint.x, targetPoint.y};
  glm::vec2 delta = b - a;

  float distance = glm::length(delta);
  if (distance <= MOVE_TARGET_EPSILON) {
    return;
  }

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

static void applyWizardTransformFacingDirection(Object3D &object,
                                                const WizardBehavior &behavior,
                                                const Transform &transform,
                                                const glm::vec2 &direction) {
  float targetYaw = std::atan2(direction.y, direction.x);
  float yawDelta = targetYaw - behavior.initialForwardYaw;

  glm::mat4 model{1.0f};
  model = glm::translate(model, transform.position);
  model = glm::rotate(model, yawDelta, glm::vec3{0.0f, 0.0f, 1.0f});
  model = model * behavior.initialRotation;
  model = glm::scale(model, transform.scale);

  object.model = model;
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
  auto transform = modelToTransform(object.model);

  if (behavior.state != WizardState::Attacking) {
    if (worldContext.wizardBehaviors.size() < 2) {
      behavior.state = WizardState::Thinking;
      object.activeAnimation = WizardAnimationMapping::Iddle;
      return;
    }

    object.activeAnimation = WizardAnimationMapping::Attacking;
    object.animationTimeSeconds = 0;
    behavior.state = WizardState::Attacking;

    // Select a random wizard to attack
    // At some point should not only be wizards
    size_t randomIndex;
    do {
      randomIndex =
          randomIndexFromValue<WizardBehavior>(worldContext.wizardBehaviors);
    } while (worldContext.wizardBehaviors[randomIndex].id == behavior.id);

    WizardBehavior &selected = worldContext.wizardBehaviors[randomIndex];

    // Who are we attacking?
    worldContext.wizzardAttacking[selected.id] = behavior.id;
    behavior.currentAttackingIndex = randomIndex;
  }

  if (behavior.currentAttackingIndex >= worldContext.wizardBehaviors.size()) {
    behavior.state = WizardState::Thinking;
    object.activeAnimation = WizardAnimationMapping::Iddle;
    return;
  }

  const WizardBehavior &targetBehavior =
      worldContext.wizardBehaviors[behavior.currentAttackingIndex];
  const Object3D &targetObject =
      vulkanRendererContext.objects[targetBehavior.id];

  // Make it look to where it is attacking.
  glm::vec2 currentPosition{transform.position.x, transform.position.y};
  glm::vec2 targetPosition{targetObject.model[3].x, targetObject.model[3].y};
  glm::vec2 delta = targetPosition - currentPosition;

  float distance = glm::length(delta);

  if (distance > MOVE_TARGET_EPSILON) {
    glm::vec2 direction = delta / distance;
    applyWizardTransformFacingDirection(object, behavior, transform, direction);
  }

  if (hasActiveAnimationEnded(object)) {
    object.activeAnimation = WizardAnimationMapping::Iddle;
    behavior.state = WizardState::Thinking;
    behavior.tick = 0;
    behavior.attacks++;
    behavior.moves = 0;
    behavior.kicks = 0;

    worldContext.wizzardAttacking.erase(targetBehavior.id);
    behavior.currentAttackingIndex = SIZE_MAX;
  }
}

FindClosestWizardInRangeReturn
findClosestWizardInRange(const Object3D &self, glm::vec3 &closestPosition,
                         float closestDistanceSquared) {
  const glm::vec3 selfPosition = glm::vec3(self.model[3]);
  bool foundWizard = false;
  size_t otherIndex = SIZE_MAX;

  for (int i = 0; i < vulkanRendererContext.objects.size(); i++) {
    const Object3D &other = vulkanRendererContext.objects[i];

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
      otherIndex = i;
    }
  }

  return FindClosestWizardInRangeReturn{.found = foundWizard,
                                        .otherIndex = otherIndex};
}

static bool wizardIsSomeoneClose(const Object3D &self) {
  glm::vec3 closestPosition{};
  return findClosestWizardInRange(self, closestPosition).found;
}

static void wizardMoveExecute(Object3D &object, WizardBehavior &behavior) {
  auto transform = modelToTransform(object.model);

  if (behavior.state != WizardState::Moving) {
    behavior.nextMovePoint = chooseMovePoint(behavior);
    behavior.state = WizardState::Moving;
    object.activeAnimation = WizardAnimationMapping::Running;
  }

  glm::vec2 currentPosition{transform.position.x, transform.position.y};
  glm::vec2 targetPosition{behavior.nextMovePoint.x, behavior.nextMovePoint.y};
  glm::vec2 delta = targetPosition - currentPosition;

  float distance = glm::length(delta);

  if (distance > MOVE_TARGET_EPSILON) {
    glm::vec2 direction = delta / distance;

    float step = glm::min(behavior.speed * timeState.deltaTime, distance);
    currentPosition += direction * step;

    transform.position.x = currentPosition.x;
    transform.position.y = currentPosition.y;

    applyWizardTransformFacingDirection(object, behavior, transform, direction);
    return;
  }

  if (wizardIsSomeoneClose(object)) {
    behavior.nextMovePoint = chooseMovePoint(behavior);
    return;
  }

  behavior.tick = 0.0f;
  behavior.state = WizardState::Thinking;
  object.activeAnimation = WizardAnimationMapping::Iddle;
  behavior.moves++;
  behavior.attacks = 0;
  behavior.kicks = 0;
}

static void wizardThinkingExecute(Object3D &object, WizardBehavior &behavior) {
  behavior.state = WizardState::Thinking;
  object.activeAnimation = WizardAnimationMapping::Iddle;
}

static void wizardKickExecute(Object3D &object, WizardBehavior &behavior) {
  if (behavior.state != WizardState::Kicking) {
    object.activeAnimation = WizardAnimationMapping::Kicking;
    const AnimationClipGpu &clip =
        object.animatedMesh->animations[object.activeAnimation];

    const int halfFrame = 45;
    object.animationTimeSeconds =
        static_cast<float>(halfFrame - clip.startFrame) /
        object.animatedMesh->fps;
    behavior.state = WizardState::Kicking;

    if (behavior.currentAttackingIndex < worldContext.wizardBehaviors.size()) {
      const WizardBehavior &targetBehavior =
          worldContext.wizardBehaviors[behavior.currentAttackingIndex];
      worldContext.wizzardAttacking[targetBehavior.id] = behavior.id;
    }
  }

  Transform transform = modelToTransform(object.model);

  applyWizardTransformFacing(object, behavior, transform,
                             behavior.kickingObject);

  // How can I get the object that this wizard is kicking?

  if (hasActiveAnimationEnded(object)) {
    object.activeAnimation = WizardAnimationMapping::Iddle;
    behavior.state = WizardState::Thinking;
    behavior.tick = 0;
    behavior.kicks++;
    behavior.moves = 0;
    behavior.attacks = 0;

    if (behavior.currentAttackingIndex < worldContext.wizardBehaviors.size()) {
      const WizardBehavior &targetBehavior =
          worldContext.wizardBehaviors[behavior.currentAttackingIndex];
      worldContext.wizzardAttacking.erase(targetBehavior.id);
    }
    behavior.currentAttackingIndex = SIZE_MAX;
  }
}

static void wizardBeingAttackedExecute(Object3D &object,
                                       WizardBehavior &behavior) {

  // Super mega hyper edge case
  // if (!worldContext.wizzardAttacking.contains(behavior.id)) {
  //   behavior.state = WizardState::Thinking;
  //   object.activeAnimation = WizardAnimationMapping::BeingAttacked;
  //   return;
  // }

  if (behavior.state == WizardState::Attacking ||
      behavior.state == WizardState::Kicking) {
    return;
  }

  if (behavior.state != WizardState::BeingAttacked) {
    behavior.state = WizardState::BeingAttacked;
    object.activeAnimation = WizardAnimationMapping::BeingAttacked;
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
  case WizardState::BeingAttacked:
    wizardBeingAttackedExecute(object, behavior);
    break;
  }
}

static bool wizardSomeoneIsSuperClose(const Object3D &self,
                                      WizardBehavior &behavior) {
  glm::vec3 closestPosition{};
  auto inRangeData =
      findClosestWizardInRange(self, closestPosition, SUPER_CLOSE_RADIUS_SQ);

  if (inRangeData.found) {
    behavior.kickingObject = closestPosition;
    if (behavior.state != WizardState::Kicking) {
      Object3D &targetObject =
          vulkanRendererContext.objects[inRangeData.otherIndex];
      behavior.currentAttackingIndex = targetObject.entityId;
    }
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

  behavior.executeBeingAttackedLogic.execute = [&object, &behavior]() {
    wizardBeingAttackedExecute(object, behavior);
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

  behavior.beingAttackedNode.conditions = [&behavior]() {
    // If i'm being attacked but I'm not currently attacking
    return worldContext.wizzardAttacking.contains(behavior.id) &&
           (behavior.state != WizardState::Attacking &&
            behavior.state != WizardState::Kicking);
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

  behavior.beingAttackedNode.yes = &behavior.executeBeingAttackedLogic;
  behavior.beingAttackedNode.no = &behavior.someoneIsSuperCloseNode;

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

  evaluateDecisionTree(&currentBehavior.beingAttackedNode);
}
