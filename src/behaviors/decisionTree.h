#pragma once

#include <functional>
#include <vector>

struct DecisionNode {
  DecisionNode *yes = nullptr;
  DecisionNode *no = nullptr;

  std::function<void()> execute = nullptr;
  std::vector<bool> conditions;
};

void evaluateDecisionTree(DecisionNode *node);
