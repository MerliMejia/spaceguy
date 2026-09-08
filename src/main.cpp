
#include "../shaders/v2/banks_shared.h"
#include "engine/blender/importer.h"
#include "engine/blender/v2/importer.h"
#include "engine/predefined/vulkanGraphicPipelines.h"
#include "engine/renderer/images/vTexture.h"
#include "engine/renderer/shaders/shaders.h"
#include "engine/renderer/vRenderer.h"
#include "glm/ext/quaternion_transform.hpp"
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
    auto worldData = loadWorldData();

    Renderer::Types::Mesh floorMesh;
    Renderer::Images::VTexture *floorDiffuse;
    // Renderer::Types::Mesh floorDetailsMesh;
    Renderer::Types::Mesh wizardMesh;
    Renderer::Images::VTexture *wizzardDiffuse;
    Renderer::Types::Mesh ogreMesh;
    Renderer::Images::VTexture *ogreDiffuse;

    renderer.onInit = [&worldData, &renderer, &ogreMesh, &ogreDiffuse,
                       &wizardMesh, &wizzardDiffuse, &floorMesh,
                       &floorDiffuse]() {
      sceneContext.cameraPosition = worldData.camera.transform.position;
      sceneContext.cameraLookAt = worldData.camera.direction;
      sceneContext.cameraFovY = worldData.camera.fovY;
      sceneContext.cameraClipStart = worldData.camera.clipStart;
      sceneContext.cameraClipEnd = worldData.camera.clipEnd;

      SceneBufferObject scene{.sunColorIntensity =
                                  glm::vec4{0.8f, 0.8f, 0.8f, 0.8f}};

      scene.view = glm::lookAt(worldData.camera.transform.position,
                               worldData.camera.transform.position +
                                   worldData.camera.direction,
                               glm::vec3{0.0f, 0.0f, 1.0f});

      scene.proj = glm::perspective(
          worldData.camera.fovY,
          static_cast<float>(renderer.vSwapChain.swapChainExtent.width) /
              static_cast<float>(renderer.vSwapChain.swapChainExtent.height),
          worldData.camera.clipStart, worldData.camera.clipEnd);

      scene.proj[1][1] *= -1.0f;
      scene.viewPosition = glm::vec4(worldData.camera.transform.position, 1.0f);

      auto &uniformsBank =
          renderer.renderGraph.context.globalUniformBufferData.data;
      auto &pushConstantsBank = renderer.renderGraph.context.pushConstantBank;

      Renderer::Shaders::UniformBank::setFloat4x4(uniformsBank, SG_VIEW_INDEX,
                                                  scene.view);
      Renderer::Shaders::UniformBank::setFloat4x4(uniformsBank, SG_PROJ_INDEX,
                                                  scene.proj);
      Renderer::Shaders::UniformBank::setFloat4(uniformsBank, SG_VIEW_POS_INDEX,
                                                scene.viewPosition);
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

    renderer.run();
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
