#include "glm/ext/matrix_transform.hpp"
#include "glm/ext/vector_float3.hpp"
#include "glm/fwd.hpp"
#include "glm/trigonometric.hpp"
#include "utils/math.h"
#include "utils/types.h"
#include <GLFW/glfw3.h>
#include <cstdint>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>

#include "engine/blender/importer.h"
#include "engine/vulkanBackend.h"
#include "engine/vulkanRenderer.h"
#include "systems/animationSystem.h"
#include "systems/lightsSystem.h"
#include "systems/particleLifeSystem.h"
#include "systems/projectileSystem.h"
#include "systems/resourceManagementSystem.h"
#include "systems/sceneContext.h"
#include "systems/spacialGridHashSystem.h"
#include "systems/wizardBehaviorSystem.h"
#include "utils/generators.h"
#include "utils/textMeshGenerator.h"
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
  Mesh floorDetailsMesh;
  Mesh waterMesh;
  AnimatedMesh wizardAnimatedMesh;
  AnimatedMesh ogreMesh;
  Mesh ogreBladeMesh;

  TextMeshGenerator textGenerator{"assets/fonts/Roboto-Regular.ttf"};
  Mesh testTextMesh = textGenerator.generateMesh("Hello World IOB8", 4.0f,
                                                 glm::vec3{1.0f, 1.0f, 1.0f});

  {
    sceneContext.cameraPosition = worldData.camera.transform.position;
    sceneContext.cameraLookAt = worldData.camera.direction;
    sceneContext.cameraFovY = worldData.camera.fovY;
    sceneContext.cameraClipStart = worldData.camera.clipStart;
    sceneContext.cameraClipEnd = worldData.camera.clipEnd;

    BlenderModel floorModel = loadModel("assets/floor.3d");
    floorMesh = generateMesh(floorModel.vertices, floorModel.indices);

    BlenderModel floorDetailModel = loadModel("assets/floor_details.3d");
    floorDetailsMesh =
        generateMesh(floorDetailModel.vertices, floorDetailModel.indices);

    BlenderModel waterModel = loadModel("assets/water.3d");
    waterMesh = generateMesh(waterModel.vertices, waterModel.indices);

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

    int floorDetailsEntity = createEntity();
    Renderable &fdRenderable = addRenderable(floorDetailsEntity);
    fdRenderable.renderKind = ObjectRenderKind::Static;
    fdRenderable.mesh = &floorDetailsMesh;
    addTransform(floorDetailsEntity);

    int waterEntity = createEntity();
    Renderable &wRenderable = addRenderable(waterEntity);
    wRenderable.renderKind = ObjectRenderKind::Static;
    wRenderable.mesh = &waterMesh;
    addTransform(waterEntity);

    std::vector<glm::vec4> animationPositions;

    BlenderModel wizardModel = loadModel("assets/Wizzard_4.3d");
    BlenderModel ogreModel = loadModel("assets/Ogre.3d");
    BlenderModel ogreBladeModel = loadModel("assets/Ogre_blade.3d");

    int wizardAnimationPositionCount = 0;

    for (const AnimationClip &clip : wizardModel.animations) {
      if (clip.kind != AnimationKind::Vertex) {
        continue;
      }

      for (const AnimationKeyPose &keyPoses : clip.keyPoses) {
        wizardAnimationPositionCount +=
            static_cast<uint32_t>(keyPoses.positions.size());
        for (const glm::vec3 &pos : keyPoses.positions) {
          animationPositions.push_back(glm::vec4(pos, 1.0f));
        }
      }
    }

    for (const AnimationClip &clip : ogreModel.animations) {
      if (clip.kind != AnimationKind::Vertex) {
        continue;
      }

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

    for (const auto &ogrePos : worldData.ogres.positions) {
      ogreMesh = generateAnimatedMesh(ogreModel, wizardAnimationPositionCount);
      ogreBladeMesh =
          generateMesh(ogreBladeModel.vertices, ogreBladeModel.indices);

      int ogreEntity = createEntity();
      Renderable &ogreRenderable = addComponent<Renderable>(ogreEntity);
      ogreRenderable.renderKind = ObjectRenderKind::Animated;
      ogreRenderable.animatedMesh = &ogreMesh;
      TransformComponent &ogreTransformComponent = addTransform(ogreEntity);

      Transform ot = modelToTransform(ogreTransformComponent.model);
      ot.position = ogrePos;

      ogreTransformComponent.model =
          transformToModel(ot.position, ot.rotation, ot.scale);

      AnimationComponent &ogreAnimation = addAnimation(ogreEntity);
      ogreAnimation.activeAnimation = OgreAnimationMapping::OgreIddle;

      int ogreBladeEntity = createEntity();
      Renderable &obRenderable = addRenderable(ogreBladeEntity);
      obRenderable.mesh = &ogreBladeMesh;
      obRenderable.renderKind = ObjectRenderKind::Static;

      addTransform(ogreBladeEntity);

      AttachmentAnimationComponent &attachmentComponent =
          addAttachmentAnimationComponent(ogreBladeEntity);
      attachmentComponent.parentEntity = ogreEntity;
      attachmentComponent.attachmentIndex = 0;
    }
  }

  initWizardBehaviors();
  initializeProjectiles();
  initSpacialGridHash();

  int sunEntity = createEntity();
  SunLightComponent &sun = addSunLight(sunEntity);
  sun.direction = glm::normalize(glm::vec3{0.0f, 0.0f, 1.0f});
  sun.color = glm::vec3{1.0f, 1.0f, 1.0f};
  sun.intensity = 0.9f;

  int testTextEntity = createEntity();
  Renderable &ttRenderable = addRenderable(testTextEntity);
  ttRenderable.renderKind = ObjectRenderKind::Static;
  ttRenderable.mesh = &testTextMesh;

  TransformComponent &tttc = addTransform(testTextEntity);
  Transform tttt = modelToTransform(tttc.model);
  tttt.position = glm::vec3{-10.0f, -10.0f, 2.0f};
  tttc.model = transformToModel(tttt.position, tttt.rotation, tttt.scale);

  while (!glfwWindowShouldClose(vulkanContext.window)) {
    glfwPollEvents();
    updateTime();
    clearDebugShapes();
    updateLightsSystem();
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
