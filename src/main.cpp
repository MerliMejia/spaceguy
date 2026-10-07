
#include "../shaders/v2/banks_shared.h"
#include "engine/blender/v2/importer.h"
#include "engine/input/orbitCamera.h"
#include "engine/renderer/images/vTexture.h"
#include "engine/renderer/shaders/shaders.h"
#include "engine/renderer/vRenderer.h"
#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/vector_float3.hpp"
#include "systems/animationSystem.h"
#include "systems/resourceManagementSystem.h"
#include "systems/sceneContext.h"
#include "utils/generators.h"
#include "utils/math.h"
#include "utils/types.h"
#include <GLFW/glfw3.h>
#include <cstdlib>
#include <glm/ext/matrix_clip_space.hpp> // orthoRH_ZO
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp> // lookAt
#include <iostream>

struct SunMatrices {
  glm::mat4 view;
  glm::mat4 proj;
};

// Fits an orthographic shadow box around a bounding sphere.
// Only the direction from sunPosition to center is used, the sun's distance
// doesn't matter.
SunMatrices computeSunMatrices(glm::vec3 sunPosition, glm::vec3 center,
                               float radius, float margin) {
  glm::vec3 sunDir = glm::normalize(center - sunPosition);

  // Virtual eye just outside the sphere, on the sun's side.
  glm::vec3 eye = center - sunDir * (radius + margin);

  // Z-up world; fall back to Y-up if the sun is (almost) straight overhead.
  glm::vec3 up{0.0f, 0.0f, 1.0f};
  if (glm::abs(glm::dot(sunDir, up)) > 0.99f) {
    up = glm::vec3{0.0f, 1.0f, 0.0f};
  }

  SunMatrices m;
  m.view = glm::lookAt(eye, center, up);

  // Explicit right-handed, 0..1 depth: doesn't depend on
  // GLM_FORCE_DEPTH_ZERO_TO_ONE.
  float zNear = 0.0f;
  float zFar = 2.0f * (radius + margin);
  m.proj = glm::orthoRH_ZO(-radius, radius, -radius, radius, zNear, zFar);

  // Same Vulkan Y-flip as the camera.
  m.proj[1][1] *= -1.0f;

  return m;
}

int main() {
  try {
    Renderer::VRenderer renderer;
    auto worldData = Blender::V2::loadWorldData();

    Renderer::Types::Mesh floorMesh;
    Renderer::Images::VTexture *floorDiffuse;

    Renderer::Types::Mesh particleMesh;

    Renderer::Types::AnimatedMesh guyMesh;
    Renderer::Images::VTexture *guyDiffuse;
    BasicGameObject guyGameObject;
    BasicGameObject guyGameObject2;

    Input::Camera::OrbitCamera camera;

    renderer.onInit = [&]() {
      sceneContext.cameraPosition = worldData.camera.transform.position;
      sceneContext.cameraLookAt = worldData.camera.direction;
      sceneContext.cameraFovY = worldData.camera.fovY;
      sceneContext.cameraClipStart = worldData.camera.clipStart;
      sceneContext.cameraClipEnd = worldData.camera.clipEnd;

      camera.position = worldData.camera.transform.position;
      camera.direction = worldData.camera.direction;
      camera.fovY = worldData.camera.fovY;
      camera.clipStart = worldData.camera.clipStart;
      camera.clipEnd = worldData.camera.clipEnd;

      camera.init(renderer.window.handler);

      glm::vec3 sunColorIntensity = glm::vec3{0.6f, 0.6f, 0.6f};
      glm::vec3 sunPosition = glm::vec3(10.7f, -24.6f, 17.0f);

      auto &uniformsBank =
          renderer.renderGraph.context.globalUniformBufferData.data;
      auto &pushConstantsBank = renderer.renderGraph.context.pushConstantBank;

      camera.update(renderer.vSwapChain.swapChainExtent.width,
                    renderer.vSwapChain.swapChainExtent.height, uniformsBank,
                    renderer.window.handler);

      SunMatrices sun = computeSunMatrices(
          sunPosition, glm::vec3{0.0f, 0.0f, 3.5f}, // sphere center
          29.0f,                                    // R
          5.0f);                                    // margin

      Renderer::Shaders::UniformBank::setFloat3(uniformsBank, SG_SUN_POS_INDEX,
                                                sunPosition);
      Renderer::Shaders::UniformBank::setFloat3(
          uniformsBank, SG_SUN_INTENSITY_INDEX, sunColorIntensity);
      Renderer::Shaders::UniformBank::setFloat4x4(uniformsBank,
                                                  SG_SUN_VIEW_INDEX, sun.view);
      Renderer::Shaders::UniformBank::setFloat4x4(uniformsBank,
                                                  SG_SUN_PROJ_INDEX, sun.proj);

      Blender::V2::BlenderModel guyModel =
          loadAnimatedModel("assets/guy_v2.3d");

      guyDiffuse = renderer.renderGraph.vTextureManager.createTexture(
          guyModel.texturePath.string(), renderer.vDevice,
          renderer.renderGraph.commandPool, renderer.vDevice.graphicsQueue);

      Transform guyTransform;
      guyTransform.position = glm::vec3{0.0f, 0.0f, 3.0f};
      guyTransform.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
      guyTransform.scale = glm::vec3(1.0f);

      Transform guy2Transform;
      guy2Transform.position = glm::vec3{5.0f, 5.0f, 3.0f};
      guy2Transform.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
      guy2Transform.scale = glm::vec3(1.0f);

      guyGameObject = createAnimatedGameObject(
          guyModel, guyMesh, guyModel.globalPositionOffset, guyTransform,
          uniformsBank, renderer.renderGraph.commandPool, renderer.vDevice,
          guyDiffuse->index);

      guyGameObject2 = createAnimatedGameObject(
          guyModel, guyMesh, guyModel.globalPositionOffset * 2, guy2Transform,
          uniformsBank, renderer.renderGraph.commandPool, renderer.vDevice,
          guyDiffuse->index);

      Blender::V2::BlenderModel floorModel =
          Blender::V2::loadModel("assets/floor_v2.3d");

      floorDiffuse = renderer.renderGraph.vTextureManager.createTexture(
          floorModel.texturePath.string(), renderer.vDevice,
          renderer.renderGraph.commandPool, renderer.vDevice.graphicsQueue);

      glm::quat floorOrientation{glm::radians(worldData.floor.rotation)};
      glm::mat4 floorModelMatrix = transformToModel(
          worldData.floor.position, floorOrientation, worldData.floor.scale);

      Transform tc = modelToTransform(floorModelMatrix);

      createBasicGameObject(floorModel, floorMesh, tc, uniformsBank,
                            renderer.renderGraph.commandPool, renderer.vDevice,
                            floorDiffuse->index);

      Blender::V2::BlenderModel particleModel =
          Blender::V2::loadModel("assets/particle_v2.3d");

      particleMesh = Renderer::Generators::generateMesh(
          particleModel.vertices, particleModel.indices,
          renderer.renderGraph.commandPool, renderer.vDevice);

      Transform peTransform;
      peTransform.position = glm::vec3{0.0f, 0.0f, 0.0f};
      peTransform.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
      peTransform.scale = glm::vec3(1.0f);

      int peEntity = createEntity();
      ParticleEmitterComponent &particleEmitter =
          addParticleEmitterComponent(peEntity);

      particleEmitter.particleMesh = &particleMesh;

      TransformComponent &peTc = addTransform(peEntity);
      peTc.model = transformToModel(peTransform.position, peTransform.rotation,
                                    peTransform.scale);

      Renderer::Shaders::UniformBank::setFloat4x4(uniformsBank, peTc.modelIndex,
                                                  peTc.model);

      initAnimations(renderer.renderGraph.context.vertAnimSSBankData,
                     renderer.renderGraph.context.vertAnimSBBankAllocations,
                     renderer.vDevice, renderer.renderGraph.commandPool);
    };

    bool isKeyPressed = false;

    renderer.onUpdate = [&]() {
      auto &uniformsBank =
          renderer.renderGraph.context.globalUniformBufferData.data;
      auto &pushConstantsBank = renderer.renderGraph.context.pushConstantBank;

      if (glfwGetKey(renderer.window.handler, GLFW_KEY_RIGHT) == GLFW_PRESS) {
        if (!isKeyPressed) {
          auto &animation = getAnimation(guyGameObject.entity);
          if (animation.activeAnimation >= 3) {
            animation.activeAnimation = 0;
          } else {
            animation.activeAnimation++;
          }
          isKeyPressed = true;
        }
      } else {
        isKeyPressed = false;
      }

      camera.update(renderer.vSwapChain.swapChainExtent.width,
                    renderer.vSwapChain.swapChainExtent.height, uniformsBank,
                    renderer.window.handler);

      updateAnimations();
    };

    renderer.run();
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
