#include "ogreBehaviorSystem.h"
#include "../behaviors/decisionTree.h"
#include "../engine/vulkanRenderer.h"
#include "../utils/behaviors.h"
#include "../utils/math.h"
#include "animationSystem.h"
#include "resourceManagementSystem.h"
#include "unordered_map"

constexpr int HOW_CLOSE = 16;
constexpr int SQUARES_TO_CHECK = 8;

static DecisionStatus noOneIsAroundLogic(int entity) {
  TransformComponent &tc = getTransform(entity);
  Transform transform = modelToTransform(tc.model);

  // Blue: no one is close
  addDebugLine(transform.position,
               glm::vec3{transform.position.x, transform.position.y, 10},
               glm::vec4{0, 0, 1, 1});

  return DecisionStatus::Done;
}

static DecisionStatus startAttackModeLogic(int entity) {
  TransformComponent &tc = getTransform(entity);
  Transform transform = modelToTransform(tc.model);
  OgreBehaviorComponent &behavior = getOgreBehaviorComponent(entity);

  // Red: Someone is close
  addDebugLine(transform.position,
               glm::vec3{transform.position.x, transform.position.y, 10},
               glm::vec4{1, 0, 0, 1});

  AnimationComponent &animation = getAnimation(entity);

  if (behavior.state != OgreState::InitiatingAttackMode &&
      behavior.state != OgreState::InAttackMode) {
    behavior.state = OgreState::InitiatingAttackMode;
    animation.activeAnimation = OgreAnimationMapping::OgreInitAttackMode;
    animation.animationTimeSeconds = 0;
  }

  if (hasActiveAnimationEnded(entity) &&
      behavior.state != OgreState::InAttackMode) {
    animation.activeAnimation = OgreAnimationMapping::OgreAttackMode;
    animation.animationTimeSeconds = 0;
    behavior.state = OgreState::InAttackMode;
    return DecisionStatus::Done;
  }

  return DecisionStatus::Running;
}

struct OgreDecisionTree {
  int entity = -1;
  DecisionTreeRunner decisionRunner;

  DecisionNode checkIfSomeoneClose{};
  DecisionNode noOneIsAround{};
  DecisionNode startAttackMode{};

  void init(int ogreEntity) {
    checkIfSomeoneClose.conditions = [ogreEntity]() {
      auto data = BehaviorUtil::isSomeoneCloseLogic(
          ogreEntity, BehaviorUtil::CheckType::Wizards, SQUARES_TO_CHECK,
          HOW_CLOSE);

      if (data.isSomeoneClose) {
        OgreBehaviorComponent &behavior = getOgreBehaviorComponent(ogreEntity);
        behavior.attackingEntity = data.closeEntity;
      }
      return data.isSomeoneClose;
    };

    checkIfSomeoneClose.no = &noOneIsAround;
    checkIfSomeoneClose.yes = &startAttackMode;

    noOneIsAround.execute = [ogreEntity]() {
      return noOneIsAroundLogic(ogreEntity);
    };

    startAttackMode.execute = [ogreEntity]() {
      return startAttackModeLogic(ogreEntity);
    };

    decisionRunner.reset(&checkIfSomeoneClose);
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
