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

void positionTextBottomLeft(TransformComponent &transform) {
  constexpr float distance = 1.0f;
  constexpr float textScale = 0.15f;
  constexpr float marginFraction = 0.025f;

  const glm::vec3 worldUp{0.0f, 0.0f, 1.0f};
  const glm::vec3 forward = glm::normalize(sceneContext.cameraLookAt);
  const glm::vec3 right = glm::normalize(glm::cross(forward, worldUp));
  const glm::vec3 up = glm::normalize(glm::cross(right, forward));

  const float aspect = static_cast<float>(vulkanContext.swapchainExtent.width) /
                       static_cast<float>(vulkanContext.swapchainExtent.height);

  const float halfHeight = distance * glm::tan(sceneContext.cameraFovY * 0.5f);

  const float halfWidth = halfHeight * aspect;
  const float margin = halfHeight * marginFraction;

  const glm::vec3 position = sceneContext.cameraPosition + forward * distance -
                             right * (halfWidth - margin) -
                             up * (halfHeight - margin);

  const glm::quat rotation = glm::quat_cast(glm::mat3{right, up, -forward});

  transform.model = transformToModel(position, rotation, glm::vec3{textScale});
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

  TextMeshGenerator textGenerator{"assets/fonts/Roboto-Regular.ttf"};

  Mesh fpsTextMesh =
      textGenerator.generateMesh("FPS: 0", 1.0f, glm::vec3{1.0f});

  int fpsTextEntity = createEntity();

  Renderable &fpsRenderable = addRenderable(fpsTextEntity);
  fpsRenderable.renderKind = ObjectRenderKind::Static;
  fpsRenderable.mesh = &fpsTextMesh;
  fpsRenderable.unlit = true;

  TransformComponent &fpsTransform = addTransform(fpsTextEntity);

  float fpsElapsedTime = 0.0f;
  int frameCounter = 0;

  while (!glfwWindowShouldClose(vulkanContext.window)) {
    glfwPollEvents();
    updateTime();
    fpsElapsedTime += timeState.deltaTime;
    frameCounter++;

    if (fpsElapsedTime >= 0.25f) {
      const int fps =
          static_cast<int>(static_cast<float>(frameCounter) / fpsElapsedTime);

      vulkanContext.device.waitIdle();

      fpsTextMesh = textGenerator.generateMesh("FPS: " + std::to_string(fps),
                                               0.25f, glm::vec3{1.0f});

      fpsRenderable.mesh = &fpsTextMesh;

      frameCounter = 0;
      fpsElapsedTime = 0.0f;
    }
    positionTextBottomLeft(fpsTransform);
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
