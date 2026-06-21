#pragma once

#include "../../systems/animationSystem.h"
#include "../../systems/projectileSystem.h"
#include "../../systems/resourceManagementSystem.h"
#include "../../systems/worldSystem.h"
#include "glm/fwd.hpp"
#include <cstdint>

constexpr int MAX_CONSECUTIVE_MOVES = 2;
constexpr int MAX_CONSECUTIVE_ATTACKS = 2;
constexpr int MAX_CONSECUTIVE_KICKS = 2;
constexpr float MOVE_TARGET_EPSILON = 0.000001f;

inline int findWizardShootingEffectEntity(int wizardEntity) {
  for (const WizardShootEffect &shootEffect :
       worldContext.wizardShootingEffects) {
    if (shootEffect.wizardEntity == wizardEntity) {
      return shootEffect.effectEntity;
    }
  }

  return -1;
}

inline void setWizardShootingEffectVisible(int wizardEntity, bool visible) {
  int effectEntity = findWizardShootingEffectEntity(wizardEntity);
  if (effectEntity < 0) {
    return;
  }

  Renderable &renderable = getRenderable(effectEntity);
  renderable.visible = visible;
}

inline void resetWizardShootingEffectAnimation(int wizardEntity) {
  int effectEntity = findWizardShootingEffectEntity(wizardEntity);
  if (effectEntity < 0) {
    return;
  }

  AnimationComponent &animation = getAnimation(effectEntity);
  animation.animationTimeSeconds = 0.0f;
}

inline void applyWizardTransformFacing(int entity,
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

  getTransform(entity).model = model;
}

inline void applyWizardTransformFacingDirection(int entity,
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

  getTransform(entity).model = model;
}

inline glm::vec3 chooseMovePoint(const WizardBehavior &behavior) {
  // Movement currently stays inside the play area centered at world origin.
  return randomPointInCircleXY(glm::vec3{0.0f}, behavior.moveToRadius);
}

inline glm::mat4 captureInitialRotation(int entity) {
  const TransformComponent &transform = getTransform(entity);
  glm::mat4 initialRotation{1.0f};

  initialRotation[0] =
      glm::vec4(glm::normalize(glm::vec3(transform.model[0])), 0.0f);
  initialRotation[1] =
      glm::vec4(glm::normalize(glm::vec3(transform.model[1])), 0.0f);
  initialRotation[2] =
      glm::vec4(glm::normalize(glm::vec3(transform.model[2])), 0.0f);
  initialRotation[3] = glm::vec4{0.0f, 0.0f, 0.0f, 1.0f};

  return initialRotation;
}

inline float getForwardYaw(const glm::mat4 &rotation) {
  glm::vec3 localForward{0.0f, -1.0f, 0.0f};
  glm::vec3 forward =
      glm::normalize(glm::vec3(rotation * glm::vec4(localForward, 0.0f)));

  return std::atan2(forward.y, forward.x);
}

inline void wizardAttackExecute(int entity, WizardBehavior &behavior) {
  TransformComponent &transformComponent = getTransform(entity);
  AnimationComponent &animation = getAnimation(entity);
  auto transform = modelToTransform(transformComponent.model);

  if (behavior.state != WizardState::Attacking) {
    if (worldContext.wizardBehaviors.size() < 2) {
      behavior.state = WizardState::Thinking;
      animation.activeAnimation = WizardAnimationMapping::Iddle;
      return;
    }

    animation.activeAnimation = WizardAnimationMapping::Attacking;
    animation.animationTimeSeconds = 0;
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
    behavior.currentAttackingEntity = selected.id;

    setWizardShootingEffectVisible(entity, true);
    resetWizardShootingEffectAnimation(entity);
  }

  if (behavior.currentAttackingEntity < 0 ||
      !isEntityAlive(behavior.currentAttackingEntity)) {
    behavior.state = WizardState::Thinking;
    animation.activeAnimation = WizardAnimationMapping::Iddle;
    return;
  }

  const int targetEntity = behavior.currentAttackingEntity;
  const TransformComponent &targetTransform = getTransform(targetEntity);

  // Make it look to where it is attacking.
  glm::vec2 currentPosition{transform.position.x, transform.position.y};
  glm::vec2 targetPosition{targetTransform.model[3].x,
                           targetTransform.model[3].y};
  glm::vec2 delta = targetPosition - currentPosition;

  float distance = glm::length(delta);

  if (distance > MOVE_TARGET_EPSILON) {
    glm::vec2 direction = delta / distance;
    applyWizardTransformFacingDirection(entity, behavior, transform, direction);
  }

  if (hasActiveAnimationEnded(entity)) {
    animation.activeAnimation = WizardAnimationMapping::Iddle;
    behavior.state = WizardState::Thinking;
    behavior.tick = 0;
    behavior.attacks++;
    behavior.moves = 0;
    behavior.kicks = 0;

    spawnWizardProjectile(entity);
    setWizardShootingEffectVisible(entity, false);
    resetWizardShootingEffectAnimation(entity);

    worldContext.wizzardAttacking.erase(targetEntity);
    behavior.currentAttackingEntity = -1;
  }
}

inline bool wizardIsSomeoneClose(int entity) {
  glm::vec3 closestPosition{};
  return findClosestWizardInRange(entity, closestPosition).found;
}

inline void wizardMoveExecute(int entity, WizardBehavior &behavior) {
  AnimationComponent &animation = getAnimation(entity);
  auto transform = modelToTransform(getTransform(entity).model);
  setWizardShootingEffectVisible(entity, false);

  if (behavior.state != WizardState::Moving) {
    behavior.nextMovePoint = chooseMovePoint(behavior);
    behavior.state = WizardState::Moving;
    animation.activeAnimation = WizardAnimationMapping::Running;
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

    applyWizardTransformFacingDirection(entity, behavior, transform, direction);
    return;
  }

  if (wizardIsSomeoneClose(entity)) {
    behavior.nextMovePoint = chooseMovePoint(behavior);
    return;
  }

  behavior.tick = 0.0f;
  behavior.state = WizardState::Thinking;
  animation.activeAnimation = WizardAnimationMapping::Iddle;
  behavior.moves++;
  behavior.attacks = 0;
  behavior.kicks = 0;
}

inline void wizardThinkingExecute(int entity, WizardBehavior &behavior) {
  AnimationComponent &animation = getAnimation(entity);
  behavior.state = WizardState::Thinking;
  animation.activeAnimation = WizardAnimationMapping::Iddle;
  setWizardShootingEffectVisible(entity, false);
}

inline void wizardKickExecute(int entity, WizardBehavior &behavior,
                              Renderable &renderable) {
  AnimationComponent &animation = getAnimation(entity);
  setWizardShootingEffectVisible(entity, false);

  if (behavior.state != WizardState::Kicking) {
    animation.activeAnimation = WizardAnimationMapping::Kicking;
    const AnimationClipGpu &clip =
        renderable.animatedMesh->animations[animation.activeAnimation];

    const int halfFrame = 45;
    animation.animationTimeSeconds =
        static_cast<float>(halfFrame - clip.startFrame) /
        renderable.animatedMesh->fps;
    behavior.state = WizardState::Kicking;

    if (behavior.currentAttackingEntity >= 0) {
      worldContext.wizzardAttacking[behavior.currentAttackingEntity] =
          behavior.id;
    }
  }

  Transform transform = modelToTransform(getTransform(entity).model);

  applyWizardTransformFacing(entity, behavior, transform,
                             behavior.kickingObject);

  // How can I get the object that this wizard is kicking?

  if (hasActiveAnimationEnded(entity)) {
    animation.activeAnimation = WizardAnimationMapping::Iddle;
    behavior.state = WizardState::Thinking;
    behavior.tick = 0;
    behavior.kicks++;
    behavior.moves = 0;
    behavior.attacks = 0;

    if (behavior.currentAttackingEntity >= 0) {
      worldContext.wizzardAttacking.erase(behavior.currentAttackingEntity);
    }
    behavior.currentAttackingEntity = -1;
  }
}

inline void wizardBeingAttackedExecute(int entity, WizardBehavior &behavior) {

  // Super mega hyper edge case
  // if (!worldContext.wizzardAttacking.contains(behavior.id)) {
  //   behavior.state = WizardState::Thinking;
  //   getAnimation(entity).activeAnimation =
  //   WizardAnimationMapping::BeingAttacked; return;
  // }

  if (behavior.state == WizardState::Attacking ||
      behavior.state == WizardState::Kicking) {
    return;
  }

  if (behavior.state != WizardState::BeingAttacked) {
    behavior.state = WizardState::BeingAttacked;
    getAnimation(entity).activeAnimation =
        WizardAnimationMapping::BeingAttacked;
  }
}

inline void continueCurrentAction(int entity, WizardBehavior &behavior,
                                  Renderable &renderable) {
  switch (behavior.state) {
  case WizardState::Moving:
    wizardMoveExecute(entity, behavior);
    break;
  case WizardState::Attacking:
    wizardAttackExecute(entity, behavior);
    break;
  case WizardState::Thinking:
    wizardThinkingExecute(entity, behavior);
    break;
  case WizardState::Kicking:
    wizardKickExecute(entity, behavior, renderable);
    break;
  case WizardState::BeingAttacked:
    wizardBeingAttackedExecute(entity, behavior);
    break;
  }
}

inline bool wizardSomeoneIsSuperClose(int entity, WizardBehavior &behavior) {
  glm::vec3 closestPosition{};
  auto inRangeData =
      findClosestWizardInRange(entity, closestPosition, SUPER_CLOSE_RADIUS_SQ);

  if (inRangeData.found) {
    behavior.kickingObject = closestPosition;
    if (behavior.state != WizardState::Kicking) {
      behavior.currentAttackingEntity = inRangeData.otherEntity;
    }
    return true;
  }

  return false;
}

inline void updateWizardShootingEffect(WizardShootEffect &shootEffect) {
  const TransformComponent &wizard = getTransform(shootEffect.wizardEntity);
  TransformComponent &effect = getTransform(shootEffect.effectEntity);

  effect.baseModel = wizard.model;
}
