#include "wizardBehavior.h"
#include "../../utils/time.h"
#include "../decisionTree.h"
#include "wizardBehaviorUtils.h"

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

static bool isActionInProgress(const WizardBehavior &behavior) {
  return behavior.state == WizardState::Moving ||
         behavior.state == WizardState::Attacking ||
         behavior.state == WizardState::Kicking;
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

FindClosestWizardInRangeReturn
findClosestWizardInRange(const Object3D &self, glm::vec3 &closestPosition,
                         float closestDistanceSquared) {
  const glm::vec3 selfPosition = glm::vec3(self.model[3]);
  bool foundWizard = false;
  size_t otherIndex = SIZE_MAX;

  for (int i = 0; i < vulkanRendererContext.objects.size(); i++) {
    const Object3D &other = vulkanRendererContext.objects[i];

    if (!other.enabled)
      continue;

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
