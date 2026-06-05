#include "wizardBehavior.h"
#include "../systems/animationSystem.h"
#include "../utils/time.h"
#include "./decisionTree.h"

void attackExecute(Object3D &object, WizardBehavior &behavior) {
  if (behavior.state != WizardState::Attacking) {
    object.activeAnimation = 0;
    object.animationTimeSeconds = 0;
    behavior.state = WizardState::Attacking;
  }

  if (hasActiveAnimationEnded(object)) {
    object.activeAnimation = 1;
    behavior.state = WizardState::Thinking;
    behavior.tick = 0;
    behavior.attacks++;
    behavior.moves = 0;
  }
}
void moveExecute(Object3D &object, WizardBehavior &behavior,
                 glm::vec3 &objectPosition, glm::vec3 &objectScale) {

  if (behavior.state != WizardState::Moving) {
    // The center should be "the position 0,0 of the current scene when the
    // camera is not moving"
    behavior.nextMovePoint =
        randomPointInCircleXY(glm::vec3(0.0f), behavior.moveToRadius);

    behavior.state = WizardState::Moving;
    object.activeAnimation = 3;
  }

  glm::vec2 a{objectPosition.x, objectPosition.y};
  glm::vec2 b{behavior.nextMovePoint.x, behavior.nextMovePoint.y};
  glm::vec2 delta = b - a;

  float distance = glm::length(delta);

  if (distance > 0.000001f) {
    glm::vec2 direction = delta / distance;

    float targetYaw = std::atan2(direction.y, direction.x);
    float yawDelta = targetYaw - behavior.initialForwardYaw;

    float step = glm::min(behavior.speed * timeState.deltaTime, distance);
    a += direction * step;

    objectPosition.x = a.x;
    objectPosition.y = a.y;

    glm::mat4 model{1.0f};
    model = glm::translate(model, objectPosition);
    model = glm::rotate(model, yawDelta, glm::vec3{0.0f, 0.0f, 1.0f});
    model = model * behavior.initialRotation;
    model = glm::scale(model, objectScale);

    object.model = model;
  } else {
    behavior.tick = 0.0f;
    behavior.state = WizardState::Thinking;
    object.activeAnimation = 1;
    behavior.moves++;
    behavior.attacks = 0;
  }
}
void thinkingExecute(Object3D &object, WizardBehavior &behavior) {
  behavior.state = WizardState::Thinking;
  // Should definetly map animations
  object.activeAnimation = 1;
}

bool isSomeoneClose(const Object3D &self) {
  const glm::vec3 selfPosition = glm::vec3(self.model[3]);

  for (const Object3D &other : vulkanRendererContext.objects) {
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

    if (distanceSquared < RADIUS_SQ) {
      return true;
    }
  }

  return false;
}

void behaveLikeWizzard(Object3D &object, WizardBehavior &currentBehavior) {

  currentBehavior.tick += timeState.deltaTime;

  glm::vec3 objectPosition = glm::vec3(object.model[3]);
  glm::vec3 objectScale = glm::vec3{glm::length(glm::vec3(object.model[0])),
                                    glm::length(glm::vec3(object.model[1])),
                                    glm::length(glm::vec3(object.model[2]))};

  if (!currentBehavior.hasInitialRotation) {
    glm::mat4 initialRotation{1.0f};

    initialRotation[0] =
        glm::vec4(glm::normalize(glm::vec3(object.model[0])), 0.0f);
    initialRotation[1] =
        glm::vec4(glm::normalize(glm::vec3(object.model[1])), 0.0f);
    initialRotation[2] =
        glm::vec4(glm::normalize(glm::vec3(object.model[2])), 0.0f);
    initialRotation[3] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

    glm::vec3 localForward{0.0f, -1.0f, 0.0f}; // -Y
    glm::vec3 initialForward = glm::normalize(
        glm::vec3(initialRotation * glm::vec4(localForward, 0.0f)));

    currentBehavior.initialRotation = initialRotation;
    currentBehavior.initialForwardYaw =
        std::atan2(initialForward.y, initialForward.x);
    currentBehavior.hasInitialRotation = true;
  }

  DecisionNode attackNode{
      .execute = [&]() { attackExecute(object, currentBehavior); }};

  DecisionNode moveNode{.execute = [&]() {
    moveExecute(object, currentBehavior, objectPosition, objectScale);
  }};

  DecisionNode someoneIscloseNode;

  DecisionNode tooManyMovesNode;
  tooManyMovesNode.yes = &attackNode;
  tooManyMovesNode.no = &moveNode;
  tooManyMovesNode.conditions = {currentBehavior.moves >= 2};

  DecisionNode tooManyAttacksNode;
  tooManyAttacksNode.yes = &moveNode;
  tooManyAttacksNode.no = &attackNode;
  tooManyAttacksNode.conditions = {currentBehavior.attacks >= 2};

  someoneIscloseNode.conditions = {isSomeoneClose(object)};
  someoneIscloseNode.yes = &tooManyMovesNode;
  someoneIscloseNode.no = &tooManyAttacksNode;

  DecisionNode thinkingNode;
  thinkingNode.execute = [&]() { thinkingExecute(object, currentBehavior); };

  DecisionNode hasToughtEnoughNode;
  hasToughtEnoughNode.conditions = {currentBehavior.tick >
                                    currentBehavior.thinkingTime};
  hasToughtEnoughNode.yes = &someoneIscloseNode;
  hasToughtEnoughNode.no = &thinkingNode;

  evaluateDecisionTree(&hasToughtEnoughNode);
}
