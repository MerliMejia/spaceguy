#pragma once

#include "renderNode.h"
#include "vulkan/vulkan.hpp"
#include <memory>

namespace Renderer {
struct ColorRenderNode {
  RenderNode *renderNode = nullptr;
  vk::ClearValue clearColor{};
  vk::ClearValue clearDepth{};
  Images::VImage depthImage;

  bool present = false;
  bool useDepthTesting = false;

  void render1(Renderer::VSwapChain &vSwapChain, uint32_t imageIndex,
               uint32_t frameIndex) {
    renderNode->commandBuffers[frameIndex].begin({});

    if (present) {
      // Set the output image ready for writing color in it.
      Images::transitionImage(
          vSwapChain.swapChainImages[imageIndex],
          vk::PipelineStageFlagBits2::eNone, {},
          vk::PipelineStageFlagBits2::eColorAttachmentOutput,
          vk::AccessFlagBits2::eColorAttachmentWrite,
          vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal,
          renderNode->commandBuffers[frameIndex]);
    } else {
      auto renderNodePtr = renderNode->output.get();
      vk::Image nodeImageHandler = static_cast<vk::Image>(renderNodePtr->image);

      // Set the output image ready for writing color in it.
      Images::transitionImage(
          nodeImageHandler, vk::PipelineStageFlagBits2::eNone, {},
          vk::PipelineStageFlagBits2::eColorAttachmentOutput,
          vk::AccessFlagBits2::eColorAttachmentWrite,
          vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal,
          renderNode->commandBuffers[frameIndex]);
    }

    clearColor.color.setFloat32(std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f});
    clearDepth.depthStencil.setDepth(1.0f);
    clearDepth.depthStencil.setStencil(0);

    if (useDepthTesting) {
      depthImage.transition(vk::PipelineStageFlagBits2::eEarlyFragmentTests |
                                vk::PipelineStageFlagBits2::eLateFragmentTests,
                            vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
                            vk::PipelineStageFlagBits2::eEarlyFragmentTests |
                                vk::PipelineStageFlagBits2::eLateFragmentTests,
                            vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
                            vk::ImageLayout::eUndefined,
                            vk::ImageLayout::eDepthAttachmentOptimal,
                            renderNode->commandBuffers[frameIndex],
                            vk::ImageAspectFlagBits::eDepth);
    }

    // In this case the output image is a color attachment because I'll write
    // colors on it.
    vk::RenderingAttachmentInfo attachmentInfo = {
        .imageView = present ? vSwapChain.swapChainImageViews[imageIndex]
                             : renderNode->output->view,
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
        .renderArea = {.offset = {0, 0}, .extent = vSwapChain.swapChainExtent},
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &attachmentInfo};

    if (useDepthTesting) {
      renderingInfo.pDepthAttachment = &depthAttachmentInfo;
    }

    renderNode->commandBuffers[frameIndex].beginRendering(renderingInfo);

    renderNode->recordCommandBuffer(vSwapChain, imageIndex, frameIndex);

    if (present) {
      Images::transitionImage(
          vSwapChain.swapChainImages[imageIndex],
          vk::PipelineStageFlagBits2::eColorAttachmentOutput,
          vk::AccessFlagBits2::eColorAttachmentWrite,
          vk::PipelineStageFlagBits2::eBottomOfPipe, {},
          vk::ImageLayout::eColorAttachmentOptimal,
          vk::ImageLayout::ePresentSrcKHR,
          renderNode->commandBuffers[frameIndex]);
    }

    renderNode->commandBuffers[frameIndex].end();
  }

  void init(Renderer::VDevice &vDevice,
            Renderer::step1_initShadersProps initShaderProps,
            Renderer::VSwapChain &vSwapChain) {
    renderNode->updateUniforms = true;
    renderNode->usePushConstants = true;

    renderNode->step1_initShaders(vDevice.device, initShaderProps);

    if (!present) {
      auto ptr = std::make_unique<Images::VImage>();

      renderNode->output = std::move(ptr);

      renderNode->output->usage = vk::ImageUsageFlagBits::eColorAttachment;

      renderNode->output->init(vSwapChain.swapChainExtent.width,
                               vSwapChain.swapChainExtent.height, vDevice);
    }

    if (useDepthTesting) {
      depthImage.format = vk::Format::eD32Sfloat;
      depthImage.usage = vk::ImageUsageFlagBits::eDepthStencilAttachment;
      depthImage.aspectMask = vk::ImageAspectFlagBits::eDepth;

      depthImage.init(vSwapChain.swapChainExtent.width,
                      vSwapChain.swapChainExtent.height, vDevice);
    }

    renderNode->perFrameFunctions[0] = [this](Renderer::VSwapChain &vSwapChain,
                                              uint32_t imageIndex,
                                              uint32_t frameIndex) {
      render1(vSwapChain, imageIndex, frameIndex);
    };
  }

public:
  template <RenderNodeUtils::VertexType T>
  void setData(std::vector<T> incomingVertices, std::vector<uint32_t> indices,
               VDevice &vDevice, vk::raii::CommandPool &commandPool) {

    renderNode->step_1_1_createAndFillVertexBuffer<T>(incomingVertices, vDevice,
                                                      commandPool);

    renderNode->step_1_2_createAndFillIndicesBuffer(indices, vDevice,
                                                    commandPool);

    renderNode->step_1_3_createUniformBuffers(vDevice);

    renderNode->step_1_4_createDescriptorSetLayout(vDevice.device);

    renderNode->step_1_5_createDescriptorPool(vDevice.device);

    renderNode->step_1_6_allocateDescriptorSets(vDevice.device);
  }

public:
  template <RenderNodeUtils::VertexType T>
  void finish(VDevice &vDevice, Renderer::Images::VManager &vTextureManager,
              Renderer::VSwapChain &vSwapChain,
              vk::raii::CommandPool &commandPool) {

    renderNode->step_1_7_configureDescriptorSets(vDevice.device,
                                                 vTextureManager);

    renderNode->step2_initPipelineConfiguration<T>(
        vDevice.device, vSwapChain.swapChainSurfaceFormat,
        Renderer::step2_pipelineConfigurationProps{
            .useDepth = useDepthTesting, .depthFormat = depthImage.format});

    renderNode->step3_initCommandBuffer(vDevice.queueIndex, vDevice.device,
                                        commandPool);
  }
};
} // namespace Renderer
