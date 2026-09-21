
#include "../shaders/v2/banks_shared.h"
#include "engine/blender/v2/importer.h"
#include "engine/input/orbitCamera.h"
#include "engine/renderer/images/vTexture.h"
#include "engine/renderer/vRenderer.h"
#include "glm/ext/vector_float3.hpp"
#include "systems/animationSystem.h"
#include "systems/resourceManagementSystem.h"
#include "systems/sceneContext.h"
#include "utils/types.h"
#include <GLFW/glfw3.h>
#include <cstdlib>
#include <iostream>

int main() {
  try {
    Renderer::VRenderer renderer;
    auto worldData = Blender::V2::loadWorldData();

    Renderer::Types::Mesh floorMesh;
    Renderer::Images::VTexture *floorDiffuse;

    Renderer::Types::AnimatedMesh guyMesh;
    Renderer::Images::VTexture *guyDiffuse;
    BasicGameObject guyGameObject;

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

      glm::vec4 sunColorIntensity = glm::vec4{0.8f, 0.8f, 0.8f, 0.8f};
      glm::vec4 sunDirection = glm::vec4(0.0f, 0.0f, -1.0f, 0.0f);

      auto &uniformsBank =
          renderer.renderGraph.context.globalUniformBufferData.data;
      auto &pushConstantsBank = renderer.renderGraph.context.pushConstantBank;

      camera.update(renderer.vSwapChain.swapChainExtent.width,
                    renderer.vSwapChain.swapChainExtent.height, uniformsBank,
                    renderer.window.handler);

      Renderer::Shaders::UniformBank::setFloat4(uniformsBank, SG_SUN_DIR_INDEX,
                                                sunDirection);
      Renderer::Shaders::UniformBank::setFloat4(
          uniformsBank, SG_SUN_INTENSITY_INDEX, sunColorIntensity);

      Blender::V2::BlenderModel guyModel =
          loadAnimatedModel("assets/guy_v2.3d");

      guyDiffuse = renderer.vTextureManager.createTexture(
          guyModel.texturePath.string(), renderer.vDevice,
          renderer.renderGraph.commandPool, renderer.vDevice.graphicsQueue);

      Transform guyTransform;
      guyTransform.position = glm::vec3{0.0f, 0.0f, 5.0f};
      guyTransform.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
      guyTransform.scale = glm::vec3(1.0f);

      guyGameObject = createAnimatedGameObject(
          guyModel, guyMesh, guyModel.globalPositionOffset, guyTransform,
          uniformsBank, renderer.renderGraph.commandPool, renderer.vDevice,
          guyDiffuse->index);

      Blender::V2::BlenderModel floorModel =
          Blender::V2::loadModel("assets/floor_v2.3d");

      floorDiffuse = renderer.vTextureManager.createTexture(
          floorModel.texturePath.string(), renderer.vDevice,
          renderer.renderGraph.commandPool, renderer.vDevice.graphicsQueue);

      glm::quat floorOrientation{glm::radians(worldData.floor.rotation)};
      glm::mat4 floorModelMatrix = transformToModel(
          worldData.floor.position, floorOrientation, worldData.floor.scale);

      Transform tc = modelToTransform(floorModelMatrix);

      createBasicGameObject(floorModel, floorMesh, tc, uniformsBank,
                            renderer.renderGraph.commandPool, renderer.vDevice,
                            floorDiffuse->index);
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
