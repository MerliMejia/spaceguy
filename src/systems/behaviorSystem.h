#pragma once

#include "../behaviors/wizards/wizardBehavior.h"
#include <unordered_map>
#include <vector>

struct WizardShootEffect {
  int wizardEntity;
  int effectEntity;
};

struct BehaviorContext {
  std::unordered_map<int, WizardBehavior> wizardBehaviorsByEntity;
  std::unordered_map<int, int> wizardAttacking;
  std::vector<WizardShootEffect> wizardShootingEffects;
};

extern BehaviorContext behaviorContext;

bool isWizardBehaviorEntity(int entity);
bool isVisibleWizardBehaviorEntity(int entity);
std::vector<int> getVisibleWizardBehaviorEntities();
void clearWizardAttackReferences(int entity);

void addWizardShootingEffect(int wizardEntity, int effectEntity);
void initializeBehaviorSystem();
void updateBehaviorSystem();
void updateWizardEffects();
