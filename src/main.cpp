#include "glm/fwd.hpp"
#include "utils/types.h"
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>

#include "engine/blender/importer.h"
#include "engine/vulkanBackend.h"
#include "engine/vulkanRenderer.h"
#include "systems/animationSystem.h"
#include "systems/particleLifeSystem.h"
#include "systems/projectileSystem.h"
#include "systems/resourceManagementSystem.h"
#include "systems/sceneContext.h"
#include "systems/spacialGridHashSystem.h"
#include "systems/wizardBehaviorSystem.h"
#include "utils/generators.h"
#include "utils/time.h"

void cleanup() {
  vulkanContext.device.waitIdle();

  // since the device is in a global object, we need to manually clear?
  vulkanRendererContext.inFlightFences.clear();
  vulkanRendererContext.imageAvailableSemaphores.clear();
  vulkanRendererContext.renderFinishedSemaphores.clear();

  glfwDestroyWindow(vulkanContext.window);
  glfwTerminate();
}

int main() {
  setupVulkan();

  setupRendererCore();

  std::cout << "Spaceguy running\n";

  std::cout << "Loading world data...\n";
  auto worldData = loadWorldData();

  Mesh floorMesh;
  AnimatedMesh wizardAnimatedMesh;

  {
    sceneContext.cameraPosition = worldData.camera.transform.position;
    sceneContext.cameraLookAt = worldData.camera.direction;
    sceneContext.cameraFovY = worldData.camera.fovY;
    sceneContext.cameraClipStart = worldData.camera.clipStart;
    sceneContext.cameraClipEnd = worldData.camera.clipEnd;

    BlenderModel floorModel = loadModel("assets/floor.3d");
    floorMesh = generateMesh(floorModel.vertices, floorModel.indices);

    glm::vec3 rotation = glm::radians(worldData.floor.rotation);

    glm::mat4 model{1.0f};
    model = glm::translate(model, worldData.floor.position);
    model = glm::rotate(model, rotation.x, glm::vec3{1.0f, 0.0f, 0.0f});
    model = glm::rotate(model, rotation.y, glm::vec3{0.0f, 1.0f, 0.0f});
    model = glm::rotate(model, rotation.z, glm::vec3{0.0f, 0.0f, 1.0f});
    model = glm::scale(model, worldData.floor.scale);

    int floorEntity = createEntity();
    Renderable &floorRenderable = addComponent<Renderable>(floorEntity);
    floorRenderable.mesh = &floorMesh;
    floorRenderable.renderKind = ObjectRenderKind::Static;

    TransformComponent &floorTransform = addTransform(floorEntity);
    floorTransform.model = model;

    std::vector<glm::vec4> animationPositions;

    BlenderModel wizardModel = loadModel("assets/Wizzard_4.3d");

    for (const AnimationClip &clip : wizardModel.animations) {
      for (const AnimationKeyPose &keyPoses : clip.keyPoses) {
        for (const glm::vec3 &pos : keyPoses.positions) {
          animationPositions.push_back(glm::vec4(pos, 1.0f));
        }
      }
    }

    uploadAnimationPositions(animationPositions);

    setupRendererAfterAssetsLoaded();

    wizardAnimatedMesh = generateAnimatedMesh(wizardModel, 0);

    BlenderTransformModel wizardShootTransformModel =
        loadTransformModel("assets/Wizard_Shooting_Effect_1.3d");

    for (const glm::vec3 &wizardPosition : worldData.wizards.positions) {
      glm::mat4 wizardModelMatrix{1.0f};
      wizardModelMatrix = glm::translate(wizardModelMatrix, wizardPosition);

      int wizardEntity = createEntity();
      Renderable &wizardRenderable = addComponent<Renderable>(wizardEntity);
      wizardRenderable.renderKind = ObjectRenderKind::Animated;
      wizardRenderable.animatedMesh = &wizardAnimatedMesh;

      TransformComponent &wizardTransform = addTransform(wizardEntity);
      wizardTransform.model = wizardModelMatrix;

      AnimationComponent &wizardAnimation = addAnimation(wizardEntity);
      wizardAnimation.activeAnimation = WizardAnimationMapping::Iddle;

      addWizardBehavior(wizardEntity);
    }
  }

  initWizardBehaviors();
  initializeProjectiles();
  initSpacialGridHash();

  while (!glfwWindowShouldClose(vulkanContext.window)) {
    glfwPollEvents();
    updateTime();
    clearDebugShapes();
    updateSpacialGridHash();
    updateProjectiles();
    updateWizardBehaviors();
    updateAnimations();
    drawFrame();
    processDestroyQueue();
    updateParticleEmittersToBeDestroyed();
  }

  cleanup();

  return 0;
}
