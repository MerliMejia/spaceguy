#include "ogre.h"
#include "../../systems/animationSystem.h"
#include "../../systems/resourceManagementSystem.h"
#include "../behaviors.h"
#include "../math.h"

constexpr int SQUARES_TO_CHECK = 8;
constexpr float WAIT_TIME_INIT_MOVE = 1;
constexpr float MOVE_SPEED = 10;
constexpr float IS_CLOSE_ENOUGH_TRESHOLD = 3;

DecisionStatus startAttackModeLogic(int entity) {
  TransformComponent &tc = getTransform(entity);
  Transform transform = modelToTransform(tc.model);
  OgreBehaviorComponent &behavior = getOgreBehaviorComponent(entity);

  AnimationComponent &animation = getAnimation(entity);

  if (behavior.state != OgreState::InitiatingAttackMode) {
    behavior.state = OgreState::InitiatingAttackMode;
    animation.activeAnimation = OgreAnimationMapping::OgreInitAttackMode;
    animation.animationTimeSeconds = 0;
  }

  if (!isEntityAlive(behavior.attackingEntity)) {
    return DecisionStatus::Done;
  }

  TransformComponent &attackTc = getTransform(behavior.attackingEntity);
  Transform attackTransform = modelToTransform(attackTc.model);

  glm::vec2 targetDir = attackTransform.position - transform.position;

  faceTowardsDir(tc.model, glm::normalize(targetDir));

  if (hasActiveAnimationEnded(entity)) {
    return DecisionStatus::Done;
  }

  return DecisionStatus::Running;
}

bool isAlreadyInAttackMode(int entity) {
  OgreBehaviorComponent &behavior = getOgreBehaviorComponent(entity);

  if (behavior.attackingEntity != -1) {
    if (isEntityAlive(behavior.attackingEntity)) {
      return true;
    } else {
      behavior.attackingEntity = -1;
      return false;
    }
  }

  return false;
}

DecisionStatus iddleModeLogic(int entiy) {

  OgreBehaviorComponent &behavior = getOgreBehaviorComponent(entiy);

  if (behavior.state != OgreState::Iddle) {
    AnimationComponent &animation = getAnimation(entiy);
    animation.activeAnimation = OgreAnimationMapping::OgreIddle;
    animation.animationTimeSeconds = 0;
    behavior.state = OgreState::Iddle;
  }

  return DecisionStatus::Done;
}

bool checkIfSomeoneCloseLogic(int entity) {
  auto data = BehaviorUtil::isSomeoneCloseLogic(
      entity, BehaviorUtil::CheckType::Wizards, SQUARES_TO_CHECK, HOW_CLOSE);

  if (data.isSomeoneClose) {
    OgreBehaviorComponent &behavior = getOgreBehaviorComponent(entity);
    behavior.attackingEntity = data.closeEntity;
  }
  return data.isSomeoneClose;
}

DecisionStatus attackModeLogic(int entity) {

  OgreBehaviorComponent &behavior = getOgreBehaviorComponent(entity);
  AnimationComponent &animation = getAnimation(entity);
  TransformComponent &tc = getTransform(entity);
  Transform transform = modelToTransform(tc.model);

  if (behavior.state != OgreState::InAttackMode) {
    behavior.state = OgreState::InAttackMode;
    animation.activeAnimation = OgreAnimationMapping::OgreAttackMode;
    animation.animationTimeSeconds = 0;
  }

  if (!isEntityAlive(behavior.attackingEntity)) {
    return DecisionStatus::Done;
  }

  TransformComponent &attackTc = getTransform(behavior.attackingEntity);
  Transform attackTransform = modelToTransform(attackTc.model);

  glm::vec2 targetDir = attackTransform.position - transform.position;

  faceTowardsDir(tc.model, glm::normalize(targetDir));

  return DecisionStatus::Done;
}

bool waitedForInitAttackLogic(int entity) {
  OgreBehaviorComponent &behavior = getOgreBehaviorComponent(entity);
  behavior.initAttackTime += timeState.deltaTime;

  return behavior.initAttackTime >= WAIT_TIME_INIT_MOVE;
}

DecisionStatus moveToAttackLogic(int entity) {
  OgreBehaviorComponent &behavior = getOgreBehaviorComponent(entity);
  AnimationComponent &animation = getAnimation(entity);
  TransformComponent &tc = getTransform(entity);
  Transform t = modelToTransform(tc.model);

  if (behavior.state != OgreState::MovingToAttack) {
    behavior.state = OgreState::MovingToAttack;
    animation.activeAnimation = OgreAnimationMapping::OgreRunning;
    animation.animationTimeSeconds = 0;
  }

  if (!isEntityAlive(behavior.attackingEntity)) {
    return DecisionStatus::Done;
  }

  TransformComponent &attackTc = getTransform(behavior.attackingEntity);
  Transform attackT = modelToTransform(attackTc.model);

  glm::vec2 delta = attackT.position - t.position;
  glm::vec2 dir = glm::normalize(delta);

  float distance = getDistanceSqr(t.position, attackT.position);

  moveTowardsDir(tc.model, MOVE_SPEED, dir, distance, timeState.deltaTime);

  return DecisionStatus::Done;
}

bool isCloseEnoughToAttackTarget(int entity) {
  OgreBehaviorComponent &behavior = getOgreBehaviorComponent(entity);
  TransformComponent &tc = getTransform(entity);
  Transform t = modelToTransform(tc.model);

  if (!isEntityAlive(behavior.attackingEntity)) {
    return false;
  }

  TransformComponent &attackTc = getTransform(behavior.attackingEntity);
  Transform attackT = modelToTransform(attackTc.model);
  return isCloseBox(t.position, attackT.position, IS_CLOSE_ENOUGH_TRESHOLD);

  return false;
}

DecisionStatus flyingAttackLogic(int entity) {
  OgreBehaviorComponent &behavior = getOgreBehaviorComponent(entity);
  AnimationComponent &animation = getAnimation(entity);

  if (behavior.state != OgreState::Attacking) {
    behavior.state = OgreState::Attacking;
    animation.activeAnimation = OgreAnimationMapping::OgreFlyingAttack;
    animation.animationTimeSeconds = 0;
  }

  if (hasActiveAnimationReachedFrame(entity, 35)) {
    // For now let's just kill the target if it's a wizard. Will definetly come
    // back for an actual health implementation.
    if (WizardBehaviorComponent *wizard =
            tryGetWizardBehavior(behavior.attackingEntity)) {
      destroyEntity(behavior.attackingEntity);
      behavior.attackingEntity = -1;
    }
  }

  if (hasActiveAnimationEnded(entity)) {
    behavior.initAttackTime = 0;
    return DecisionStatus::Done;
  }

  return DecisionStatus::Running;
}

bool isAttackEntityIsValid(int entity) {
  OgreBehaviorComponent &behavior = getOgreBehaviorComponent(entity);

  return behavior.attackingEntity != -1 &&
         isEntityAlive(behavior.attackingEntity);
}
