#include "worldSystem.h"
#include <vector>

#include "../behaviors/wizardBehavior.h"

static std::vector<WizardBehavior> wizardBehaviors;
static bool wasBehaviorsInitialized = false;

WorldContext worldContext;

void updateBehaviors() {
  if (!wasBehaviorsInitialized) {
    for (Object3D &object : vulkanRendererContext.objects) {
      wizardBehaviors.push_back({});
    }
  }

  for (int i = 0; i < vulkanRendererContext.objects.size(); i++) {
    Object3D &object = vulkanRendererContext.objects[i];
    WizardBehavior &behavior = wizardBehaviors[i];

    switch (object.worldKind) {
    case ObjectWorldKind::None:
      break;
    case ObjectWorldKind::Floor:
      break;
    case ObjectWorldKind::Wizard:
      behaveLikeWizzard(object, behavior);
    }
  }
}
