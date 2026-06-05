#include "decisionTree.h"

void evaluateDecisionTree(DecisionNode *node) {
  if (node == nullptr)
    return;

  if (node->yes == nullptr && node->no == nullptr) {
    if (node->execute)
      node->execute();
    return;
  }

  bool isYes = false;

  if (node->conditions) {
    isYes = node->conditions();
  }

  evaluateDecisionTree(isYes ? node->yes : node->no);
}
