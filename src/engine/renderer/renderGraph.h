#pragma once

#include "images/vImage.h"
#include "images/vTexture.h"
#include "renderGraphUtils.h"
#include "renderNode/renderNode.h"
#include "renderNode/renderNodeUtils.h"
#include "vDevice.h"
#include "vSwapChain.h"
#include "vulkan/vulkan.hpp"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include "../blender/v2/importer.h"
#include <iostream>
#include <vulkan/vulkan_raii.hpp>

namespace Renderer {

struct NodePipelines {
  size_t staticPipeline = 0;
  size_t animatedPipeline = 0;
};

namespace RenderGraph {
struct RenderGraph {
  std::vector<vk::raii::Semaphore> presentCompleteSemaphores;
  std::vector<vk::raii::Semaphore> renderFinishedSemaphores;
  std::vector<vk::raii::Fence> inFlightFences;
  unsigned int imageIndex = UINT32_MAX;
  uint32_t frameIndex = 0;
  std::array<bool, RenderNodeUtils::MAX_FRAMES_IN_FLIGHT> initializedFrames{
      false, false};
  vk::raii::CommandPool commandPool = nullptr;

  Context context{};

  Renderer::RenderNode mainNode;
  Renderer::RenderNode depthTestNode;

  Images::VTexture *sampledDepthTexture = nullptr;
  Images::VImage mainNodeDepthTestImage;
  Images::VImage colorMSAAsampleImage;

  NodePipelines shadowVisualizationPipelines{};
  NodePipelines depthTestPipelines{};

  Renderer::Images::VManager vTextureManager{};

  Images::TransitionState deptTestWriteAttachmentState{
      .stage = vk::PipelineStageFlagBits2::eEarlyFragmentTests |
               vk::PipelineStageFlagBits2::eLateFragmentTests,
      .access = vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
      .layout = vk::ImageLayout::eDepthAttachmentOptimal};

#ifndef RENDER_GRAPH_MACROS
#define SHADOWS_RES 800
#endif

  void preInit(VDevice &vDevice) {
    vk::CommandPoolCreateInfo poolInfo{
        .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
        .queueFamilyIndex = vDevice.queueIndex,
    };

    commandPool = vk::raii::CommandPool{
        vDevice.device,
        poolInfo,
    };

    vTextureManager.init(vDevice, commandPool);
    sampledDepthTexture = vTextureManager.getTextureFromImage();
  }

  void init(Renderer::VSwapChain &vSwapChain, Renderer::VDevice &vDevice) {

    assert(presentCompleteSemaphores.empty() &&
           renderFinishedSemaphores.empty() && inFlightFences.empty());

    for (size_t i = 0; i < vSwapChain.swapChainImages.size(); i++) {
      renderFinishedSemaphores.emplace_back(vDevice.device,
                                            vk::SemaphoreCreateInfo());
    }

    for (size_t i = 0; i < RenderNodeUtils::MAX_FRAMES_IN_FLIGHT; i++) {
      presentCompleteSemaphores.emplace_back(vDevice.device,
                                             vk::SemaphoreCreateInfo());
      inFlightFences.emplace_back(
          vDevice.device,
          vk::FenceCreateInfo{.flags = vk::FenceCreateFlagBits::eSignaled});
    }

    for (size_t i = 0; i < RenderNodeUtils::MAX_FRAMES_IN_FLIGHT; i++) {
      mainNode.outputs[i] = std::make_unique<Images::VImage>();
      depthTestNode.outputs[i] = std::make_unique<Images::VImage>();
    }

    mainNode.preInit(context);
    mainNode.updateUniforms = true;
    mainNode.usePushConstants = true;

    depthTestNode.preInit(context);
    depthTestNode.updateUniforms = true;
    depthTestNode.usePushConstants = true;

    mainNode.step1_initShaders(vDevice.device,
                               Renderer::step1_initShadersProps{
                                   .shaderFile = "shaders/v2/mainNode.spv"});

    depthTestNode.step1_initShaders(
        vDevice.device, Renderer::step1_initShadersProps{
                            .shaderFile = "shaders/v2/depthTest.spv"});

    sampledDepthTexture->vImage.format = vk::Format::eD32Sfloat;
    sampledDepthTexture->vImage.usage =
        vk::ImageUsageFlagBits::eDepthStencilAttachment |
        vk::ImageUsageFlagBits::eSampled;
    sampledDepthTexture->vImage.aspectMask = vk::ImageAspectFlagBits::eDepth;

    sampledDepthTexture->vImage.init(SHADOWS_RES, SHADOWS_RES, vDevice);

    mainNodeDepthTestImage.format = vk::Format::eD32Sfloat;
    mainNodeDepthTestImage.usage =
        vk::ImageUsageFlagBits::eDepthStencilAttachment;
    mainNodeDepthTestImage.aspectMask = vk::ImageAspectFlagBits::eDepth;

    mainNodeDepthTestImage.init(
        vSwapChain.swapChainExtent.width, vSwapChain.swapChainExtent.height,
        vDevice,
        vk::SampleCountFlagBits::e4); // Since this depth test will be used with
                                      // a e4 sampled color attachment, it also
                                      // needs to be e4

    colorMSAAsampleImage.usage = vk::ImageUsageFlagBits::eTransientAttachment |
                                 vk::ImageUsageFlagBits::eColorAttachment;
    colorMSAAsampleImage.format = vSwapChain.swapChainSurfaceFormat.format;

    colorMSAAsampleImage.init(vSwapChain.swapChainExtent.width,
                              vSwapChain.swapChainExtent.height, vDevice,
                              vk::SampleCountFlagBits::e4);

    depthTestNode.perFrameFunction = [&](Renderer::VSwapChain &vSwapChain,
                                         uint32_t imageIndex,
                                         uint32_t frameIndex) {
      vk::ClearValue clearDepth{};

      clearDepth.depthStencil.setDepth(1.0f);
      clearDepth.depthStencil.setStencil(0);

      sampledDepthTexture->vImage.transition(
          deptTestWriteAttachmentState,
          depthTestNode.commandBuffers[frameIndex],
          vk::ImageAspectFlagBits::eDepth);

      vk::RenderingAttachmentInfo depthAttachmentInfo = {
          .imageView = sampledDepthTexture->vImage.view,
          .imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
          .loadOp = vk::AttachmentLoadOp::eClear,
          .storeOp = vk::AttachmentStoreOp::eStore,
          .clearValue = clearDepth};

      vk::RenderingInfo renderingInfo = {
          .renderArea = {.offset = {0, 0},
                         .extent = vk::Extent2D{.width = SHADOWS_RES,
                                                .height = SHADOWS_RES}},
          .layerCount = 1,
          .colorAttachmentCount = 0,
          .pDepthAttachment = &depthAttachmentInfo};

      depthTestNode.commandBuffers[frameIndex].beginRendering(renderingInfo);

      // Do the depth test pass
      depthTestNode.recordCommandBuffer(SHADOWS_RES * 1.0f, SHADOWS_RES * 1.0f,
                                        frameIndex);
      depthTestNode.commandBuffers[frameIndex].end();
    };

    mainNode.perFrameFunction = [&](Renderer::VSwapChain &vSwapChain,
                                    uint32_t imageIndex, uint32_t frameIndex) {
      vk::ClearValue clearColor{};
      vk::ClearValue clearDepth{};

      // Water/sky color. At some point should be a config.
      clearColor.color.setFloat32(
          std::array<float, 4>{0.309469f, 0.991102f, 0.266356f, 1.0f});

      clearDepth.depthStencil.setDepth(1.0f);
      clearDepth.depthStencil.setStencil(0);

      mainNodeDepthTestImage.transition(deptTestWriteAttachmentState,
                                        mainNode.commandBuffers[frameIndex],
                                        vk::ImageAspectFlagBits::eDepth);

      Renderer::Images::VImage *mainNodeImage =
          mainNode.outputs[frameIndex].get();

      if (!initializedFrames[frameIndex]) {

        mainNodeImage->usage = vk::ImageUsageFlagBits::eColorAttachment |
                               vk::ImageUsageFlagBits::eTransferSrc;

        mainNodeImage->format = vSwapChain.swapChainSurfaceFormat.format;
        initializedFrames[frameIndex] = true;

        mainNodeImage->init(vSwapChain.swapChainExtent.width,
                            vSwapChain.swapChainExtent.height, vDevice,
                            vk::SampleCountFlagBits::e1);
      }

      Images::TransitionState toColorWriteState{
          .stage = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
          .access = vk::AccessFlagBits2::eColorAttachmentWrite,
          .layout = vk::ImageLayout::eColorAttachmentOptimal};

      mainNodeImage->transition(toColorWriteState,
                                mainNode.commandBuffers[frameIndex]);

      colorMSAAsampleImage.transition(toColorWriteState,
                                      mainNode.commandBuffers[frameIndex]);

      Images::TransitionState toSampledOnShaderRead{
          .stage = vk::PipelineStageFlagBits2::eFragmentShader,
          .access = vk::AccessFlagBits2::eShaderSampledRead,
          .layout = vk::ImageLayout::eShaderReadOnlyOptimal};

      sampledDepthTexture->vImage.transition(
          toSampledOnShaderRead, mainNode.commandBuffers[frameIndex],
          vk::ImageAspectFlagBits::eDepth);

      vk::RenderingAttachmentInfo colorAttachmentInfo = {
          .imageView = colorMSAAsampleImage.view,
          .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
          .resolveMode = vk::ResolveModeFlagBits::eAverage,
          .resolveImageView = mainNodeImage->view,
          .resolveImageLayout = vk::ImageLayout::eColorAttachmentOptimal,
          .loadOp = vk::AttachmentLoadOp::eClear,
          .storeOp = vk::AttachmentStoreOp::eDontCare,
          .clearValue = clearColor,
      };

      vk::RenderingAttachmentInfo depthAttachmentInfo = {
          .imageView = mainNodeDepthTestImage.view,
          .imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
          .loadOp = vk::AttachmentLoadOp::eClear,
          .storeOp = vk::AttachmentStoreOp::eDontCare,
          .clearValue = clearDepth};

      vk::RenderingInfo renderingInfo = {
          .renderArea = {.offset = {0, 0},
                         .extent = vSwapChain.swapChainExtent},
          .layerCount = 1,
          .colorAttachmentCount = 1,
          .pColorAttachments = &colorAttachmentInfo,
          .pDepthAttachment = &depthAttachmentInfo};

      mainNode.commandBuffers[frameIndex].beginRendering(renderingInfo);

      // Do the drawings on the mainNodeImage.
      mainNode.recordCommandBuffer(
          static_cast<float>(vSwapChain.swapChainExtent.width),
          static_cast<float>(vSwapChain.swapChainExtent.height), frameIndex);

      // From main pass to swapchain:

      Images::TransitionState toTransferRead{
          .stage = vk::PipelineStageFlagBits2::eTransfer,
          .access = vk::AccessFlagBits2::eTransferRead,
          .layout = vk::ImageLayout::eTransferSrcOptimal};

      mainNodeImage->transition(toTransferRead,
                                mainNode.commandBuffers[frameIndex]);

      Images::TransitionState fromNoneUndefined{
          .stage = vk::PipelineStageFlagBits2::eNone,
          .access = {},
          .layout = vk::ImageLayout::eUndefined};

      Images::TransitionState toTransferWrite{
          .stage = vk::PipelineStageFlagBits2::eTransfer,
          .access = vk::AccessFlagBits2::eTransferWrite,
          .layout = vk::ImageLayout::eTransferDstOptimal};

      Images::transitionImage(vSwapChain.swapChainImages[imageIndex],
                              fromNoneUndefined, toTransferWrite,
                              mainNode.commandBuffers[frameIndex]);

      // Resolve instead of copy because we're doing multisampling.
      vk::ImageCopy copyRegion{
          .srcSubresource =
              {
                  .aspectMask = vk::ImageAspectFlagBits::eColor,
                  .mipLevel = 0,
                  .baseArrayLayer = 0,
                  .layerCount = 1,
              },
          .srcOffset = {.x = 0, .y = 0, .z = 0},
          .dstSubresource =
              {
                  .aspectMask = vk::ImageAspectFlagBits::eColor,
                  .mipLevel = 0,
                  .baseArrayLayer = 0,
                  .layerCount = 1,
              },
          .dstOffset = {.x = 0, .y = 0, .z = 0},
          .extent =
              {
                  .width = vSwapChain.swapChainExtent.width,
                  .height = vSwapChain.swapChainExtent.height,
                  .depth = 1,
              },
      };

      mainNode.commandBuffers[frameIndex].copyImage(
          mainNodeImage->image, vk::ImageLayout::eTransferSrcOptimal,
          vSwapChain.swapChainImages[imageIndex],
          vk::ImageLayout::eTransferDstOptimal, copyRegion);

      Images::TransitionState toPresent{
          .stage = vk::PipelineStageFlagBits2::eNone,
          .access = {},
          .layout = vk::ImageLayout::ePresentSrcKHR,
      };

      Images::transitionImage(vSwapChain.swapChainImages[imageIndex],
                              toTransferWrite, toPresent,
                              mainNode.commandBuffers[frameIndex]);

      mainNode.commandBuffers[frameIndex].end();
    };

    mainNode.step_1_2_createUniformBuffers(vDevice);
    mainNode.step_1_3_createDescriptorSetLayout(vDevice.device);
    mainNode.step_1_4_createDescriptorPool(vDevice.device);
    mainNode.step_1_5_allocateDescriptorSets(vDevice.device);

    mainNode.step_1_6_configureDescriptorSets(vDevice.device, vTextureManager);
    mainNode.step2_createPipelineLayout(vDevice.device);
    mainNode.step3_initCommandBuffer(vDevice.device, commandPool);

    // Commented out means the previous node already set them up
    //
    // depthTestNode.step_1_2_createUniformBuffers(vDevice);
    // depthTestNode.step_1_3_createDescriptorSetLayout(vDevice.device);
    // depthTestNode.step_1_4_createDescriptorPool(vDevice.device);
    // depthTestNode.step_1_5_allocateDescriptorSets(vDevice.device);

    // depthTestNode.step_1_6_configureDescriptorSets(vDevice.device,
    //                                                vTextureManager);
    depthTestNode.step2_createPipelineLayout(vDevice.device);
    depthTestNode.step3_initCommandBuffer(vDevice.device, commandPool);

    // static pipeline
    depthTestPipelines.staticPipeline =
        depthTestNode.step2_addPipeline<Blender::V2::Vertex>(
            vDevice.device,
            Renderer::step2_pipelineConfigurationProps{
                .useDepth = true,
                .isDepthPass = true,
                .depthFormat = vk::Format::eD32Sfloat,
                .useMultiSampling = false},
            {RenderNodeUtils::ShaderCreateInfo{
                .type = RenderNodeUtils::ShaderType::Vertex,
                .name = "vertMain"}});

    // animated pipeline
    depthTestPipelines.animatedPipeline =
        depthTestNode.step2_addPipeline<Blender::V2::AnimatedVertex>(
            vDevice.device,
            Renderer::step2_pipelineConfigurationProps{
                .useDepth = true,
                .isDepthPass = true,
                .depthFormat = vk::Format::eD32Sfloat,
                .useMultiSampling = false},
            {RenderNodeUtils::ShaderCreateInfo{
                .type = RenderNodeUtils::ShaderType::Vertex,
                .name = "vertAnimated"}});

    // static pipeline
    shadowVisualizationPipelines.staticPipeline =
        mainNode.step2_addPipeline<Blender::V2::Vertex>(
            vDevice.device,
            Renderer::step2_pipelineConfigurationProps{
                .useDepth = true,
                .colorFormat = vSwapChain.swapChainSurfaceFormat.format,
                .depthFormat = vk::Format::eD32Sfloat,
                .samples = vk::SampleCountFlagBits::e4,
                .useMultiSampling = true},
            {RenderNodeUtils::ShaderCreateInfo{
                 .type = RenderNodeUtils::ShaderType::Vertex,
                 .name = "vertMain"},
             RenderNodeUtils::ShaderCreateInfo{
                 .type = RenderNodeUtils::ShaderType::Fragment,
                 .name = "fragMain"}});

    // animated pipeline
    shadowVisualizationPipelines.animatedPipeline =
        mainNode.step2_addPipeline<Blender::V2::AnimatedVertex>(
            vDevice.device,
            Renderer::step2_pipelineConfigurationProps{
                .useDepth = true,
                .colorFormat = vSwapChain.swapChainSurfaceFormat.format,
                .depthFormat = vk::Format::eD32Sfloat,
                .samples = vk::SampleCountFlagBits::e4,
                .useMultiSampling = true,
            },
            {RenderNodeUtils::ShaderCreateInfo{
                 .type = RenderNodeUtils::ShaderType::Vertex,
                 .name = "vertAnimated"},
             RenderNodeUtils::ShaderCreateInfo{
                 .type = RenderNodeUtils::ShaderType::Fragment,
                 .name = "fragMain"}});
  }

  void prepareNodes(vk::raii::Device &device,
                    Renderer::VSwapChain &vSwapChain) {

    auto fenceResult =
        device.waitForFences(*inFlightFences[frameIndex], vk::True, UINT64_MAX);
    if (fenceResult != vk::Result::eSuccess) {
      throw std::runtime_error("failed to wait for fence!");
    }

    auto [_, acquiredImageIndex] = vSwapChain.swapChain.acquireNextImage(
        UINT64_MAX, *presentCompleteSemaphores[frameIndex], nullptr);

    imageIndex = acquiredImageIndex;

    // Depth testing pass
    depthTestNode.commandBuffers[frameIndex].reset();
    depthTestNode.commandBuffers[frameIndex].begin(
        vk::CommandBufferBeginInfo{});
    depthTestNode.perFrame1_updateUniformBuffers(frameIndex);
    depthTestNode.perFrameFunction(vSwapChain, imageIndex, frameIndex);

    // Main node pass
    mainNode.commandBuffers[frameIndex].reset();
    mainNode.commandBuffers[frameIndex].begin(vk::CommandBufferBeginInfo{});
    // Not needed since the depth pass already updates this.
    // mainNode.perFrame1_updateUniformBuffers(frameIndex);
    mainNode.perFrameFunction(vSwapChain, imageIndex, frameIndex);

    device.resetFences(*inFlightFences[frameIndex]);
  }

  void submit(vk::raii::Queue &graphicsQueue,
              Renderer::VSwapChain &vSwapChain) {

    vk::PipelineStageFlags waitDestinationStageMask(
        vk::PipelineStageFlagBits::eColorAttachmentOutput);

    std::vector<vk::CommandBuffer> commandBuffers;

    // Depth test pass
    commandBuffers.push_back(depthTestNode.commandBuffers[frameIndex]);
    // Main node
    commandBuffers.push_back(mainNode.commandBuffers[frameIndex]);

    const vk::SubmitInfo submitInfo{
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &*presentCompleteSemaphores[frameIndex],
        .pWaitDstStageMask = &waitDestinationStageMask,
        .commandBufferCount = static_cast<uint32_t>(commandBuffers.size()),
        .pCommandBuffers = commandBuffers.data(),
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &*renderFinishedSemaphores[imageIndex]};
    graphicsQueue.submit(submitInfo, *inFlightFences[frameIndex]);

    const vk::PresentInfoKHR presentInfoKHR{
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &*renderFinishedSemaphores[imageIndex],
        .swapchainCount = 1,
        .pSwapchains = &*vSwapChain.swapChain,
        .pImageIndices = &imageIndex};
    auto result = graphicsQueue.presentKHR(presentInfoKHR);
    switch (result) {
    case vk::Result::eSuccess:
      break;
    case vk::Result::eSuboptimalKHR:
      std::cout
          << "vk::Queue::presentKHR returned vk::Result::eSuboptimalKHR !\n";
      break;
    default:
      break; // an unexpected result is returned!
    }

    frameIndex = (frameIndex + 1) % RenderNodeUtils::MAX_FRAMES_IN_FLIGHT;
  }
};
} // namespace RenderGraph

} // namespace Renderer
