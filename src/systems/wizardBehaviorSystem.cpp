#include "wizardBehaviorSystem.h"
#include "../behaviors/decisionTree.h"
#include "../engine/blender/importer.h"
#include "../engine/vulkanRenderer.h"
#include "../utils/generators.h"
#include "../utils/math.h"
#include "../utils/time.h"
#include "animationSystem.h"
#include "glm/fwd.hpp"
#include "glm/geometric.hpp"
#include "projectileSystem.h"
#include "resourceManagementSystem.h"
#include "unordered_map"
#include <cmath>

constexpr float NEXT_POS_RADIUS = 2 * 2;
constexpr float MOVING_AREA = 20.0F;
constexpr float CLOSE_RADIUS = 3.0f;

static TransformAnimatedMesh shootEffectMesh{};
static BlenderTransformModel shootModel{};

DecisionStatus choosNewPositionLogic(int entity) {
  // We may want to change the center of this at some point.
  glm::vec2 randomPoint = randomPointInCircle(0, 0, MOVING_AREA);

  WizardBehaviorComponent &behavior = getWizardBehavior(entity);
  behavior.attackCounter = 0;
  behavior.state = WizzardState::ChoosingNewPosition;
  behavior.nextPos = randomPoint;

  AnimationComponent &animation = getAnimation(entity);
  animation.activeAnimation = WizardAnimationMapping::Iddle;

  return DecisionStatus::Done;
}

static bool hasReservedPosition(const WizardBehaviorComponent &wizard) {
  return wizard.state == WizzardState::ChoosingNewPosition ||
         wizard.state == WizzardState::MovingToPosition;
}

bool positionIsFreeLogic(int entity) {
  WizardBehaviorComponent &thisWizard = getWizardBehavior(entity);

  for (WizardBehaviorComponent &wizard : resources.wizardBehaviors) {
    // So we don't take thisWizard into account
    if (wizard.entity == entity) {
      continue;
    }

    TransformComponent &t = getTransform(wizard.entity);
    Transform wt = modelToTransform(t.model);

    float distanceToWizardPosition = getDistanceSqr(
        thisWizard.nextPos, glm::vec2{wt.position.x, wt.position.y});

    bool wizardReservedNearbyPosition =
        hasReservedPosition(wizard) &&
        getDistanceSqr(thisWizard.nextPos, wizard.nextPos) <= NEXT_POS_RADIUS;

    if (distanceToWizardPosition <= NEXT_POS_RADIUS ||
        wizardReservedNearbyPosition) {
      return false;
    }
  }

  return true;
}

DecisionStatus moveToPositionLogic(int entity) {
  WizardBehaviorComponent &thisWizard = getWizardBehavior(entity);
  thisWizard.state = WizzardState::MovingToPosition;

  AnimationComponent &animation = getAnimation(entity);
  animation.activeAnimation = WizardAnimationMapping::Running;
  animation.animationPlaySpeed = 1.8;

  TransformComponent &tc = getTransform(entity);
  const Transform &transform = modelToTransform(tc.model);
  glm::vec2 position = glm::vec2{transform.position.x, transform.position.y};

  glm::vec2 delta = thisWizard.nextPos - position;
  float distanceSqr = getDistanceSqr(position, thisWizard.nextPos);

  if (distanceSqr < 0.001f * 0.001f) {
    return DecisionStatus::Done;
  }

  float distance = glm::sqrt(distanceSqr);
  glm::vec2 direction = delta / distance;

  moveTowardsDir(tc.model, thisWizard.speed, direction, distance,
                 timeState.deltaTime);

  return DecisionStatus::Done;
}

bool arrivedNewPositionLogic(int entity) {
  WizardBehaviorComponent &thisWizard = getWizardBehavior(entity);
  TransformComponent &tc = getTransform(entity);
  const Transform &t = modelToTransform(tc.model);
  glm::vec2 position = glm::vec2{t.position};

  float distance = getDistanceSqr(position, thisWizard.nextPos);

  if (distance < SUPER_CLOSE_RADIUS_SQ) {
    thisWizard.state = WizzardState::None;
    return true;
  }

  return false;
}

DecisionStatus chooseNextAttackEntityLogic(int entity) {
  WizardBehaviorComponent &wizard = getWizardBehavior(entity);

  int nextWizardToAttackIndex = getRandom(resources.wizardBehaviors.size());
  WizardBehaviorComponent nextWizardToAttack =
      resources.wizardBehaviors[nextWizardToAttackIndex];

  while (nextWizardToAttack.entity == wizard.entity) {
    nextWizardToAttackIndex = getRandom(resources.wizardBehaviors.size());
    nextWizardToAttack = resources.wizardBehaviors[nextWizardToAttackIndex];
  }

  wizard.nextAttackEntity = nextWizardToAttack.entity;

  return DecisionStatus::Done;
}

DecisionStatus attackLogic(int entity) {
  WizardBehaviorComponent &wizard = getWizardBehavior(entity);
  TransformComponent thisWizardTC = getTransform(entity);

  if (wizard.state != WizzardState::Attacking) {
    AnimationComponent &animation = getAnimation(entity);
    animation.activeAnimation = WizardAnimationMapping::Attacking;
    animation.animationTimeSeconds = 0;
    wizard.state = WizzardState::Attacking;
    wizard.attackCounter++;

    int shootEffectEntity = createEntity();

    wizard.shootEffecEntity = shootEffectEntity;

    Renderable &r = addRenderable(shootEffectEntity);
    r.renderKind = ObjectRenderKind::TransformAnimated;
    r.transformAnimatedMesh = &shootEffectMesh;
    r.visible = true;

    TransformComponent &transform = addTransform(shootEffectEntity);
    transform.baseModel = thisWizardTC.model;
    transform.model = thisWizardTC.model;

    AnimationComponent &shootEffectAnimation = addAnimation(shootEffectEntity);

    shootEffectAnimation.activeAnimation = 0;
    shootEffectAnimation.animationTimeSeconds = 0.0f;
    shootEffectAnimation.animationPlaySpeed = 1.8f;
  }

  TransformComponent &wtc = getTransform(entity);
  const Transform &wt = modelToTransform(wtc.model);
  glm::vec2 wtPos = glm::vec2{wt.position};

  wizard.attackTime = 0;

  WizardBehaviorComponent &nextAttack =
      getWizardBehavior(wizard.nextAttackEntity);
  TransformComponent &natc = getTransform(nextAttack.entity);
  const Transform &nat = modelToTransform(natc.model);
  glm::vec2 natPos = glm::vec2{nat.position};

  glm::vec2 nextAttackDir = glm::normalize(natPos - wtPos);

  faceTowardsDir(wtc.model, nextAttackDir);

  if (hasActiveAnimationEnded(wizard.shootEffecEntity)) {
    wizard.state = WizzardState::None;

    spawnWizardProjectile(entity);
    destroyEntity(wizard.shootEffecEntity);

    return DecisionStatus::Done;
  }

  return DecisionStatus::Running;
}

bool isSomeoneCloseLogic(int entity) {
  TransformComponent &wtc = getTransform(entity);
  const Transform &wt = modelToTransform(wtc.model);
  glm::vec2 wizardPos = glm::vec2{wt.position};

  for (WizardBehaviorComponent &checkWizard : resources.wizardBehaviors) {
    if (entity == checkWizard.entity) {
      continue;
    }

    TransformComponent &cwtc = getTransform(checkWizard.entity);
    const Transform &cwt = modelToTransform(cwtc.model);
    glm::vec2 checkWizardPos = glm::vec2{cwt.position};

    float distance = getDistanceSqr(wizardPos, checkWizardPos);

    constexpr float closeRadius = CLOSE_RADIUS;
    constexpr float closeRadiusSqr = closeRadius * closeRadius;

    if (distance <= closeRadiusSqr) {
      return true;
    }
  }

  return false;
}

bool waitedForNextAttackLogic(int entity) {
  WizardBehaviorComponent &wizard = getWizardBehavior(entity);

  if (wizard.state != WizzardState::Waiting) {
    wizard.state = WizzardState::Waiting;

    AnimationComponent &animation = getAnimation(entity);
    animation.activeAnimation = WizardAnimationMapping::Iddle;
    animation.animationTimeSeconds = 0;
  }

  return wizard.attackTime >= 0.5;
}

struct WizardDecisionTree {
  int entity = -1;

  DecisionTreeRunner runner;

  DecisionNode incrementAttackTimer{};
  DecisionNode attacked2Times{};
  DecisionNode waitedForNextAttack{};
  DecisionNode isSomeoneClose{};
  DecisionNode attack{};
  DecisionNode chooseNextAttackEntity{};
  DecisionNode arrivedNewPosition{};
  DecisionNode moveToPosition{};
  DecisionNode chooseNewPosition{};
  DecisionNode positionIsFree{};

  void init(int wizardEntity) {
    entity = wizardEntity;
    chooseNewPosition.next = &positionIsFree;
    chooseNewPosition.execute = [wizardEntity]() {
      return choosNewPositionLogic(wizardEntity);
    };

    positionIsFree.conditions = [wizardEntity]() {
      return positionIsFreeLogic(wizardEntity);
    };
    positionIsFree.no = &chooseNewPosition;
    positionIsFree.yes = &moveToPosition;

    moveToPosition.execute = [wizardEntity]() {
      return moveToPositionLogic(wizardEntity);
    };
    moveToPosition.next = &arrivedNewPosition;

    arrivedNewPosition.conditions = [wizardEntity]() {
      return arrivedNewPositionLogic(wizardEntity);
    };
    arrivedNewPosition.no = &moveToPosition;
    arrivedNewPosition.yes = &chooseNextAttackEntity;

    chooseNextAttackEntity.execute = [wizardEntity]() {
      return chooseNextAttackEntityLogic(wizardEntity);
    };
    chooseNextAttackEntity.next = &attack;

    attack.execute = [wizardEntity]() { return attackLogic(wizardEntity); };
    attack.next = &isSomeoneClose;

    isSomeoneClose.conditions = [wizardEntity]() {
      return isSomeoneCloseLogic(wizardEntity);
    };
    isSomeoneClose.no = &waitedForNextAttack;
    isSomeoneClose.yes = &chooseNewPosition;

    waitedForNextAttack.conditions = [wizardEntity]() {
      return waitedForNextAttackLogic(wizardEntity);
    };
    waitedForNextAttack.no = &incrementAttackTimer;
    waitedForNextAttack.yes = &attacked2Times;

    incrementAttackTimer.execute = [wizardEntity]() {
      WizardBehaviorComponent &wizard = getWizardBehavior(wizardEntity);
      wizard.attackTime += timeState.deltaTime;

      return DecisionStatus::Done;
    };
    incrementAttackTimer.next = &waitedForNextAttack;

    attacked2Times.conditions = [wizardEntity]() {
      WizardBehaviorComponent &wizard = getWizardBehavior(wizardEntity);
      return wizard.attackCounter >= 2;
    };
    attacked2Times.no = &chooseNextAttackEntity;
    attacked2Times.yes = &chooseNewPosition;

    runner.reset(&chooseNewPosition);
  }

  void tick() { runner.tick(); }
};

// Wizard entity to Wizard Decision Tree
static std::unordered_map<int, WizardDecisionTree> decisionTrees;

static glm::vec4 getDebugColor(int index) {
  constexpr float goldenRatioConjugate = 0.61803398875f;
  float hue = std::fmod(static_cast<float>(index) * goldenRatioConjugate, 1.0f);
  constexpr float saturation = 0.75f;
  constexpr float value = 1.0f;

  float hueSector = hue * 6.0f;
  int sector = static_cast<int>(hueSector);
  float fraction = hueSector - static_cast<float>(sector);

  float p = value * (1.0f - saturation);
  float q = value * (1.0f - saturation * fraction);
  float t = value * (1.0f - saturation * (1.0f - fraction));

  switch (sector % 6) {
  case 0:
    return glm::vec4{value, t, p, 1.0f};
  case 1:
    return glm::vec4{q, value, p, 1.0f};
  case 2:
    return glm::vec4{p, value, t, 1.0f};
  case 3:
    return glm::vec4{p, q, value, 1.0f};
  case 4:
    return glm::vec4{t, p, value, 1.0f};
  default:
    return glm::vec4{value, p, q, 1.0f};
  }
}

static void initWizard(WizardBehaviorComponent &wizardBehavior) {
  WizardDecisionTree &decisionTree = decisionTrees[wizardBehavior.entity];
  decisionTree = WizardDecisionTree{};
  decisionTree.init(wizardBehavior.entity);
}

void initWizardBehaviors() {
  int debugColorIndex = 0;

  shootModel = loadTransformModel("assets/Wizard_Shooting_Effect_1.3d");
  shootEffectMesh = generateTransformAnimatedMesh(shootModel);

  for (WizardBehaviorComponent &wizardBehavior : resources.wizardBehaviors) {
    wizardBehavior.debugColor = getDebugColor(debugColorIndex++);
    initWizard(wizardBehavior);
  }
}

void updateWizardBehaviors() {
  addDebugDiskXY(glm::vec3{0.0f, 0.0f, 1.0f}, MOVING_AREA,
                 glm::vec4{0, 0, 1, 1});

  for (WizardBehaviorComponent &wizardBehavior : resources.wizardBehaviors) {
    WizardDecisionTree &decisionTree = decisionTrees[wizardBehavior.entity];
    TransformComponent &wtc = getTransform(wizardBehavior.entity);
    const Transform &wt = modelToTransform(wtc.model);

    addDebugDiskXY(wt.position, CLOSE_RADIUS, wizardBehavior.debugColor);
    addDebugCube(glm::vec3{wizardBehavior.nextPos.x + 0.5f,
                           wizardBehavior.nextPos.y + 0.5f, 1.0f},
                 0.5, wizardBehavior.debugColor);
    addDebugDiskXY(
        glm::vec3{wizardBehavior.nextPos.x, wizardBehavior.nextPos.y, 1.0f},
        NEXT_POS_RADIUS, wizardBehavior.debugColor);

    decisionTree.tick();

    if (wizardBehavior.shootEffecEntity != -1 &&
        isEntityAlive(wizardBehavior.shootEffecEntity)) {
      TransformComponent &effectTransform =
          getTransform(wizardBehavior.shootEffecEntity);

      effectTransform.baseModel = wtc.model;
    }
  }
}
