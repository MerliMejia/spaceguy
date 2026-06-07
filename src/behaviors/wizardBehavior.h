#pragma once

#include "decisionTree.h"
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "../engine/vulkanRenderer.h"
#include "../utils/math.h"
#include <functional>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

enum class WizardState { Thinking, Moving, Attacking, Kicking };

struct WizardBehavior {
  // Tuning
  float speed = 3.0f;
  float moveToRadius = CHECK_RADIUS * 4;
  float thinkingTime = 0.2f;
  float tick = 0.0f;

  // Current action state
  WizardState state = WizardState::Thinking;
  glm::vec3 nextMovePoint;
  glm::vec3 kickingObject;

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
  DecisionNode attackNode;
  DecisionNode moveNode;
  DecisionNode thinkingNode;
  DecisionNode continueActionNode;

  // Decision tree interrup nodes
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

void initializeWizardDecisionTree(Object3D &object,
                                  WizardBehavior &currentBehavior);
void behaveLikeWizzard(Object3D &object, WizardBehavior &currentBehavior);
bool findClosestWizardInRange(const Object3D &self, glm::vec3 &closestPosition,
                              float closestDistanceSquared = RADIUS_SQ);
