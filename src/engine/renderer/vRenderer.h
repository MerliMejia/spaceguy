#pragma once

#include "renderNode/renderNodeUtils.h"
#include "shaders/banksManager.h"
#include "shaders/shaders.h"
#include <GLFW/glfw3.h>
#include <functional>
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "renderGraph.h"
#include "vDevice.h"
#include "vInstance.h"
#include "vSwapChain.h"
#include "window.h"

#include "../../systems/animationSystem.h"
#include "../../systems/resourceManagementSystem.h"
#include "../../utils/time.h"

namespace Renderer {

struct VRenderer {
  void run() {
    Shaders::bankManager.init();
    window.init("Spaceguy");

    initVulkan();
    mainLoop();
    cleanup();
  }

  std::function<void()> onUpdate;
  std::function<void()> onInit;

  Renderer::VInstance vInstance;
  Renderer::Window window;
  Renderer::VDevice vDevice;
  Renderer::VSwapChain vSwapChain;
  Renderer::RenderGraph::RenderGraph renderGraph{};

  void initVulkan() {
    vInstance.create();
    window.createSurface(vInstance.handler);
    vDevice.pickAndCreate(vInstance.handler, window.surface);
    vSwapChain.create(vDevice.physicalDevice, window.surface, vDevice.device,
                      window.handler);

    renderGraph.preInit(vDevice);

    if (onInit) {
      onInit();
    }

    renderGraph.init(vSwapChain, vDevice);
  }

  void mainLoop() {

    auto &groups = renderGraph.shadowPassNode.pipelineGroups;

    window.update([&]() {
      updateTime();

      if (onUpdate) {
        onUpdate();
      }

      renderGraph.shadowPassNode.clearRenderCalls();

      for (Renderable &renderable : resources.renderables) {
        if (!renderable.visible) {
          continue;
        }

        if (renderable.renderKind == ObjectRenderKind::Static) {
          groups[renderGraph.staticPipeline].renderCalls.emplace_back(
              RenderNodeUtils::RenderCall{
                  .vertexBuffer = renderable.meshV2->vertexAllocations.buffer,
                  .indexBuffer = renderable.meshV2->indexAllocations.buffer,
                  .indexCount = renderable.meshV2->indexCount,
                  .updatePushConstants = [this, &renderable]() {
                    auto &pushConstantsBank =
                        renderGraph.context.pushConstantBank;

                    TransformComponent &tc = getTransform(renderable.entity);

                    Renderer::Shaders::PushConstantsBank::setInt(
                        pushConstantsBank, SG_PUSH_MODEL_INDEX, tc.modelIndex);

                    if (renderable.textureIndex != -1) {
                      Renderer::Shaders::PushConstantsBank::setUInt(
                          pushConstantsBank, SG_PUSH_DIFF_TEX_INDEX,
                          renderable.textureIndex);
                    }
                  }});
        }

        if (renderable.renderKind == ObjectRenderKind::Animated) {
          groups[renderGraph.animatedPipeline].renderCalls.emplace_back(
              RenderNodeUtils::RenderCall{
                  .vertexBuffer =
                      renderable.animatedMeshV2->mesh.vertexAllocations.buffer,
                  .indexBuffer =
                      renderable.animatedMeshV2->mesh.indexAllocations.buffer,
                  .indexCount = renderable.animatedMeshV2->mesh.indexCount,
                  .updatePushConstants = [&]() {
                    auto &pushConstantsBank =
                        renderGraph.context.pushConstantBank;

                    auto animationData =
                        getAnimationDataFromEntity(renderable.entity);

                    Renderer::Shaders::PushConstantsBank::setInt(
                        pushConstantsBank, SG_PUSH_PREV_POSE_INDEX,
                        animationData.previousPositionOffset);
                    Renderer::Shaders::PushConstantsBank::setInt(
                        pushConstantsBank, SG_PUSH_NEXT_POSE_INDEX,
                        animationData.nextPositionOffset);
                    Renderer::Shaders::PushConstantsBank::setFloat(
                        pushConstantsBank, SG_PUS_INTERPOLATION_FACTOR_INDEX,
                        animationData.interpolation);

                    TransformComponent &tc = getTransform(renderable.entity);

                    Renderer::Shaders::PushConstantsBank::setInt(
                        pushConstantsBank, SG_PUSH_MODEL_INDEX, tc.modelIndex);

                    if (renderable.textureIndex != -1) {
                      Renderer::Shaders::PushConstantsBank::setUInt(
                          pushConstantsBank, SG_PUSH_DIFF_TEX_INDEX,
                          renderable.textureIndex);
                    }
                  }});
        }
      }

      renderGraph.prepareNodes(vDevice.device, vSwapChain);
      renderGraph.submit(vDevice.graphicsQueue, vSwapChain);
    });

    vDevice.device.waitIdle();
  }

  void cleanup() { window.cleanup(); }
};
} // namespace Renderer
