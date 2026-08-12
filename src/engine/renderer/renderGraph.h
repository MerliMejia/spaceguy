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
  vk::raii::Semaphore presentCompleteSemaphore = nullptr;
  vk::raii::Semaphore renderFinishedSemaphore = nullptr;
  vk::raii::Fence drawFence = nullptr;
  unsigned int imageIndex = UINT32_MAX;

  std::vector<Renderer::RenderNode> renderNodes;

  void init(vk::raii::Device &device) {
    presentCompleteSemaphore =
        vk::raii::Semaphore(device, vk::SemaphoreCreateInfo());
    renderFinishedSemaphore =
        vk::raii::Semaphore(device, vk::SemaphoreCreateInfo());
    drawFence =
        vk::raii::Fence(device, {.flags = vk::FenceCreateFlagBits::eSignaled});
  }

  void prepareNodes(vk::raii::Device &device,
                    Renderer::VSwapChain &vSwapChain) {

    auto fenceResult = device.waitForFences(*drawFence, vk::True, UINT64_MAX);
    if (fenceResult != vk::Result::eSuccess) {
      throw std::runtime_error("failed to wait for fence!");
    }

    auto [_, acquiredImageIndex] = vSwapChain.swapChain.acquireNextImage(
        UINT64_MAX, *presentCompleteSemaphore, nullptr);

    imageIndex = acquiredImageIndex;

    for (RenderNode &node : renderNodes) {
      node.perFrame1_recordCommandBuffer(vSwapChain, imageIndex);
    }

    device.resetFences(*drawFence);
  }

  void submit(vk::raii::Queue &graphicsQueue,
              Renderer::VSwapChain &vSwapChain) {

    vk::PipelineStageFlags waitDestinationStageMask(
        vk::PipelineStageFlagBits::eColorAttachmentOutput);

    std::vector<vk::CommandBuffer> commandBuffers;

    for (RenderNode &node : renderNodes) {
      commandBuffers.push_back(node.commandBuffer);
    }

    const vk::SubmitInfo submitInfo{
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &*presentCompleteSemaphore,
        .pWaitDstStageMask = &waitDestinationStageMask,
        .commandBufferCount = static_cast<uint32_t>(commandBuffers.size()),
        .pCommandBuffers = commandBuffers.data(),
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &*renderFinishedSemaphore};
    graphicsQueue.submit(submitInfo, *drawFence);

    const vk::PresentInfoKHR presentInfoKHR{
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &*renderFinishedSemaphore,
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
  }
};

} // namespace Renderer
