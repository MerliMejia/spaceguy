
#include "../shaders/v2/banks_shared.h"
#include "engine/blender/importer.h"
#include "engine/predefined/vulkanGraphicPipelines.h"
#include "engine/renderer/shaders/shaders.h"
#include "engine/renderer/vRenderer.h"
#include "glm/fwd.hpp"
#include "systems/resourceManagementSystem.h"
#include "systems/sceneContext.h"
#include "utils/generators.h"
#include "utils/math.h"
#include "utils/types.h"
#include <cstdlib>
#include <iostream>

int main() {
  try {
    Renderer::VRenderer renderer;
    auto worldData = loadWorldData();

    Renderer::Types::Mesh floorMesh;
    Renderer::Types::Mesh floorDetailsMesh;
    Renderer::Types::Mesh wizardMesh;
    Renderer::Types::Mesh ogreMesh;

    renderer.onInit = [&worldData, &renderer, &floorMesh, &floorDetailsMesh,
                       &wizardMesh, &ogreMesh]() {
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

      BlenderModel floorModel = loadModel("assets/floor.3d");
      Transform floorT = Transform{
          .position = worldData.floor.position,
          .scale = worldData.floor.scale,
          .rotation = worldData.floor.rotation,
      };
      createBasicGameObject(floorModel, floorMesh, floorT, uniformsBank,
                            pushConstantsBank, renderer.renderGraph.commandPool,
                            renderer.vDevice);

      BlenderModel floorDetailModel = loadModel("assets/floor_details.3d");
      Transform floorDetailsT = Transform{
          .position = worldData.floor_details.position,
          .scale = worldData.floor_details.scale,
          .rotation = worldData.floor_details.rotation,
      };
      createBasicGameObject(floorDetailModel, floorDetailsMesh, floorDetailsT,
                            uniformsBank, pushConstantsBank,
                            renderer.renderGraph.commandPool, renderer.vDevice);

      BlenderModel wizardModel = loadModel("assets/Wizzard_4.3d");

      for (const glm::vec3 &wizardPosition : worldData.wizards.positions) {
        glm::mat4 wizardModelMatrix{1.0f};
        wizardModelMatrix = glm::translate(wizardModelMatrix, wizardPosition);

        Transform tc = modelToTransform(wizardModelMatrix);

        createBasicGameObject(
            wizardModel, wizardMesh, tc, uniformsBank, pushConstantsBank,
            renderer.renderGraph.commandPool, renderer.vDevice);
      }

      BlenderModel ogreModel = loadModel("assets/Ogre.3d");

      for (const auto &ogrePos : worldData.ogres.positions) {
        glm::mat4 ogreMat4Model = glm::mat4(1.0f);

        Transform ot = modelToTransform(ogreMat4Model);
        ot.position = ogrePos;

        ogreMat4Model = transformToModel(ot.position, ot.rotation, ot.scale);

        createBasicGameObject(
            ogreModel, ogreMesh, ot, uniformsBank, pushConstantsBank,
            renderer.renderGraph.commandPool, renderer.vDevice);
      }
    };

    renderer.run();
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
