#pragma once

#include "renderGraph/renderGraph.h"
#include "renderNode/renderNodeUtils.h"
#include "shaders/banksManager.h"
#include "shaders/shaders.h"
#include "vDevice.h"
#include "vInstance.h"
#include "vSwapChain.h"
#include "window.h"
#include <GLFW/glfw3.h>
#include <functional>

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

    auto &depthTestingGroups = renderGraph.depthTestNode.pipelineGroups;
    auto &mainNodeGroups = renderGraph.mainNode.pipelineGroups;

    renderGraph.particlesComputeNode.updatePushConstants = [&]() {
      auto &pushConstantsBank = renderGraph.context.pushConstantBank;
      Renderer::Shaders::PushConstantsBank::setUInt(
          pushConstantsBank, SG_PUSH_PARTICLES_COUNT_INDEX, 100);
    };

    window.update([&]() {
      updateTime();

      auto &uniformsBank = renderGraph.context.globalUniformBufferData.data;
      Renderer::Shaders::UniformBank::setFloat(
          uniformsBank, SG_DELTA_TIME_INDEX, timeState.deltaTime);

      if (onUpdate) {
        onUpdate();
      }

      renderGraph.depthTestNode.clearRenderCalls();
      renderGraph.mainNode.clearRenderCalls();
      renderGraph.particlesDrawNode.clearRenderCalls();

      for (Renderable &renderable : resources.renderables) {
        if (!renderable.visible) {
          continue;
        }

        if (renderable.renderKind == ObjectRenderKind::Static) {
          auto staticRenderCall = RenderNodeUtils::RenderCall{
              .vertexBuffer = renderable.meshV2->vertexAllocations.buffer,
              .indexBuffer = renderable.meshV2->indexAllocations.buffer,
              .indexCount = renderable.meshV2->indexCount,
              .updatePushConstants = [this, &renderable]() {
                auto &pushConstantsBank = renderGraph.context.pushConstantBank;

                TransformComponent &tc = getTransform(renderable.entity);

                Renderer::Shaders::PushConstantsBank::setInt(
                    pushConstantsBank, SG_PUSH_MODEL_INDEX, tc.modelIndex);

                Renderer::Shaders::PushConstantsBank::setUInt(
                    pushConstantsBank, SG_PUSH_DIFF_TEX_INDEX,
                    renderable.textureIndex != -1 ? renderable.textureIndex
                                                  : SG_MAX_TEXTURES - 1);

                Renderer::Shaders::PushConstantsBank::setInt(
                    pushConstantsBank, SG_PUSH_SAMPLED_DEPTH_PASS_IMAGE_INDEX,
                    renderGraph.sampledDepthTexture->index);
              }};

          // depthTestingGroups[renderGraph.depthTestPipelines.staticPipeline]
          //     .renderCalls.emplace_back(staticRenderCall);
          mainNodeGroups[renderGraph.mainPipelines.staticPipeline]
              .renderCalls.emplace_back(staticRenderCall);
        }

        if (renderable.renderKind == ObjectRenderKind::Animated) {

          auto animatedRenderCall = RenderNodeUtils::RenderCall{
              .vertexBuffer =
                  renderable.animatedMeshV2->mesh.vertexAllocations.buffer,
              .indexBuffer =
                  renderable.animatedMeshV2->mesh.indexAllocations.buffer,
              .indexCount = renderable.animatedMeshV2->mesh.indexCount,
              .updatePushConstants = [&]() {
                auto &pushConstantsBank = renderGraph.context.pushConstantBank;

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

                Renderer::Shaders::PushConstantsBank::setUInt(
                    pushConstantsBank, SG_PUSH_DIFF_TEX_INDEX,
                    renderable.textureIndex != -1 ? renderable.textureIndex
                                                  : SG_MAX_TEXTURES - 1);

                Renderer::Shaders::PushConstantsBank::setInt(
                    pushConstantsBank, SG_PUSH_SAMPLED_DEPTH_PASS_IMAGE_INDEX,
                    renderGraph.sampledDepthTexture->index);
              }};

          depthTestingGroups[renderGraph.depthTestPipelines.animatedPipeline]
              .renderCalls.emplace_back(animatedRenderCall);
          mainNodeGroups[renderGraph.mainPipelines.animatedPipeline]
              .renderCalls.emplace_back(animatedRenderCall);
        }
      }

      for (auto &emitter : resources.particleEmitters) {
        renderGraph.particlesDrawNode.pipelineGroups[0].renderCalls.push_back(
            Renderer::RenderNodeUtils::RenderCall{
                .vertexBuffer = emitter.particleMesh->vertexAllocations.buffer,
                .indexBuffer = emitter.particleMesh->indexAllocations.buffer,
                .indexCount = emitter.particleMesh->indexCount,
                .instanceCount = emitter.maxParticles,
                .updatePushConstants = [&]() {
                  auto &pushConstantsBank =
                      renderGraph.context.pushConstantBank;

                  TransformComponent &tc = getTransform(emitter.entity);

                  Renderer::Shaders::PushConstantsBank::setInt(
                      pushConstantsBank, SG_PUSH_MODEL_INDEX, tc.modelIndex);
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
