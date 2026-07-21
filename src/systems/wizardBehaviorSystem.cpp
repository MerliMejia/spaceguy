#include "wizardBehaviorSystem.h"
#include "../behaviors/decisionTree.h"
#include "../engine/blender/importer.h"
#include "../engine/vulkanRenderer.h"
#include "../utils/generators.h"
#include "../utils/math.h"
#include "../utils/time.h"
#include "animationSystem.h"
#include "glm/common.hpp"
#include "glm/fwd.hpp"
#include "glm/geometric.hpp"
#include "projectileSystem.h"
#include "resourceManagementSystem.h"
#include "spacialGridHashSystem.h"
#include "unordered_map"
#include <cmath>

// Almost the size of floor right now.
constexpr float MOVING_AREA = 30.0F;
constexpr float NEXT_POS_RADIUS = 1;
constexpr float CLOSE_RADIUS = 1;
constexpr float PROJECTILE_LOOKAHEAD = 14.0f;
constexpr float PROJECTILE_DANGER_RADIUS = 1.35f;
constexpr float WIZARD_AVOID_RADIUS = 2.0f;
constexpr float EVADE_DISTANCE = 4.0f;
constexpr float ATTACK_LIGHT_INITIAL_INTENSITY = 0.0f;
constexpr float ATTACK_LIGHT_MAX_INTENSITY = 0.5f;
constexpr float ATTACK_LIGHT_GROWTH_PER_SECOND = 0.1f;
constexpr float MIN_MOVE_STAMINA = 0.08f;
constexpr float WIZARD_PROJECTILE_SPEED = 20.0f;
constexpr float WIZARD_RUN_ANIMATION_SPEED = 1.0f;
constexpr float AIM_VELOCITY_SMOOTHING = 0.25f;

static TransformAnimatedMesh shootEffectMesh{};
static BlenderTransformModel shootModel{};

static void recoverStamina(WizardBehaviorComponent &wizard) {
  wizard.stamina = glm::min(wizard.maxStamina,
                            wizard.stamina + wizard.staminaRecoverPerSecond *
                                                 timeState.deltaTime);
}

static bool spendStamina(WizardBehaviorComponent &wizard, float amount) {
  if (wizard.stamina < amount) {
    return false;
  }

  wizard.stamina -= amount;
  return true;
}

static glm::vec2 clampToMovingArea(const glm::vec2 &position) {
  float distanceSqr = getDistanceSqr(glm::vec2{0.0f}, position);

  if (distanceSqr <= MOVING_AREA * MOVING_AREA) {
    return position;
  }

  return glm::normalize(position) * MOVING_AREA;
}

static glm::vec2 predictAimPoint(glm::vec2 shooterPos, glm::vec2 targetPos,
                                 glm::vec2 targetVelocity) {
  float targetSpeed = glm::length(targetVelocity);

  if (targetSpeed <= 0.001f) {
    return targetPos;
  }

  glm::vec2 targetMoveDirection = targetVelocity / targetSpeed;
  float distanceToTargetSqr = getDistanceSqr(shooterPos, targetPos);
  float projectileTravelTime =
      glm::sqrt(distanceToTargetSqr) / WIZARD_PROJECTILE_SPEED;

  return targetPos + targetMoveDirection * targetSpeed * projectileTravelTime;
}

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

bool positionIsFreeLogic(int entity) {
  WizardBehaviorComponent &thisWizard = getWizardBehavior(entity);

  CellCoord cell = worldToCell(thisWizard.nextPos, spacialGridContext.cellWidth,
                               spacialGridContext.cellHeight);

  bool isFree = true;

  executeOnNearbyCells(cell, [entity, thisWizard, &isFree](int checkEntity) {
    WizardBehaviorComponent *wizard = tryGetWizardBehavior(checkEntity);
    OgreBehaviorComponent *ogre = nullptr;

    if (wizard == nullptr) {
      ogre = tryGetOgreBehaviorComponent(checkEntity);

      if (ogre == nullptr) {
        return ExecuteOnNearbyCellsStatus::Running;
      }
    }

    // So we don't take thisWizard into account
    if (wizard != nullptr && wizard->entity == entity) {
      return ExecuteOnNearbyCellsStatus::Running;
    }

    TransformComponent &t = wizard != nullptr ? getTransform(wizard->entity)
                                              : getTransform(ogre->entity);
    Transform wt = modelToTransform(t.model);

    float distanceToWizardPosition = getDistanceSqr(
        thisWizard.nextPos, glm::vec2{wt.position.x, wt.position.y});

    if (distanceToWizardPosition <= NEXT_POS_RADIUS * NEXT_POS_RADIUS) {
      isFree = false;
      return ExecuteOnNearbyCellsStatus::Done;
    }

    return ExecuteOnNearbyCellsStatus::Running;
  });

  return isFree;
}

DecisionStatus moveToPositionLogic(int entity) {
  WizardBehaviorComponent &thisWizard = getWizardBehavior(entity);
  thisWizard.state = WizzardState::MovingToPosition;

  AnimationComponent &animation = getAnimation(entity);
  animation.activeAnimation = WizardAnimationMapping::Running;
  animation.animationPlaySpeed = WIZARD_RUN_ANIMATION_SPEED;

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

  if (!spendStamina(thisWizard,
                    thisWizard.moveStaminaPerSecond * timeState.deltaTime)) {
    thisWizard.state = WizzardState::Recovering;
    animation.activeAnimation =
        WizardAnimationMapping::Iddle; // probably need another animation for
                                       // when recovering...
    animation.animationPlaySpeed = 1.0f;

    return DecisionStatus::Done;
  }

  moveTowardsDir(tc.model, thisWizard.speed, direction, distance,
                 timeState.deltaTime);

  return DecisionStatus::Done;
}

bool hasMoveStaminaLogic(int entity) {
  WizardBehaviorComponent &wizard = getWizardBehavior(entity);
  return wizard.stamina > MIN_MOVE_STAMINA;
}

DecisionStatus recoverStaminaLogic(int entity) {
  WizardBehaviorComponent &wizard = getWizardBehavior(entity);
  wizard.state = WizzardState::Recovering;

  AnimationComponent &animation = getAnimation(entity);
  animation.activeAnimation = WizardAnimationMapping::Iddle;
  animation.animationPlaySpeed = 1.0f;

  recoverStamina(wizard);

  return wizard.stamina >= wizard.maxStamina ? DecisionStatus::Done
                                             : DecisionStatus::Running;
}

bool hasAvoidanceThreatLogic(int entity) {
  WizardBehaviorComponent &wizard = getWizardBehavior(entity);

  TransformComponent &wtc = getTransform(entity);
  const Transform &wt = modelToTransform(wtc.model);
  glm::vec2 wizardPos = glm::vec2{wt.position};

  bool hasThreat = false;
  glm::vec2 avoidDir{0.0f};

  for (ProjectileComponent &projectile : resources.projectiles) {
    if (projectile.ownerEntity == entity || !isEntityAlive(projectile.entity)) {
      continue;
    }

    TransformComponent &ptc = getTransform(projectile.entity);
    const Transform &pt = modelToTransform(ptc.model);

    glm::vec2 projectilePos = glm::vec2{pt.position};
    glm::vec2 projectileDir = glm::vec2{projectile.direction};

    if (glm::dot(projectileDir, projectileDir) <= 0.001f) {
      continue;
    }

    projectileDir = glm::normalize(projectileDir);
    glm::vec2 toWizard = wizardPos - projectilePos;
    float forwardDistance = glm::dot(toWizard, projectileDir);

    if (forwardDistance < 0.0f || forwardDistance > PROJECTILE_LOOKAHEAD) {
      continue;
    }

    glm::vec2 closestPoint = projectilePos + projectileDir * forwardDistance;
    float missDistanceSqr = getDistanceSqr(wizardPos, closestPoint);

    if (missDistanceSqr <=
        PROJECTILE_DANGER_RADIUS * PROJECTILE_DANGER_RADIUS) {
      glm::vec2 sideStep{-projectileDir.y, projectileDir.x};

      if (glm::dot(sideStep, toWizard) < 0.0f) {
        sideStep = -sideStep;
      }

      avoidDir += sideStep;
      hasThreat = true;
    }
  }

  CellCoord cell = worldToCell(wizardPos, spacialGridContext.cellWidth,
                               spacialGridContext.cellHeight);

  executeOnNearbyCells(cell, [entity, wizardPos, &hasThreat,
                              &avoidDir](int checkEntity) {
    WizardBehaviorComponent *otherWizard = tryGetWizardBehavior(checkEntity);
    OgreBehaviorComponent *ogre = tryGetOgreBehaviorComponent(checkEntity);

    if (otherWizard || ogre) {
      if (otherWizard && otherWizard->entity == entity) {
        return ExecuteOnNearbyCellsStatus::Running;
      }

      TransformComponent &otc = otherWizard != nullptr
                                    ? getTransform(otherWizard->entity)
                                    : getTransform(ogre->entity);
      const Transform &ot = modelToTransform(otc.model);
      glm::vec2 otherWizardPos = glm::vec2{ot.position};
      glm::vec2 away = wizardPos - otherWizardPos;
      float distanceSqr = getDistanceSqr(wizardPos, otherWizardPos);

      if (distanceSqr <= WIZARD_AVOID_RADIUS * WIZARD_AVOID_RADIUS &&
          distanceSqr > 0.001f) {
        avoidDir += glm::normalize(away);
        hasThreat = true;
      }
    }

    return ExecuteOnNearbyCellsStatus::Running;
  });

  if (hasThreat && glm::dot(avoidDir, avoidDir) > 0.001f) {
    wizard.evadeDir = glm::normalize(avoidDir);
    wizard.nextPos =
        clampToMovingArea(wizardPos + wizard.evadeDir * EVADE_DISTANCE);
  }

  return hasThreat;
}

bool hasEvadeStaminaLogic(int entity) {
  WizardBehaviorComponent &wizard = getWizardBehavior(entity);
  return wizard.stamina >= wizard.evadeStaminaCost;
}

DecisionStatus evadeMoveLogic(int entity) {
  WizardBehaviorComponent &wizard = getWizardBehavior(entity);

  if (wizard.state != WizzardState::Evading) {
    if (!spendStamina(wizard, wizard.evadeStaminaCost)) {
      return DecisionStatus::Done;
    }
    wizard.state = WizzardState::Evading;
  }

  AnimationComponent &animation = getAnimation(entity);
  animation.activeAnimation = WizardAnimationMapping::Running;
  animation.animationPlaySpeed = WIZARD_RUN_ANIMATION_SPEED;

  TransformComponent &tc = getTransform(entity);
  const Transform &transform = modelToTransform(tc.model);
  glm::vec2 position = glm::vec2{transform.position};
  glm::vec2 delta = wizard.nextPos - position;
  float distanceSqr = getDistanceSqr(position, wizard.nextPos);

  if (distanceSqr < 0.001f) {
    return DecisionStatus::Done;
  }

  float distance = glm::sqrt(distanceSqr);
  glm::vec2 direction = delta / distance;
  moveTowardsDir(tc.model, wizard.speed, direction, distance,
                 timeState.deltaTime);

  return DecisionStatus::Done;
}

bool hasAttackStaminaLogic(int entity) {
  WizardBehaviorComponent &wizard = getWizardBehavior(entity);
  return wizard.stamina >= wizard.attackStaminaCost;
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
  std::vector<int> candidates;
  candidates.reserve(resources.wizardBehaviors.size() +
                     resources.ogreBehaviors.size());

  for (const WizardBehaviorComponent &candidate : resources.wizardBehaviors) {
    if (candidate.entity != entity && isEntityAlive(candidate.entity)) {
      candidates.push_back(candidate.entity);
    }
  }

  for (const OgreBehaviorComponent &candidate : resources.ogreBehaviors) {
    if (isEntityAlive(candidate.entity)) {
      candidates.push_back(candidate.entity);
    }
  }

  if (candidates.empty()) {
    wizard.nextAttackEntity = -1;
    wizard.state = WizzardState::None;
    return DecisionStatus::Done;
  }

  wizard.nextAttackEntity = candidates[getRandom(candidates.size())];
  return DecisionStatus::Done;
}

DecisionStatus attackLogic(int entity) {
  WizardBehaviorComponent &wizard = getWizardBehavior(entity);
  TransformComponent &thisWizardTC = getTransform(entity);

  if (wizard.state != WizzardState::Attacking) {
    if (!spendStamina(wizard, wizard.attackStaminaCost)) {
      wizard.state = WizzardState::Recovering;
      return DecisionStatus::Done;
    }

    AnimationComponent &animation = getAnimation(entity);
    animation.activeAnimation = WizardAnimationMapping::Attacking;
    animation.animationTimeSeconds = 0.0f;

    wizard.state = WizzardState::Attacking;
    wizard.attackCounter++;

    wizard.shootEffecEntity = createEntity();

    Renderable &renderable = addRenderable(wizard.shootEffecEntity);
    renderable.renderKind = ObjectRenderKind::TransformAnimated;
    renderable.transformAnimatedMesh = &shootEffectMesh;
    renderable.visible = true;

    TransformComponent &effectTransform = addTransform(wizard.shootEffecEntity);
    effectTransform.baseModel = thisWizardTC.model;
    effectTransform.model = thisWizardTC.model;

    AnimationComponent &effectAnimation = addAnimation(wizard.shootEffecEntity);
    effectAnimation.activeAnimation = 0;
    effectAnimation.animationTimeSeconds = 0.0f;
    effectAnimation.animationPlaySpeed = 1.8f;

    wizard.attackLightEntity = createEntity();

    PointLightComponent &attackLight = addPointLight(wizard.attackLightEntity);
    attackLight.position = modelToTransform(thisWizardTC.model).position;
    attackLight.color = {1.0f, 0.0f, 0.0f};
    attackLight.intensity = ATTACK_LIGHT_INITIAL_INTENSITY;
    attackLight.attenuation = {0.01f, 0.01f, 0.01f};
  }

  TransformComponent &wizardTransform = getTransform(entity);
  const Transform &wizardWorld = modelToTransform(wizardTransform.model);
  const glm::vec2 wizardPosition{wizardWorld.position};

  wizard.attackTime = 0.0f;

  const int targetEntity = wizard.nextAttackEntity;

  WizardBehaviorComponent *targetWizard = tryGetWizardBehavior(targetEntity);
  OgreBehaviorComponent *targetOgre = tryGetOgreBehaviorComponent(targetEntity);

  const bool isValidTarget = isEntityAlive(targetEntity) &&
                             (targetWizard != nullptr || targetOgre != nullptr);

  if (!isValidTarget) {
    wizard.nextAttackEntity = -1;

    if (wizard.shootEffecEntity != -1) {
      destroyEntity(wizard.shootEffecEntity);
      wizard.shootEffecEntity = -1;
    }

    if (wizard.attackLightEntity != -1) {
      destroyEntity(wizard.attackLightEntity);
      wizard.attackLightEntity = -1;
    }

    wizard.state = WizzardState::None;
    return DecisionStatus::Done;
  }

  TransformComponent &targetTransform = getTransform(targetEntity);
  const Transform &targetWorld = modelToTransform(targetTransform.model);
  const glm::vec2 targetPosition{targetWorld.position};

  // Ogres currently have no velocity field, so aim at their current position.
  const glm::vec2 targetVelocity =
      targetWizard != nullptr ? targetWizard->velocity : glm::vec2{0.0f};

  const glm::vec2 predictedPosition =
      predictAimPoint(wizardPosition, targetPosition, targetVelocity);

  const glm::vec2 targetDir = targetPosition - wizardPosition;
  glm::vec2 aimDir = predictedPosition - wizardPosition;

  const float targetDistanceSqr =
      getDistanceSqr(wizardPosition, targetPosition);
  float aimDistanceSqr = getDistanceSqr(wizardPosition, predictedPosition);

  if (targetDistanceSqr > 0.001f) {
    faceTowardsDir(wizardTransform.model, glm::normalize(targetDir));
  }

  if (aimDistanceSqr <= 0.001f) {
    aimDir = targetDir;
    aimDistanceSqr = targetDistanceSqr;
  }

  if (PointLightComponent *attackLight =
          tryGetPointLight(wizard.attackLightEntity)) {
    glm::vec3 chargeDirection{0.0f, -1.0f, 0.0f};

    if (aimDistanceSqr > 0.001f) {
      const glm::vec2 normalizedAimDir = glm::normalize(aimDir);
      chargeDirection = {
          normalizedAimDir.x,
          normalizedAimDir.y,
          0.0f,
      };
    }

    attackLight->position = wizardWorld.position + chargeDirection;
    attackLight->intensity =
        glm::min(ATTACK_LIGHT_MAX_INTENSITY,
                 attackLight->intensity +
                     ATTACK_LIGHT_GROWTH_PER_SECOND * timeState.deltaTime);
  }

  if (!hasActiveAnimationEnded(wizard.shootEffecEntity)) {
    return DecisionStatus::Running;
  }

  wizard.state = WizzardState::None;

  const int projectileLightEntity = wizard.attackLightEntity;
  wizard.attackLightEntity = -1;

  if (aimDistanceSqr > 0.001f) {
    const glm::vec2 normalizedAimDir = glm::normalize(aimDir);

    spawnWizardProjectile(entity,
                          glm::vec3{
                              normalizedAimDir.x,
                              normalizedAimDir.y,
                              0.0f,
                          },
                          projectileLightEntity);
  } else {
    spawnWizardProjectile(entity, projectileLightEntity);
  }

  destroyEntity(wizard.shootEffecEntity);
  wizard.shootEffecEntity = -1;

  return DecisionStatus::Done;
}
bool isSomeoneCloseLogic(int entity) {

  TransformComponent &wtc = getTransform(entity);
  const Transform &wt = modelToTransform(wtc.model);
  glm::vec2 wizardPos = glm::vec2{wt.position};

  CellCoord cell = worldToCell(wizardPos, spacialGridContext.cellWidth,
                               spacialGridContext.cellHeight);

  bool isSomeoneClose = false;

  executeOnNearbyCells(cell, [entity, &isSomeoneClose,
                              wizardPos](int checkEntity) {
    WizardBehaviorComponent *checkWizard = tryGetWizardBehavior(checkEntity);
    OgreBehaviorComponent *checkOgre = nullptr;

    if (checkWizard == nullptr) {
      checkOgre = tryGetOgreBehaviorComponent(checkEntity);

      if (checkOgre == nullptr) {
        return ExecuteOnNearbyCellsStatus::Running;
      }
    }

    if (checkWizard != nullptr) {
      if (entity == checkWizard->entity) {
        return ExecuteOnNearbyCellsStatus::Running;
      }
    }

    TransformComponent &cwtc = checkWizard != nullptr
                                   ? getTransform(checkWizard->entity)
                                   : getTransform(checkOgre->entity);
    const Transform &cwt = modelToTransform(cwtc.model);
    glm::vec2 checkWizardPos = glm::vec2{cwt.position};

    float distance = getDistanceSqr(wizardPos, checkWizardPos);

    constexpr float closeRadius = CLOSE_RADIUS;
    constexpr float closeRadiusSqr = closeRadius * closeRadius;

    if (distance <= closeRadiusSqr) {
      isSomeoneClose = true;
      return ExecuteOnNearbyCellsStatus::Done;
    }

    return ExecuteOnNearbyCellsStatus::Running;
  });

  return isSomeoneClose;
}

bool waitedForNextAttackLogic(int entity) {
  WizardBehaviorComponent &wizard = getWizardBehavior(entity);
  recoverStamina(wizard);

  if (wizard.state != WizzardState::Waiting) {
    wizard.state = WizzardState::Waiting;

    AnimationComponent &animation = getAnimation(entity);
    animation.activeAnimation = WizardAnimationMapping::Iddle;
    animation.animationTimeSeconds = 0;
  }

  return wizard.attackTime >= 0.5 && wizard.stamina >= wizard.maxStamina;
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
  DecisionNode hasAttackStamina{};
  DecisionNode hasMoveStamina{};
  DecisionNode hasAvoidanceThreat{};
  DecisionNode hasEvadeStamina{};
  DecisionNode evadeMove{};
  DecisionNode recoverStamina{};

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
    positionIsFree.yes = &hasMoveStamina;

    hasMoveStamina.conditions = [wizardEntity]() {
      return hasMoveStaminaLogic(wizardEntity);
    };
    hasMoveStamina.no = &recoverStamina;
    hasMoveStamina.yes = &moveToPosition;

    moveToPosition.execute = [wizardEntity]() {
      return moveToPositionLogic(wizardEntity);
    };
    moveToPosition.next = &hasAvoidanceThreat;

    hasAvoidanceThreat.conditions = [wizardEntity]() {
      return hasAvoidanceThreatLogic(wizardEntity);
    };
    hasAvoidanceThreat.no = &arrivedNewPosition;
    hasAvoidanceThreat.yes = &hasEvadeStamina;

    hasEvadeStamina.conditions = [wizardEntity]() {
      return hasEvadeStaminaLogic(wizardEntity);
    };
    hasEvadeStamina.no = &recoverStamina;
    hasEvadeStamina.yes = &evadeMove;

    evadeMove.execute = [wizardEntity]() {
      return evadeMoveLogic(wizardEntity);
    };
    evadeMove.next = &hasAvoidanceThreat;

    recoverStamina.execute = [wizardEntity]() {
      return recoverStaminaLogic(wizardEntity);
    };
    recoverStamina.next = &hasAvoidanceThreat;

    arrivedNewPosition.conditions = [wizardEntity]() {
      return arrivedNewPositionLogic(wizardEntity);
    };
    arrivedNewPosition.no = &moveToPosition;
    arrivedNewPosition.yes = &chooseNextAttackEntity;

    chooseNextAttackEntity.execute = [wizardEntity]() {
      return chooseNextAttackEntityLogic(wizardEntity);
    };
    chooseNextAttackEntity.next = &hasAttackStamina;

    hasAttackStamina.conditions = [wizardEntity]() {
      return hasAttackStaminaLogic(wizardEntity);
    };
    hasAttackStamina.no = &recoverStamina;
    hasAttackStamina.yes = &attack;

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

static void addDebugRectXY(glm::vec3 center, float width, float height,
                           glm::vec4 color) {
  float halfWidth = width * 0.5f;
  float halfHeight = height * 0.5f;
  float x0 = center.x - halfWidth;
  float x1 = center.x + halfWidth;
  float y0 = center.y - halfHeight;
  float y1 = center.y + halfHeight;
  float z = center.z;

  addDebugLine({x0, y0, z}, {x1, y0, z}, color);
  addDebugLine({x1, y0, z}, {x1, y1, z}, color);
  addDebugLine({x1, y1, z}, {x0, y1, z}, color);
  addDebugLine({x0, y1, z}, {x0, y0, z}, color);
}

static void drawWizardStaminaDebug(const WizardBehaviorComponent &wizard,
                                   const Transform &transform) {
  float staminaRatio =
      wizard.maxStamina > 0.0f ? wizard.stamina / wizard.maxStamina : 0.0f;
  staminaRatio = glm::clamp(staminaRatio, 0.0f, 1.0f);

  constexpr float barWidth = 3.4f;
  constexpr float barHeight = 0.75f;
  constexpr int segmentCount = 10;

  glm::vec3 barCenter = transform.position + glm::vec3{0.0f, 1.75f, 1.55f};

  addDebugRectXY(barCenter + glm::vec3{0.0f, 0.0f, -0.02f}, barWidth + 0.45f,
                 barHeight + 0.45f, glm::vec4{0.0f, 0.0f, 0.0f, 1.0f});
  addDebugRectXY(barCenter + glm::vec3{0.0f, 0.0f, -0.01f}, barWidth + 0.25f,
                 barHeight + 0.25f, glm::vec4{1.0f, 0.0f, 1.0f, 1.0f});
  addDebugRectXY(barCenter, barWidth, barHeight,
                 glm::vec4{1.0f, 1.0f, 1.0f, 1.0f});

  float x0 = barCenter.x - barWidth * 0.5f;
  float y0 = barCenter.y - barHeight * 0.5f;
  float y1 = barCenter.y + barHeight * 0.5f;
  float segmentWidth = barWidth / static_cast<float>(segmentCount);
  int filledSegments = static_cast<int>(
      glm::ceil(staminaRatio * static_cast<float>(segmentCount)));

  glm::vec4 emptyColor{0.02f, 0.02f, 0.08f, 1.0f};
  glm::vec4 fillColor{0.0f, 1.0f, 0.15f, 1.0f};

  for (int segment = 0; segment < segmentCount; segment++) {
    float segmentX = x0 + (static_cast<float>(segment) + 0.5f) * segmentWidth;
    glm::vec4 color = segment < filledSegments ? fillColor : emptyColor;
    addDebugLine({segmentX, y0, barCenter.z + 0.01f},
                 {segmentX, y1, barCenter.z + 0.01f}, color);
    addDebugLine({segmentX + segmentWidth * 0.22f, y0, barCenter.z + 0.015f},
                 {segmentX + segmentWidth * 0.22f, y1, barCenter.z + 0.015f},
                 color);
    addDebugLine({segmentX - segmentWidth * 0.22f, y0, barCenter.z + 0.015f},
                 {segmentX - segmentWidth * 0.22f, y1, barCenter.z + 0.015f},
                 color);
  }

  if (staminaRatio >= 1.0f) {
    addDebugRectXY(barCenter + glm::vec3{0.0f, 0.0f, 0.03f}, barWidth + 0.18f,
                   barHeight + 0.18f, glm::vec4{1.0f, 1.0f, 0.0f, 1.0f});
  }
}

static void drawWizardDebug(const WizardBehaviorComponent &wizardBehavior) {
  TransformComponent &wtc = getTransform(wizardBehavior.entity);
  const Transform &wt = modelToTransform(wtc.model);

  drawWizardStaminaDebug(wizardBehavior, wt);

  switch (wizardBehavior.state) {
  case WizzardState::MovingToPosition:
    addDebugLine(
        wt.position,
        glm::vec3{wizardBehavior.nextPos.x, wizardBehavior.nextPos.y, 1.0f},
        wizardBehavior.debugColor);
    addDebugDiskXY(
        glm::vec3{wizardBehavior.nextPos.x, wizardBehavior.nextPos.y, 1.0f},
        NEXT_POS_RADIUS, wizardBehavior.debugColor);
    break;

  case WizzardState::Attacking:
    if (wizardBehavior.nextAttackEntity != -1 &&
        isEntityAlive(wizardBehavior.nextAttackEntity)) {
      TransformComponent &targetTc =
          getTransform(wizardBehavior.nextAttackEntity);
      const Transform &target = modelToTransform(targetTc.model);
      WizardBehaviorComponent *targetBehavior =
          tryGetWizardBehavior(wizardBehavior.nextAttackEntity);
      glm::vec2 predictedTargetPos = glm::vec2{target.position};

      if (targetBehavior != nullptr) {
        predictedTargetPos =
            predictAimPoint(glm::vec2{wt.position}, glm::vec2{target.position},
                            targetBehavior->velocity);
      }

      addDebugLine(wt.position, target.position, wizardBehavior.debugColor);
      addDebugSphere(target.position, 0.75f, wizardBehavior.debugColor);
      addDebugLine(wt.position,
                   glm::vec3{predictedTargetPos.x, predictedTargetPos.y, 1.25f},
                   glm::vec4{1.0f, 0.0f, 1.0f, 1.0f});
      addDebugSphere(glm::vec3{predictedTargetPos.x, predictedTargetPos.y,
                               target.position.z},
                     0.35f, glm::vec4{1.0f, 0.0f, 1.0f, 1.0f});
    }
    break;

  case WizzardState::Waiting:
    addDebugDiskXY(wt.position, CLOSE_RADIUS, wizardBehavior.debugColor);
    break;

  case WizzardState::ChoosingNewPosition:
    addDebugCube(
        glm::vec3{wizardBehavior.nextPos.x, wizardBehavior.nextPos.y, 1.0f},
        0.5f, wizardBehavior.debugColor);
    break;

  case WizzardState::Evading:
    addDebugLine(wt.position,
                 wt.position + glm::vec3{wizardBehavior.evadeDir * 3.0f, 0.0f},
                 glm::vec4{1.0f, 1.0f, 0.0f, 1.0f});
    addDebugDiskXY(
        glm::vec3{wizardBehavior.nextPos.x, wizardBehavior.nextPos.y, 1.08f},
        PROJECTILE_DANGER_RADIUS, glm::vec4{1.0f, 1.0f, 0.0f, 1.0f});
    break;

  case WizzardState::Recovering:
    addDebugDiskXY(wt.position, CLOSE_RADIUS,
                   glm::vec4{1.0f, 0.45f, 0.0f, 1.0f});
    break;

  default:
    break;
  }
}

static void initWizard(WizardBehaviorComponent &wizardBehavior) {
  WizardDecisionTree &decisionTree = decisionTrees[wizardBehavior.entity];
  TransformComponent &wtc = getTransform(wizardBehavior.entity);
  const Transform &wt = modelToTransform(wtc.model);

  wizardBehavior.previousPosition = glm::vec2{wt.position};
  wizardBehavior.velocity = glm::vec2{0.0f};

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
    glm::vec2 wizardPositionBeforeTick = glm::vec2{wt.position};

    decisionTree.tick();

    TransformComponent &updatedWtc = getTransform(wizardBehavior.entity);
    const Transform &updatedWt = modelToTransform(updatedWtc.model);
    glm::vec2 wizardPositionAfterTick = glm::vec2{updatedWt.position};

    if (timeState.deltaTime > 0.0f) {
      glm::vec2 measuredVelocity =
          (wizardPositionAfterTick - wizardBehavior.previousPosition) /
          timeState.deltaTime;
      wizardBehavior.velocity =
          wizardBehavior.velocity * (1.0f - AIM_VELOCITY_SMOOTHING) +
          measuredVelocity * AIM_VELOCITY_SMOOTHING;
    } else {
      wizardBehavior.velocity = glm::vec2{0.0f};
    }

    if (getDistanceSqr(wizardPositionBeforeTick, wizardPositionAfterTick) <=
        0.0001f) {
      wizardBehavior.velocity *= 1.0f - AIM_VELOCITY_SMOOTHING;
    }

    wizardBehavior.previousPosition = wizardPositionAfterTick;

    if (vulkanRendererContext.isDebug) {
      drawWizardDebug(wizardBehavior);
    }

    if (wizardBehavior.shootEffecEntity != -1 &&
        isEntityAlive(wizardBehavior.shootEffecEntity)) {
      TransformComponent &effectTransform =
          getTransform(wizardBehavior.shootEffecEntity);

      effectTransform.baseModel = wtc.model;
    }
  }
}
