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
#include "systems/worldSystem.h"
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
  AnimatedMesh animatedMesh;

  {
    worldContext.cameraPosition = worldData.camera.transform.position;
    worldContext.cameraLookAt = worldData.camera.direction;
    worldContext.cameraFovY = worldData.camera.fovY;
    worldContext.cameraClipStart = worldData.camera.clipStart;
    worldContext.cameraClipEnd = worldData.camera.clipEnd;

    BlenderModel floorModel = loadModel("assets/floor.3d");
    floorMesh = generateMesh(floorModel.vertices, floorModel.indices);

    glm::vec3 rotation = glm::radians(worldData.floor.rotation);

    glm::mat4 model{1.0f};
    model = glm::translate(model, worldData.floor.position);
    model = glm::rotate(model, rotation.x, glm::vec3{1.0f, 0.0f, 0.0f});
    model = glm::rotate(model, rotation.y, glm::vec3{0.0f, 1.0f, 0.0f});
    model = glm::rotate(model, rotation.z, glm::vec3{0.0f, 0.0f, 1.0f});
    model = glm::scale(model, worldData.floor.scale);

    vulkanRendererContext.objects.push_back(
        Object3D{.mesh = &floorMesh,
                 .renderKind = ObjectRenderKind::Static,
                 .worldKind = ObjectWorldKind::Floor,
                 .model = model});

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

    animatedMesh = generateAnimatedMesh(wizardModel, 0);

    for (const glm::vec3 &wizardPosition : worldData.wizards.positions) {
      glm::mat4 wizardModelMatrix{1.0f};
      wizardModelMatrix = glm::translate(wizardModelMatrix, wizardPosition);

      vulkanRendererContext.objects.push_back(Object3D{
          .renderKind = ObjectRenderKind::Animated,
          .worldKind = ObjectWorldKind::Wizard,
          .mesh = nullptr,
          .animatedMesh = &animatedMesh,
          .model = wizardModelMatrix,
          .activeAnimation = WizardAnimationMapping::Iddle,
          .activeFrame = 0,
      });
    }
  }

  initializeBehaviors();

  while (!glfwWindowShouldClose(vulkanContext.window)) {
    glfwPollEvents();
    updateTime();
    if (vulkanRendererContext.isDebug) {
      clearDebugShapes();
    }
    updateBehaviors();
    updateAnimations();
    drawFrame();
  }

  cleanup();

  return 0;
}
