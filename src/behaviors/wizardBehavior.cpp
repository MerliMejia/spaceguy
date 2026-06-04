#include "wizardBehavior.h"

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

  if (currentBehavior.tick < currentBehavior.thinkingTime) {
    currentBehavior.state = WizardState::Thinking;
    // Should definetly map animations
    object.activeAnimation = 1;
    return;
  }

  if (currentBehavior.state != WizardState::Moving) {
    // The center should be "the position 0,0 of the current scene when the
    // camera is not moving"
    currentBehavior.nextMovePoint =
        randomPointInCircleXY(glm::vec3(0.0f), currentBehavior.moveToRadius);

    currentBehavior.state = WizardState::Moving;
    object.activeAnimation = 3;
  }

  glm::vec2 a{objectPosition.x, objectPosition.y};
  glm::vec2 b{currentBehavior.nextMovePoint.x, currentBehavior.nextMovePoint.y};
  glm::vec2 delta = b - a;

  float distance = glm::length(delta);

  if (distance > 0.000001f) {
    glm::vec2 direction = delta / distance;

    float targetYaw = std::atan2(direction.y, direction.x);
    float yawDelta = targetYaw - currentBehavior.initialForwardYaw;

    float step =
        glm::min(currentBehavior.speed * timeState.deltaTime, distance);
    a += direction * step;

    objectPosition.x = a.x;
    objectPosition.y = a.y;

    glm::mat4 model{1.0f};
    model = glm::translate(model, objectPosition);
    model = glm::rotate(model, yawDelta, glm::vec3{0.0f, 0.0f, 1.0f});
    model = model * currentBehavior.initialRotation;
    model = glm::scale(model, objectScale);

    object.model = model;
  } else {
    currentBehavior.tick = 0.0f;
    currentBehavior.state = WizardState::Thinking;
    object.activeAnimation = 1;
  }
}
