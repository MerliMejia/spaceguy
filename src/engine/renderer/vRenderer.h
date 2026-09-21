#pragma once

#include "renderNode/renderNodeUtils.h"
#include "shaders/banksManager.h"
#include "shaders/shaders.h"
#include <GLFW/glfw3.h>
#include <functional>
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "renderGraph.h"
#include "renderNode/colorRenderNode.h"
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
  Renderer::RenderGraph::Fucntions renderGraph{};

  Renderer::ColorRenderNode colorRenderNode;
  Renderer::Images::VManager vTextureManager{};

  size_t staticPipeline = 0;
  size_t animatedPipeline = 0;

  void initVulkan() {
    vInstance.create();
    window.createSurface(vInstance.handler);
    vDevice.pickAndCreate(vInstance.handler, window.surface);
    vSwapChain.create(vDevice.physicalDevice, window.surface, vDevice.device,
                      window.handler);

    renderGraph.init(vDevice);

    vTextureManager.init(vDevice, renderGraph.commandPool);

    if (onInit) {
      onInit();
    }

    colorRenderNode.renderNode = &renderGraph.createNode();
    colorRenderNode.present = true;
    colorRenderNode.useDepthTesting = true;

    colorRenderNode.init(vDevice,
                         Renderer::step1_initShadersProps{
                             .shaderFile = "shaders/v2/objectNode.spv"},
                         vSwapChain);

    colorRenderNode.setData(vDevice);
    colorRenderNode.finish(vDevice, vTextureManager, renderGraph.commandPool);

    using Renderer::RenderNodeUtils::ShaderCreateInfo;
    using Renderer::RenderNodeUtils::ShaderType;

    staticPipeline = colorRenderNode.addPipeline<Blender::V2::Vertex>(
        vDevice, vSwapChain,
        {ShaderCreateInfo{.type = ShaderType::Vertex, .name = "vertMain"},
         ShaderCreateInfo{.type = ShaderType::Fragment, .name = "fragMain"}});

    animatedPipeline = colorRenderNode.addPipeline<Blender::V2::AnimatedVertex>(
        vDevice, vSwapChain,
        {ShaderCreateInfo{.type = ShaderType::Vertex, .name = "vertAnimated"},
         ShaderCreateInfo{.type = ShaderType::Fragment, .name = "fragMain"}});

    renderGraph.init(vDevice.device, vSwapChain.swapChainImages);
  }

  void mainLoop() {

    auto &groups = colorRenderNode.renderNode->pipelineGroups;
    groups[staticPipeline].renderCalls.reserve(resources.renderables.size());
    groups[animatedPipeline].renderCalls.reserve(resources.renderables.size());

    window.update([&]() {
      updateTime();

      if (onUpdate) {
        onUpdate();
      }

      colorRenderNode.renderNode->clearRenderCalls();

      for (Renderable &renderable : resources.renderables) {
        if (!renderable.visible) {
          continue;
        }

        if (renderable.renderKind == ObjectRenderKind::Static) {
          groups[staticPipeline].renderCalls.emplace_back(
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
          groups[animatedPipeline].renderCalls.emplace_back(
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
