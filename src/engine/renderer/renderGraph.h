#pragma once

#include "images/vImage.h"
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

  Renderer::RenderNode shadowPassNode;
  Images::VImage depthImage;
  Images::VImage colorMSAAsampleImage;

  size_t staticPipeline = 0;
  size_t animatedPipeline = 0;

  Renderer::Images::VManager vTextureManager{};

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
      shadowPassNode.outputs[i] = std::make_unique<Images::VImage>();
      shadowPassNode.outputs[i] = std::make_unique<Images::VImage>();
    }

    shadowPassNode.preInit(context);
    shadowPassNode.updateUniforms = true;
    shadowPassNode.usePushConstants = true;

    shadowPassNode.step1_initShaders(
        vDevice.device, Renderer::step1_initShadersProps{
                            .shaderFile = "shaders/v2/objectNode.spv"});

    depthImage.format = vk::Format::eD32Sfloat;
    depthImage.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment;
    depthImage.aspectMask = vk::ImageAspectFlagBits::eDepth;

    depthImage.init(vSwapChain.swapChainExtent.width,
                    vSwapChain.swapChainExtent.height, vDevice,
                    vk::SampleCountFlagBits::e4);

    colorMSAAsampleImage.usage = vk::ImageUsageFlagBits::eTransientAttachment |
                                 vk::ImageUsageFlagBits::eColorAttachment;

    colorMSAAsampleImage.init(vSwapChain.swapChainExtent.width,
                              vSwapChain.swapChainExtent.height, vDevice,
                              vk::SampleCountFlagBits::e4);

    shadowPassNode.perFrameFunction = [&](Renderer::VSwapChain &vSwapChain,
                                          uint32_t imageIndex,
                                          uint32_t frameIndex) {
      Renderer::Images::VImage *shadowImage =
          shadowPassNode.outputs[frameIndex].get();

      if (!initializedFrames[frameIndex]) {

        shadowImage->usage = vk::ImageUsageFlagBits::eColorAttachment |
                             vk::ImageUsageFlagBits::eTransferSrc;
        shadowImage->format = vSwapChain.swapChainSurfaceFormat.format;
        initializedFrames[frameIndex] = true;

        shadowImage->init(vSwapChain.swapChainExtent.width,
                          vSwapChain.swapChainExtent.height, vDevice,
                          vk::SampleCountFlagBits::e4);

        // If this is the first frame (per frame in flight), from undefined to
        // color attachment optimal layout, from being on eNoNe stage to color
        // attachment output (we're writing colors on this). From no access flag
        // to color attachment write (again, writing color on this image).
        shadowImage->transition(
            vk::PipelineStageFlagBits2::eNone, {},
            vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            vk::AccessFlagBits2::eColorAttachmentWrite,
            vk::ImageLayout::eUndefined,
            vk::ImageLayout::eColorAttachmentOptimal,
            shadowPassNode.commandBuffers[frameIndex]);
      } else {
        // Else, from being on transfer with transfer read access and transfer
        // src optimal layout (This is only because that was the last state of
        // this image, we were transfering it's pixels to the swapchain at the
        // end) to color attachment write stage with color attachment write
        // access on color attachment optimal layout.
        shadowImage->transition(
            vk::PipelineStageFlagBits2::eTransfer,
            vk::AccessFlagBits2::eTransferRead,
            vk::PipelineStageFlagBits2::eColorAttachmentOutput,
            vk::AccessFlagBits2::eColorAttachmentWrite,
            vk::ImageLayout::eTransferSrcOptimal,
            vk::ImageLayout::eColorAttachmentOptimal,
            shadowPassNode.commandBuffers[frameIndex]);
      }

      vk::ClearValue clearColor{};
      vk::ClearValue clearDepth{};

      // Water/sky color. At some point should be a config.
      clearColor.color.setFloat32(
          std::array<float, 4>{0.309469f, 0.991102f, 0.266356f, 1.0f});

      clearDepth.depthStencil.setDepth(1.0f);
      clearDepth.depthStencil.setStencil(0);

      depthImage.transition(vk::PipelineStageFlagBits2::eEarlyFragmentTests |
                                vk::PipelineStageFlagBits2::eLateFragmentTests,
                            vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
                            vk::PipelineStageFlagBits2::eEarlyFragmentTests |
                                vk::PipelineStageFlagBits2::eLateFragmentTests,
                            vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
                            vk::ImageLayout::eUndefined,
                            vk::ImageLayout::eDepthAttachmentOptimal,
                            shadowPassNode.commandBuffers[frameIndex],
                            vk::ImageAspectFlagBits::eDepth);

      colorMSAAsampleImage.transition(
          vk::PipelineStageFlagBits2::eColorAttachmentOutput,
          vk::AccessFlagBits2::eColorAttachmentWrite,
          vk::PipelineStageFlagBits2::eColorAttachmentOutput,
          vk::AccessFlagBits2::eColorAttachmentWrite,
          vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal,
          shadowPassNode.commandBuffers[frameIndex]);

      vk::RenderingAttachmentInfo colorAttachmentInfo = {
          .imageView = shadowImage->view,
          .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
          .loadOp = vk::AttachmentLoadOp::eClear,
          .storeOp = vk::AttachmentStoreOp::eStore,
          .clearValue = clearColor};

      vk::RenderingAttachmentInfo depthAttachmentInfo = {
          .imageView = depthImage.view,
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

      shadowPassNode.commandBuffers[frameIndex].beginRendering(renderingInfo);

      // Do the drawings on the shadowImage.
      shadowPassNode.recordCommandBuffer(vSwapChain, frameIndex);

      // From shadow pass to swapchain:
      shadowImage->transition(
          vk::PipelineStageFlagBits2::eColorAttachmentOutput,
          vk::AccessFlagBits2::eColorAttachmentWrite,
          vk::PipelineStageFlagBits2::eTransfer,
          vk::AccessFlagBits2::eTransferRead,
          vk::ImageLayout::eColorAttachmentOptimal,
          vk::ImageLayout::eTransferSrcOptimal,
          shadowPassNode.commandBuffers[frameIndex]);

      Images::transitionImage(vSwapChain.swapChainImages[imageIndex],
                              vk::PipelineStageFlagBits2::eNone, {},
                              vk::PipelineStageFlagBits2::eTransfer,
                              vk::AccessFlagBits2::eTransferWrite,
                              vk::ImageLayout::eUndefined,
                              vk::ImageLayout::eTransferDstOptimal,
                              shadowPassNode.commandBuffers[frameIndex]);

      // Resolve instead of copy because we're doing multisampling.
      vk::ImageResolve resolveRegion{
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

      shadowPassNode.commandBuffers[frameIndex].resolveImage(
          shadowImage->image, vk::ImageLayout::eTransferSrcOptimal,
          vSwapChain.swapChainImages[imageIndex],
          vk::ImageLayout::eTransferDstOptimal, resolveRegion);

      Images::transitionImage(vSwapChain.swapChainImages[imageIndex],
                              vk::PipelineStageFlagBits2::eTransfer,
                              vk::AccessFlagBits2::eTransferWrite,
                              vk::PipelineStageFlagBits2::eNone, {},
                              vk::ImageLayout::eTransferDstOptimal,
                              vk::ImageLayout::ePresentSrcKHR,
                              shadowPassNode.commandBuffers[frameIndex]);

      shadowPassNode.commandBuffers[frameIndex].end();
    };

    shadowPassNode.step_1_2_createUniformBuffers(vDevice);
    shadowPassNode.step_1_3_createDescriptorSetLayout(vDevice.device);
    shadowPassNode.step_1_4_createDescriptorPool(vDevice.device);
    shadowPassNode.step_1_5_allocateDescriptorSets(vDevice.device);

    shadowPassNode.step_1_6_configureDescriptorSets(vDevice.device,
                                                    vTextureManager);
    shadowPassNode.step2_createPipelineLayout(vDevice.device);
    shadowPassNode.step3_initCommandBuffer(vDevice.queueIndex, vDevice.device,
                                           commandPool);

    // static pipeline
    staticPipeline = shadowPassNode.step2_addPipeline<Blender::V2::Vertex>(
        vDevice.device, vSwapChain.swapChainSurfaceFormat,
        Renderer::step2_pipelineConfigurationProps{
            .useDepth = true,
            .depthFormat = depthImage.format,
            .samples = vk::SampleCountFlagBits::e4,
            .useMultiSampling = true},
        {RenderNodeUtils::ShaderCreateInfo{
             .type = RenderNodeUtils::ShaderType::Vertex, .name = "vertMain"},
         RenderNodeUtils::ShaderCreateInfo{
             .type = RenderNodeUtils::ShaderType::Fragment,
             .name = "fragMain"}});

    // animated pipeline
    animatedPipeline =
        shadowPassNode.step2_addPipeline<Blender::V2::AnimatedVertex>(
            vDevice.device, vSwapChain.swapChainSurfaceFormat,
            Renderer::step2_pipelineConfigurationProps{
                .useDepth = true,
                .depthFormat = depthImage.format,
                .samples = vk::SampleCountFlagBits::e4,
                .useMultiSampling = true},
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

    // Shadow pass
    shadowPassNode.commandBuffers[frameIndex].reset();
    shadowPassNode.commandBuffers[frameIndex].begin(
        vk::CommandBufferBeginInfo{});
    shadowPassNode.perFrame1_updateUniformBuffers(frameIndex);
    shadowPassNode.perFrameFunction(vSwapChain, imageIndex, frameIndex);

    device.resetFences(*inFlightFences[frameIndex]);
  }

  void submit(vk::raii::Queue &graphicsQueue,
              Renderer::VSwapChain &vSwapChain) {

    vk::PipelineStageFlags waitDestinationStageMask(
        vk::PipelineStageFlagBits::eColorAttachmentOutput);

    std::vector<vk::CommandBuffer> commandBuffers;

    // Shadow pass
    commandBuffers.push_back(shadowPassNode.commandBuffers[frameIndex]);

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
