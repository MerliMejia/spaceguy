#include "worldSystem.h"
#include <vector>

#include "../behaviors/wizardBehavior.h"

static std::vector<WizardBehavior> wizardBehaviors;

WorldContext worldContext;

void initializeBehaviors() {
  std::size_t wizardCount = 0;

  for (const Object3D &object : vulkanRendererContext.objects) {
    if (object.worldKind == ObjectWorldKind::Wizard) {
      wizardCount++;
    }
  }

  wizardBehaviors.reserve(wizardCount);

  for (Object3D &object : vulkanRendererContext.objects) {
    if (object.worldKind != ObjectWorldKind::Wizard) {
      continue;
    }

    WizardBehavior &behavior = wizardBehaviors.emplace_back();
    initializeWizardDecisionTree(object, behavior);
  }
}

void updateBehaviors() {
  std::size_t wizardIndex = 0;
  for (int i = 0; i < vulkanRendererContext.objects.size(); i++) {
    Object3D &object = vulkanRendererContext.objects[i];

    switch (object.worldKind) {
    case ObjectWorldKind::None:
      break;
    case ObjectWorldKind::Floor:
      break;
    case ObjectWorldKind::Wizard: {
      WizardBehavior &behavior = wizardBehaviors[wizardIndex++];
      behaveLikeWizzard(object, behavior);
    }
    }
  }
}
