#include "wizardBehavior.h"
#include "../../utils/time.h"
#include "../decisionTree.h"
#include "wizardBehaviorUtils.h"

static void initializeActionNodes(int entity, WizardBehavior &behavior,
                                  Renderable &renderable) {
  behavior.attackNode.execute = [entity, &behavior]() {
    wizardAttackExecute(entity, behavior);
  };

  behavior.kickNode.execute = [entity, &behavior, &renderable]() {
    wizardKickExecute(entity, behavior, renderable);
  };

  behavior.moveNode.execute = [entity, &behavior]() {
    wizardMoveExecute(entity, behavior);
  };

  behavior.thinkingNode.execute = [entity, &behavior]() {
    wizardThinkingExecute(entity, behavior);
  };

  behavior.continueActionNode.execute = [entity, &behavior, &renderable]() {
    continueCurrentAction(entity, behavior, renderable);
  };

  behavior.executeBeingAttackedLogic.execute = [entity, &behavior]() {
    wizardBeingAttackedExecute(entity, behavior);
  };
}

static bool isActionInProgress(const WizardBehavior &behavior) {
  return behavior.state == WizardState::Moving ||
         behavior.state == WizardState::Attacking ||
         behavior.state == WizardState::Kicking;
}

static void initializeConditionNodes(int entity,
                                     WizardBehavior &behavior) {
  behavior.someoneIsClose = [entity]() {
    return wizardIsSomeoneClose(entity);
  };

  behavior.someoneIsSuperClose = [entity, &behavior]() {
    return wizardSomeoneIsSuperClose(entity, behavior);
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

FindClosestWizardInRangeReturn
findClosestWizardInRange(int selfEntity, glm::vec3 &closestPosition,
                         float closestDistanceSquared) {
  const TransformComponent &selfTransform = getTransform(selfEntity);
  const glm::vec3 selfPosition = glm::vec3(selfTransform.model[3]);
  bool foundWizard = false;
  int otherEntity = -1;

  for (const WorldComponent &otherWorld : resources.worlds) {
    const Renderable *otherRenderable = tryGetRenderable(otherWorld.entity);

    if (otherRenderable == nullptr || !otherRenderable->visible)
      continue;

    if (otherWorld.entity == selfEntity) {
      continue;
    }

    if (otherWorld.worldKind != ObjectWorldKind::Wizard) {
      continue;
    }

    const TransformComponent &otherTransform = getTransform(otherWorld.entity);
    const glm::vec3 otherPosition = glm::vec3(otherTransform.model[3]);

    const glm::vec2 delta{
        otherPosition.x - selfPosition.x,
        otherPosition.y - selfPosition.y,
    };

    const float distanceSquared = glm::dot(delta, delta);

    if (distanceSquared <= closestDistanceSquared) {
      closestPosition = otherPosition;
      closestDistanceSquared = distanceSquared;
      foundWizard = true;
      otherEntity = otherWorld.entity;
    }
  }

  return FindClosestWizardInRangeReturn{.found = foundWizard,
                                        .otherEntity = otherEntity};
}

void initializeWizardDecisionTree(int entity, WizardBehavior &behavior,
                                  Renderable &renderable) {
  initializeActionNodes(entity, behavior, renderable);
  initializeConditionNodes(entity, behavior);
  connectDecisionTree(behavior);
}

void behaveLikeWizzard(int entity, WizardBehavior &currentBehavior) {
  currentBehavior.tick += timeState.deltaTime;

  if (!currentBehavior.hasInitialRotation) {
    currentBehavior.initialRotation = captureInitialRotation(entity);
    currentBehavior.initialForwardYaw =
        getForwardYaw(currentBehavior.initialRotation);
    currentBehavior.hasInitialRotation = true;
  }

  evaluateDecisionTree(&currentBehavior.beingAttackedNode);
}
