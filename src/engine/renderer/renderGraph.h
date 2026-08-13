#pragma once

#include "renderNode.h"
#include "vSwapChain.h"
#include <cstdint>
#include <vector>
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#include <iostream>
#include <vulkan/vulkan_raii.hpp>

namespace Renderer {

struct RenderGraph {
  std::vector<vk::raii::Semaphore> presentCompleteSemaphores;
  std::vector<vk::raii::Semaphore> renderFinishedSemaphores;
  std::vector<vk::raii::Fence> inFlightFences;
  unsigned int imageIndex = UINT32_MAX;
  uint32_t frameIndex = 0;

  std::vector<Renderer::RenderNode> renderNodes;

  void init(vk::raii::Device &device, std::vector<vk::Image> &swapChainImages) {
    assert(presentCompleteSemaphores.empty() &&
           renderFinishedSemaphores.empty() && inFlightFences.empty());

    for (size_t i = 0; i < swapChainImages.size(); i++) {
      renderFinishedSemaphores.emplace_back(device, vk::SemaphoreCreateInfo());
    }

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
      presentCompleteSemaphores.emplace_back(device, vk::SemaphoreCreateInfo());
      inFlightFences.emplace_back(
          device,
          vk::FenceCreateInfo{.flags = vk::FenceCreateFlagBits::eSignaled});
    }
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

    for (RenderNode &node : renderNodes) {
      node.commandBuffers[frameIndex].reset();
      node.perFrame1_recordCommandBuffer(vSwapChain, imageIndex, frameIndex);
    }

    device.resetFences(*inFlightFences[frameIndex]);
  }

  void submit(vk::raii::Queue &graphicsQueue,
              Renderer::VSwapChain &vSwapChain) {

    vk::PipelineStageFlags waitDestinationStageMask(
        vk::PipelineStageFlagBits::eColorAttachmentOutput);

    std::vector<vk::CommandBuffer> commandBuffers;

    for (RenderNode &node : renderNodes) {
      commandBuffers.push_back(node.commandBuffers[frameIndex]);
    }

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

    frameIndex = (frameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
  }
};

} // namespace Renderer
