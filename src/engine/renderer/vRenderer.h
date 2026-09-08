#pragma once

#include "renderNode/renderNodeUtils.h"
#include "shaders/banksManager.h"
#include <functional>
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "renderGraph.h"
#include "renderNode/colorRenderNode.h"
#include "vDevice.h"
#include "vInstance.h"
#include "vSwapChain.h"
#include "window.h"

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

    colorRenderNode.init(
        vDevice,
        Renderer::step1_initShadersProps{
            .shaderCreateInfos =
                {Renderer::RenderNodeUtils::ShaderCreateInfo{
                     .type = Renderer::RenderNodeUtils::ShaderType::Vertex,
                     .name = "vertMain"},
                 Renderer::RenderNodeUtils::ShaderCreateInfo{
                     .type = Renderer::RenderNodeUtils::ShaderType::Fragment,
                     .name = "fragMain"}},
            .shaderFile = "shaders/v2/objectNode.spv"},
        vSwapChain);

    colorRenderNode.setData<Blender::V2::Vertex>(vDevice,
                                                 renderGraph.commandPool);

    colorRenderNode.finish<Blender::V2::Vertex>(
        vDevice, vTextureManager, vSwapChain, renderGraph.commandPool);

    renderGraph.init(vDevice.device, vSwapChain.swapChainImages);
  }

  void mainLoop() {

    // Let's asume for now that all Renderables are just the initial color pass.
    colorRenderNode.renderNode->renderCalls.reserve(
        resources.renderables.size());

    window.update([this]() {
      updateTime();

      if (onUpdate) {
        onUpdate();
      }

      colorRenderNode.renderNode->renderCalls.clear();

      for (Renderable &renderable : resources.renderables) {
        if (!renderable.visible || renderable.meshV2 == nullptr) {
          continue;
        }

        colorRenderNode.renderNode->renderCalls.emplace_back(
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

      renderGraph.prepareNodes(vDevice.device, vSwapChain);
      renderGraph.submit(vDevice.graphicsQueue, vSwapChain);
    });

    vDevice.device.waitIdle();
  }

  void cleanup() { window.cleanup(); }
};
} // namespace Renderer
