#pragma once

#include <functional>

enum class DecisionStatus { Running, Done };

struct DecisionNode {
  DecisionNode *yes = nullptr;
  DecisionNode *no = nullptr;
  DecisionNode *next = nullptr;

  std::function<DecisionStatus()> execute = nullptr;
  std::function<bool()> conditions;
};

struct DecisionTreeRunner {
  DecisionNode *root = nullptr;
  DecisionNode *current = nullptr;

  void reset(DecisionNode *node) {
    root = node;
    current = node;
  }

  void tick() {
    if (current == nullptr) {
      current = root;
    }

    DecisionNode *node = current;
    if (node == nullptr) {
      return;
    }

    if (node->execute) {
      DecisionStatus status = node->execute();

      if (status == DecisionStatus::Done) {
        current = node->next;
      }

      return;
    }

    if (node->conditions) {
      bool isYes = node->conditions();
      current = isYes ? node->yes : node->no;
      return;
    }

    current = node->next;
  }
};
