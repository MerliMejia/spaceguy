#pragma once

#include "../decisionTree.h"
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "../../systems/resourceManagementSystem.h"
#include "../../utils/math.h"
#include <cstdint>
#include <functional>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

enum class WizardState { Thinking, Moving, Attacking, Kicking, BeingAttacked };

struct WizardBehavior {
  // For external use
  int id;

  // Tuning
  float speed = 3.0f;
  float moveToRadius = CHECK_RADIUS * 4;
  float thinkingTime = 0.2f;
  float tick = 0.0f;

  // Current action state
  WizardState state = WizardState::Thinking;
  glm::vec3 nextMovePoint;
  glm::vec3 kickingObject;
  int currentAttackingEntity = -1;

  // Orientation captured from the imported model the first time it updates.
  bool hasInitialRotation = false;
  glm::mat4 initialRotation{1.0f};
  float initialForwardYaw = 0.0f;

  // Consecutive action counters
  int moves = 0;
  int attacks = 0;
  int kicks = 0;

  // Shared condition used by the tree and debug drawing.
  std::function<bool()> someoneIsClose;
  std::function<bool()> someoneIsSuperClose;

  // Decision tree action leaves
  DecisionNode executeBeingAttackedLogic;
  DecisionNode attackNode;
  DecisionNode moveNode;
  DecisionNode thinkingNode;
  DecisionNode continueActionNode;

  // Decision tree interrupt nodes
  DecisionNode beingAttackedNode;
  DecisionNode kickNode;

  // Decision tree branches
  DecisionNode actionInProgressNode;
  DecisionNode thoughtEnoughNode;
  DecisionNode someoneIsCloseNode;
  DecisionNode someoneIsSuperCloseNode;
  DecisionNode tooManyMovesNode;
  DecisionNode tooManyAttacksNode;
  DecisionNode tooManyKicksNode;
};

void initializeWizardBehavior(int entity, WizardBehavior &currentBehavior);
void behaveLikeWizard(int entity, WizardBehavior &currentBehavior);

struct FindClosestWizardInRangeReturn {
  bool found;
  int otherEntity;
};

FindClosestWizardInRangeReturn
findClosestWizardInRange(int selfEntity, glm::vec3 &closestPosition,
                         float closestDistanceSquared = RADIUS_SQ);
