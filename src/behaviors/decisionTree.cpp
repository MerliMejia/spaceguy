#include "decisionTree.h"

void evaluateDecisionTree(DecisionNode *node) {
  if (node == nullptr)
    return;

  if (node->yes == nullptr && node->no == nullptr) {
    if (node->execute)
      node->execute();
    return;
  }

  bool isYes = true;
  for (bool condition : node->conditions) {
    if (!condition) {
      isYes = false;
      break;
    }
  }

  evaluateDecisionTree(isYes ? node->yes : node->no);
}
