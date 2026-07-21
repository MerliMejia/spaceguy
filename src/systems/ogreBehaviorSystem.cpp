#include "ogreBehaviorSystem.h"
#include "../behaviors/decisionTree.h"
#include "unordered_map"

struct OgreDecisionTree {
  int entity = -1;
  DecisionTreeRunner decisionRunner;

  void init(int ogreEntity) {}
  void tick() { decisionRunner.tick(); }
};

std::unordered_map<int, OgreDecisionTree> decisionTrees;

void initOgres() {}
void updateOgres() {}
