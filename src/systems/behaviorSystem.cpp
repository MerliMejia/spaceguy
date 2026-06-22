#include "behaviorSystem.h"
#include "../behaviors/wizards/wizardBehaviorUtils.h"
#include "../engine/vulkanRenderer.h"
#include "resourceManagementSystem.h"
#include <algorithm>
#include <cstddef>

BehaviorContext behaviorContext;

bool isWizardBehaviorEntity(int entity) {
  if (!isEntityAlive(entity)) {
    return false;
  }

  const BehaviorComponent *behavior = tryGetBehavior(entity);
  return behavior != nullptr &&
         behavior->behaviorKind == BehaviorKind::Wizard &&
         tryGetTransform(entity) != nullptr &&
         tryGetAnimation(entity) != nullptr;
}

bool isVisibleWizardBehaviorEntity(int entity) {
  if (!isWizardBehaviorEntity(entity)) {
    return false;
  }

  const Renderable *renderable = tryGetRenderable(entity);
  return renderable != nullptr && renderable->visible;
}

std::vector<int> getVisibleWizardBehaviorEntities() {
  std::vector<int> wizardEntities;

  for (const BehaviorComponent &behavior : resources.behaviors) {
    if (behavior.behaviorKind == BehaviorKind::Wizard &&
        isVisibleWizardBehaviorEntity(behavior.entity)) {
      wizardEntities.push_back(behavior.entity);
    }
  }

  return wizardEntities;
}

void clearWizardAttackReferences(int entity) {
  if (entity < 0) {
    return;
  }

  behaviorContext.wizardAttacking.erase(entity);

  for (auto it = behaviorContext.wizardAttacking.begin();
       it != behaviorContext.wizardAttacking.end();) {
    if (it->second == entity) {
      it = behaviorContext.wizardAttacking.erase(it);
    } else {
      ++it;
    }
  }

  for (auto &[_, behavior] : behaviorContext.wizardBehaviorsByEntity) {
    if (behavior.currentAttackingEntity == entity) {
      behavior.currentAttackingEntity = -1;
      if (behavior.state == WizardState::Attacking ||
          behavior.state == WizardState::Kicking ||
          behavior.state == WizardState::BeingAttacked) {
        behavior.state = WizardState::Thinking;
      }
    }
  }
}

static void syncBehaviorContext() {
  for (auto it = behaviorContext.wizardBehaviorsByEntity.begin();
       it != behaviorContext.wizardBehaviorsByEntity.end();) {
    if (!isWizardBehaviorEntity(it->first)) {
      const int removedEntity = it->first;
      it = behaviorContext.wizardBehaviorsByEntity.erase(it);
      clearWizardAttackReferences(removedEntity);
    } else {
      ++it;
    }
  }

  for (const BehaviorComponent &behavior : resources.behaviors) {
    if (behavior.behaviorKind != BehaviorKind::Wizard ||
        !isWizardBehaviorEntity(behavior.entity)) {
      continue;
    }

    auto [it, inserted] =
        behaviorContext.wizardBehaviorsByEntity.try_emplace(behavior.entity);
    if (inserted) {
      initializeWizardBehavior(behavior.entity, it->second);
    }
  }

  for (auto it = behaviorContext.wizardAttacking.begin();
       it != behaviorContext.wizardAttacking.end();) {
    if (!isWizardBehaviorEntity(it->first) ||
        !isWizardBehaviorEntity(it->second)) {
      it = behaviorContext.wizardAttacking.erase(it);
    } else {
      ++it;
    }
  }

  behaviorContext.wizardShootingEffects.erase(
      std::remove_if(
          behaviorContext.wizardShootingEffects.begin(),
          behaviorContext.wizardShootingEffects.end(),
          [](const WizardShootEffect &shootEffect) {
            return !isWizardBehaviorEntity(shootEffect.wizardEntity) ||
                   !isEntityAlive(shootEffect.effectEntity) ||
                   tryGetTransform(shootEffect.effectEntity) == nullptr;
          }),
      behaviorContext.wizardShootingEffects.end());
}

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
  case WizardState::BeingAttacked:
    return glm::vec4{0.85f, 0.0f, 1.0f, 1.0f};
  }

  return glm::vec4{1.0f, 1.0f, 1.0f, 1.0f};
}

static void drawWizardDebug(int entity, const WizardBehavior &behavior) {
  const TransformComponent &transform = getTransform(entity);
  const glm::vec3 position = glm::vec3(transform.model[3]);
  const bool someoneIsClose = wizardIsSomeoneClose(entity);
  glm::vec3 closestPosition{};
  const bool someoneIsSuperClose =
      findClosestWizardInRange(entity, closestPosition, SUPER_CLOSE_RADIUS_SQ)
          .found;

  const glm::vec4 radiusColor = someoneIsClose
                                    ? glm::vec4{1.0f, 0.05f, 0.05f, 1.0f}
                                    : glm::vec4{0.25f, 0.45f, 1.0f, 1.0f};
  const glm::vec4 superCloseRadiusColor =
      someoneIsSuperClose ? glm::vec4{1.0f, 0.05f, 0.05f, 1.0f}
                          : glm::vec4{0.25f, 0.45f, 1.0f, 1.0f};

  addDebugDiskXY(position, CHECK_RADIUS, radiusColor);
  addDebugDiskXY(position, SUPER_CLOSE_RADIUS, superCloseRadiusColor);

  const glm::vec3 forward = glm::normalize(
      glm::vec3(transform.model * glm::vec4{0.0f, -1.0f, 0.0f, 0.0f}));
  addDebugLine(position, position + forward * 2.0f,
               glm::vec4{1.0f, 0.0f, 0.85f, 1.0f});

  if (behavior.state == WizardState::Moving) {
    addDebugCube(behavior.nextMovePoint, 0.75f,
                 glm::vec4{1.0f, 0.45f, 0.0f, 1.0f});

    addDebugLine(position, behavior.nextMovePoint,
                 glm::vec4{1.0f, 0.45f, 0.0f, 1.0f});
  }

  if (behavior.state == WizardState::Attacking &&
      isWizardBehaviorEntity(behavior.currentAttackingEntity)) {
    const TransformComponent &currentAttackingTransform =
        getTransform(behavior.currentAttackingEntity);

    const glm::vec3 currentAttackingPosition =
        glm::vec3(currentAttackingTransform.model[3]);

    addDebugLine(position, currentAttackingPosition,
                 debugColorForWizardState(WizardState::Attacking));
  }

  glm::vec3 closestWizardPosition{};
  auto foundInRange = findClosestWizardInRange(entity, closestWizardPosition);
  if (foundInRange.found) {
    addDebugLine(position, closestWizardPosition,
                 glm::vec4{1.0f, 0.0f, 0.0f, 1.0f});
  }

  addDebugLine(position, position + glm::vec3(0, 0, 5),
               debugColorForWizardState(behavior.state));
}

void addWizardShootingEffect(int wizardEntity, int effectEntity) {
  behaviorContext.wizardShootingEffects.push_back(WizardShootEffect{
      .wizardEntity = wizardEntity, .effectEntity = effectEntity});
}

void initializeBehaviorSystem() { syncBehaviorContext(); }

void updateBehaviorSystem() {
  syncBehaviorContext();

  for (const BehaviorComponent &behaviorComponent : resources.behaviors) {
    if (behaviorComponent.behaviorKind != BehaviorKind::Wizard ||
        !isVisibleWizardBehaviorEntity(behaviorComponent.entity)) {
      continue;
    }

    WizardBehavior &behavior =
        behaviorContext.wizardBehaviorsByEntity.at(behaviorComponent.entity);
    behaveLikeWizard(behaviorComponent.entity, behavior);
    if (vulkanRendererContext.isDebug) {
      drawWizardDebug(behaviorComponent.entity, behavior);
    }
  }
}

void updateWizardEffects() {
  syncBehaviorContext();

  for (WizardShootEffect &shootingEffect :
       behaviorContext.wizardShootingEffects) {
    updateWizardShootingEffect(shootingEffect);
  }
}
