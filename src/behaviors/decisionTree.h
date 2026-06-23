#pragma once

#include <functional>

struct DecisionNode {
  DecisionNode *yes = nullptr;
  DecisionNode *no = nullptr;
  DecisionNode *next = nullptr;

  std::function<void()> execute = nullptr;
  std::function<bool()> conditions;
};

void evaluateDecisionTree(DecisionNode *node);
