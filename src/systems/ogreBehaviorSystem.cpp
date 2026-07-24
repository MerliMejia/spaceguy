#include "ogreBehaviorSystem.h"
#include "../behaviors/decisionTree.h"
#include "../engine/vulkanRenderer.h"
#include "../utils/behaviors.h"
#include "../utils/math.h"
#include "animationSystem.h"
#include "glm/fwd.hpp"
#include "glm/geometric.hpp"
#include "resourceManagementSystem.h"
#include "unordered_map"

constexpr int HOW_CLOSE = 16;
constexpr int SQUARES_TO_CHECK = 8;
constexpr float WAIT_TIME_INIT_MOVE = 1;
constexpr float MOVE_SPEED = 10;
constexpr float IS_CLOSE_ENOUGH_TRESHOLD = 3;

static DecisionStatus startAttackModeLogic(int entity) {
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

DecisionStatus attack1Logic(int entity) {
  OgreBehaviorComponent &behavior = getOgreBehaviorComponent(entity);
  AnimationComponent &animation = getAnimation(entity);

  if (behavior.state != OgreState::Attacking) {
    behavior.state = OgreState::Attacking;
    animation.activeAnimation = OgreAnimationMapping::OgreAttack1;
    animation.animationTimeSeconds = 0;
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

struct OgreDecisionTree {
  int entity = -1;
  DecisionTreeRunner decisionRunner;

  // questions
  DecisionNode checkIfAlreadyInAttackMode{};
  DecisionNode checkIfSomeoneClose{};
  DecisionNode checkIfWaitedForInitAttack{};
  DecisionNode checkIfCloseEnoughToAttackTarget{};
  DecisionNode checkIfAttackEntityIsValid{};

  // executions
  DecisionNode iddleMode{};
  DecisionNode startAttackMode{};
  DecisionNode attackMode{};
  DecisionNode moveToAttack{};
  DecisionNode attack1{};

  void init(int ogreEntity) {

    checkIfAlreadyInAttackMode.conditions = [ogreEntity]() {
      return isAlreadyInAttackMode(ogreEntity);
    };

    checkIfAlreadyInAttackMode.yes = &attackMode;
    checkIfAlreadyInAttackMode.no = &checkIfSomeoneClose;

    checkIfSomeoneClose.conditions = [ogreEntity]() {
      return checkIfSomeoneCloseLogic(ogreEntity);
    };

    checkIfSomeoneClose.yes = &startAttackMode;
    checkIfSomeoneClose.no = &iddleMode;

    iddleMode.execute = [ogreEntity]() { return iddleModeLogic(ogreEntity); };

    startAttackMode.execute = [ogreEntity]() {
      return startAttackModeLogic(ogreEntity);
    };

    startAttackMode.next = &attackMode;

    attackMode.execute = [ogreEntity]() { return attackModeLogic(ogreEntity); };
    attackMode.next = &checkIfWaitedForInitAttack;

    checkIfWaitedForInitAttack.conditions = [ogreEntity]() {
      return waitedForInitAttackLogic(ogreEntity);
    };

    checkIfWaitedForInitAttack.no = &attackMode;
    checkIfWaitedForInitAttack.yes = &moveToAttack;

    moveToAttack.execute = [ogreEntity]() {
      return moveToAttackLogic(ogreEntity);
    };

    moveToAttack.next = &checkIfAttackEntityIsValid;

    checkIfAttackEntityIsValid.conditions = [ogreEntity]() {
      return isAttackEntityIsValid(ogreEntity);
    };
    checkIfAttackEntityIsValid.no = &checkIfAlreadyInAttackMode;
    checkIfAttackEntityIsValid.yes = &checkIfCloseEnoughToAttackTarget;

    checkIfCloseEnoughToAttackTarget.conditions = [ogreEntity]() {
      return isCloseEnoughToAttackTarget(ogreEntity);
    };

    checkIfCloseEnoughToAttackTarget.no = &moveToAttack;
    checkIfCloseEnoughToAttackTarget.yes = &attack1;

    attack1.execute = [ogreEntity]() { return attack1Logic(ogreEntity); };
    attack1.next = &attackMode;

    decisionRunner.reset(&checkIfAlreadyInAttackMode);
  }
  void tick() { decisionRunner.tick(); }
};

std::unordered_map<int, OgreDecisionTree> decisionTrees;

void initOgres() {
  for (OgreBehaviorComponent &ogreBehavior : resources.ogreBehaviors) {
    OgreDecisionTree &decisionTree = decisionTrees[ogreBehavior.entity];

    decisionTree = OgreDecisionTree{};

    decisionTree.init(ogreBehavior.entity);
  }
}

void updateOgres() {
  for (OgreBehaviorComponent &ogreBehavior : resources.ogreBehaviors) {
    OgreDecisionTree &decisionTree = decisionTrees[ogreBehavior.entity];

    if (vulkanRendererContext.isDebug) {
      TransformComponent tc = getTransform(ogreBehavior.entity);
      Transform t = modelToTransform(tc.model);

      addDebugDiskXY(glm::vec3{t.position.x, t.position.y, 2}, HOW_CLOSE,
                     glm::vec4{1, 1, 1, 1});
    }

    decisionTree.decisionRunner.tick();
  }
}
