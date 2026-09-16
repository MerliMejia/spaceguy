
#include "../shaders/v2/banks_shared.h"
#include "engine/blender/v2/importer.h"
#include "engine/input/orbitCamera.h"
#include "engine/predefined/vulkanGraphicPipelines.h"
#include "engine/renderer/images/vTexture.h"
#include "engine/renderer/shaders/shaders.h"
#include "engine/renderer/vRenderer.h"
#include "glm/fwd.hpp"
#include "systems/resourceManagementSystem.h"
#include "systems/sceneContext.h"
#include "utils/math.h"
#include "utils/types.h"
#include <cstdlib>
#include <iostream>

int main() {
  try {
    Renderer::VRenderer renderer;
    auto worldData = Blender::V2::loadWorldData();

    Renderer::Types::Mesh floorMesh;
    Renderer::Images::VTexture *floorDiffuse;
    Renderer::Types::Mesh wizardMesh;
    Renderer::Images::VTexture *wizzardDiffuse;
    Renderer::Types::Mesh ogreMesh;
    Renderer::Images::VTexture *ogreDiffuse;

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

      SceneBufferObject scene{.sunColorIntensity =
                                  glm::vec4{0.8f, 0.8f, 0.8f, 0.8f}};

      auto &uniformsBank =
          renderer.renderGraph.context.globalUniformBufferData.data;
      auto &pushConstantsBank = renderer.renderGraph.context.pushConstantBank;

      camera.update(renderer.vSwapChain.swapChainExtent.width,
                    renderer.vSwapChain.swapChainExtent.height, uniformsBank,
                    renderer.window.handler);

      Renderer::Shaders::UniformBank::setFloat4(uniformsBank, SG_SUN_DIR_INDEX,
                                                scene.sunDirection);
      Renderer::Shaders::UniformBank::setFloat4(
          uniformsBank, SG_SUN_INTENSITY_INDEX, scene.sunColorIntensity);

      Blender::V2::BlenderModel wizardModel =
          Blender::V2::loadModel("assets/Wizzard_4_v2.3d");

      wizzardDiffuse = renderer.vTextureManager.createTexture(
          wizardModel.texturePath, renderer.vDevice,
          renderer.renderGraph.commandPool, renderer.vDevice.graphicsQueue);

      for (const glm::vec3 &wizardPosition : worldData.wizards.positions) {
        glm::mat4 wizardModelMatrix{1.0f};
        wizardModelMatrix = glm::translate(wizardModelMatrix, wizardPosition);

        Transform tc = modelToTransform(wizardModelMatrix);

        createBasicGameObject(wizardModel, wizardMesh, tc, uniformsBank,
                              pushConstantsBank,
                              renderer.renderGraph.commandPool,
                              renderer.vDevice, wizzardDiffuse->index);
      }

      Blender::V2::BlenderModel ogreModel =
          Blender::V2::loadModel("assets/Ogre_v2.3d");

      ogreDiffuse = renderer.vTextureManager.createTexture(
          ogreModel.texturePath, renderer.vDevice,
          renderer.renderGraph.commandPool, renderer.vDevice.graphicsQueue);

      for (const auto &ogrePos : worldData.ogres.positions) {
        glm::mat4 ogreMat4Model = glm::mat4(1.0f);

        Transform ot = modelToTransform(ogreMat4Model);
        ot.position = ogrePos;

        ogreMat4Model = transformToModel(ot.position, ot.rotation, ot.scale);

        createBasicGameObject(ogreModel, ogreMesh, ot, uniformsBank,
                              pushConstantsBank,
                              renderer.renderGraph.commandPool,
                              renderer.vDevice, ogreDiffuse->index);
      }

      Blender::V2::BlenderModel floorModel =
          Blender::V2::loadModel("assets/floor_v2.3d");

      floorDiffuse = renderer.vTextureManager.createTexture(
          floorModel.texturePath, renderer.vDevice,
          renderer.renderGraph.commandPool, renderer.vDevice.graphicsQueue);

      glm::quat floorOrientation{glm::radians(worldData.floor.rotation)};
      glm::mat4 floorModelMatrix = transformToModel(
          worldData.floor.position, floorOrientation, worldData.floor.scale);

      Transform tc = modelToTransform(floorModelMatrix);

      createBasicGameObject(floorModel, floorMesh, tc, uniformsBank,
                            pushConstantsBank, renderer.renderGraph.commandPool,
                            renderer.vDevice, floorDiffuse->index);
    };

    renderer.onUpdate = [&]() {
      auto &uniformsBank =
          renderer.renderGraph.context.globalUniformBufferData.data;
      auto &pushConstantsBank = renderer.renderGraph.context.pushConstantBank;

      camera.update(renderer.vSwapChain.swapChainExtent.width,
                    renderer.vSwapChain.swapChainExtent.height, uniformsBank,
                    renderer.window.handler);
    };

    renderer.run();
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
