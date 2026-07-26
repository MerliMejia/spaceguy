#include "ogreBehaviorSystem.h"
#include "../behaviors/decisionTree.h"
#include "../engine/vulkanRenderer.h"
#include "../utils/behaviors/ogre.h"
#include "../utils/math.h"
#include "resourceManagementSystem.h"

struct OgreDecisionTree {
  int entity = -1;
  DecisionTreeRunner decisionRunner;

  // questions
  DecisionNode checkIfAlreadyInAttackMode{};
  DecisionNode checkIfSomeoneClose{};
  DecisionNode checkIfWaitedForInitAttack{};
  DecisionNode checkIfCloseEnoughToAttackTarget{};
  DecisionNode checkIfAttackEntityIsValid{};

  // executions
  DecisionNode iddleMode{};
  DecisionNode startAttackMode{};
  DecisionNode attackMode{};
  DecisionNode moveToAttack{};
  DecisionNode flyingAttack{};

  void init(int ogreEntity) {

    checkIfAlreadyInAttackMode.conditions = [ogreEntity]() {
      return isAlreadyInAttackMode(ogreEntity);
    };

    checkIfAlreadyInAttackMode.yes = &attackMode;
    checkIfAlreadyInAttackMode.no = &checkIfSomeoneClose;

    checkIfSomeoneClose.conditions = [ogreEntity]() {
      return checkIfSomeoneCloseLogic(ogreEntity);
    };

    checkIfSomeoneClose.yes = &startAttackMode;
    checkIfSomeoneClose.no = &iddleMode;

    iddleMode.execute = [ogreEntity]() { return iddleModeLogic(ogreEntity); };

    startAttackMode.execute = [ogreEntity]() {
      return startAttackModeLogic(ogreEntity);
    };

    startAttackMode.next = &attackMode;

    attackMode.execute = [ogreEntity]() { return attackModeLogic(ogreEntity); };
    attackMode.next = &checkIfWaitedForInitAttack;

    checkIfWaitedForInitAttack.conditions = [ogreEntity]() {
      return waitedForInitAttackLogic(ogreEntity);
    };

    checkIfWaitedForInitAttack.no = &attackMode;
    checkIfWaitedForInitAttack.yes = &moveToAttack;

    moveToAttack.execute = [ogreEntity]() {
      return moveToAttackLogic(ogreEntity);
    };

    moveToAttack.next = &checkIfAttackEntityIsValid;

    checkIfAttackEntityIsValid.conditions = [ogreEntity]() {
      return isAttackEntityIsValid(ogreEntity);
    };
    checkIfAttackEntityIsValid.no = &checkIfAlreadyInAttackMode;
    checkIfAttackEntityIsValid.yes = &checkIfCloseEnoughToAttackTarget;

    checkIfCloseEnoughToAttackTarget.conditions = [ogreEntity]() {
      return isCloseEnoughToAttackTarget(ogreEntity);
    };

    checkIfCloseEnoughToAttackTarget.no = &moveToAttack;
    checkIfCloseEnoughToAttackTarget.yes = &flyingAttack;

    flyingAttack.execute = [ogreEntity]() {
      return flyingAttackLogic(ogreEntity);
    };
    flyingAttack.next = &attackMode;

    decisionRunner.reset(&checkIfAlreadyInAttackMode);
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
