#pragma once

#include "bufferUtils.h"
#include "images/vImageManager.h"
#include "renderGraph/renderGraphUtils.h"
#include "renderNode/computeNode.h"
#include "renderNode/renderNode.h"
#include "renderNode/renderNodeUtils.h"
#include "vDevice.h"
#include "vulkan/vulkan.hpp"

#include <cstdint>
#include <vulkan/vulkan_raii.hpp>

namespace Renderer {

namespace VBarriers {

struct BufferTransitionState {
  vk::PipelineStageFlags2 stage = vk::PipelineStageFlagBits2::eNone;
  vk::AccessFlags2 access = vk::AccessFlagBits2::eNone;
};

inline void bufferTransition(vk::raii::CommandBuffer &commandBuffer,
                             BufferTransitionState &oldState,
                             BufferTransitionState newState) {

  vk::MemoryBarrier2 prepareForCopy{
      .srcStageMask = oldState.stage,
      .srcAccessMask = oldState.access,
      .dstStageMask = newState.stage,
      .dstAccessMask = newState.access,
  };

  vk::DependencyInfo prepareDependency{.memoryBarrierCount = 1,
                                       .pMemoryBarriers = &prepareForCopy

  };

  commandBuffer.pipelineBarrier2(prepareDependency);
  oldState = newState;
}

struct VBarrier {
  BufferTransitionState state{};

  void bTransition(vk::raii::CommandBuffer &commandBuffer,
                   BufferTransitionState newState) {
    bufferTransition(commandBuffer, state, newState);
  }
};
} // namespace VBarriers

struct VConfig {
  void createUniformBuffers(VDevice &vDevice,
                            Renderer::RenderGraph::Context &context) {
    // For now 1 per frame in flight. At some point I may want something that is
    // more static.
    for (size_t i = 0; i < RenderNodeUtils::MAX_FRAMES_IN_FLIGHT; i++) {

      vk::DeviceSize bufferSize =
          sizeof(RenderGraph::Context::GlobalUniformBankBuffer);
      BufferAllocationWithMapped newUniformBuffer{};

      BufferAllocation alloc = createBuffer(
          vDevice, bufferSize, vk::BufferUsageFlagBits::eUniformBuffer,
          vk::MemoryPropertyFlagBits::eHostVisible |
              vk::MemoryPropertyFlagBits::eHostCoherent);

      newUniformBuffer.buffer = std::move(alloc.buffer);
      newUniformBuffer.memory = std::move(alloc.memory);
      newUniformBuffer.mapped =
          newUniformBuffer.memory.mapMemory(0, bufferSize);

      context.globalUniformBankBuffers.emplace_back(
          std::move(newUniformBuffer));
    }
  }

  void createDescriptorSetLayout(VDevice &vDevice,
                                 Renderer::RenderGraph::Context &context) {
    std::array<vk::DescriptorSetLayoutBinding, 4> bingdings{
        vk::DescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = vk::DescriptorType::eUniformBuffer,
            .descriptorCount = 1,
            .stageFlags =
                vk::ShaderStageFlagBits::eVertex |
                vk::ShaderStageFlagBits::eFragment |
                vk::ShaderStageFlagBits::
                    eCompute}, // Also need eCompute because set 1 (compute
                               // shaders) will use this uniforms bank
        vk::DescriptorSetLayoutBinding{
            .binding = 1,
            .descriptorType = vk::DescriptorType::eSampler,
            .descriptorCount = 1,
            .stageFlags = vk::ShaderStageFlagBits::eFragment},
        vk::DescriptorSetLayoutBinding{
            .binding = 2,
            .descriptorType = vk::DescriptorType::eSampledImage,
            .descriptorCount = SG_MAX_TEXTURES,
            .stageFlags = vk::ShaderStageFlagBits::eFragment},
        vk::DescriptorSetLayoutBinding{
            .binding = 3,
            .descriptorType = vk::DescriptorType::eStorageBuffer,
            .descriptorCount = 1,
            .stageFlags = vk::ShaderStageFlagBits::eVertex |
                          vk::ShaderStageFlagBits::eFragment}};

    vk::DescriptorSetLayoutCreateInfo layoutInfo{
        .bindingCount = static_cast<uint32_t>(bingdings.size()),
        .pBindings = bingdings.data()};

    context.defaultDescriptorSetLayout =
        vk::raii::DescriptorSetLayout(vDevice.device, layoutInfo);
  }

  void
  createComputeDescriptorSetLayout(VDevice &vDevice,
                                   Renderer::RenderGraph::Context &context) {
    std::array<vk::DescriptorSetLayoutBinding, 1> bingdings{
        vk::DescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = vk::DescriptorType::eStorageBuffer,
            .descriptorCount = 1,
            .stageFlags = vk::ShaderStageFlagBits::eCompute},
    };

    vk::DescriptorSetLayoutCreateInfo layoutInfo{
        .bindingCount = static_cast<uint32_t>(bingdings.size()),
        .pBindings = bingdings.data()};

    context.computeDescriptorSetLayout =
        vk::raii::DescriptorSetLayout(vDevice.device, layoutInfo);
  }

  void createDescriptorPool(vk::raii::Device &device,
                            Renderer::RenderGraph::Context &context) {
    std::array<vk::DescriptorPoolSize, 4> poolSizes = {
        vk::DescriptorPoolSize{.type = vk::DescriptorType::eUniformBuffer,
                               .descriptorCount =
                                   RenderNodeUtils::MAX_FRAMES_IN_FLIGHT},
        vk::DescriptorPoolSize{.type = vk::DescriptorType::eSampler,
                               .descriptorCount =
                                   RenderNodeUtils::MAX_FRAMES_IN_FLIGHT},
        // Each frame in flight to have 20 sampled textures.
        vk::DescriptorPoolSize{.type = vk::DescriptorType::eSampledImage,
                               .descriptorCount =
                                   RenderNodeUtils::MAX_FRAMES_IN_FLIGHT *
                                   SG_MAX_TEXTURES},
        // For now, 1 bind for graphics pipeline and 1 for compute per frame in
        // flight.
        vk::DescriptorPoolSize{.type = vk::DescriptorType::eStorageBuffer,
                               .descriptorCount =
                                   RenderNodeUtils::MAX_FRAMES_IN_FLIGHT * 2}};

    vk::DescriptorPoolCreateInfo poolInfo{
        .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
        // For now, 1 bind for graphics pipeline and 1 for compute per frame in
        // flight.
        .maxSets = RenderNodeUtils::MAX_FRAMES_IN_FLIGHT * 2,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes = poolSizes.data()};

    context.defaultDescriptorPool = vk::raii::DescriptorPool(device, poolInfo);
  }

  void allocateDescriptorSets(vk::raii::Device &device,
                              Renderer::RenderGraph::Context &context) {
    std::vector<vk::DescriptorSetLayout> layouts(
        RenderNodeUtils::MAX_FRAMES_IN_FLIGHT,
        *context.defaultDescriptorSetLayout);

    vk::DescriptorSetAllocateInfo allocInfo{
        .descriptorPool = context.defaultDescriptorPool,
        .descriptorSetCount = static_cast<uint32_t>(layouts.size()),
        .pSetLayouts = layouts.data()};

    context.defaultDescriptorSets = device.allocateDescriptorSets(allocInfo);
  }

  void allocateComputeDescriptorSets(vk::raii::Device &device,
                                     Renderer::RenderGraph::Context &context) {
    std::vector<vk::DescriptorSetLayout> layouts(
        RenderNodeUtils::MAX_FRAMES_IN_FLIGHT,
        *context.computeDescriptorSetLayout);

    vk::DescriptorSetAllocateInfo allocInfo{
        .descriptorPool = context.defaultDescriptorPool,
        .descriptorSetCount = static_cast<uint32_t>(layouts.size()),
        .pSetLayouts = layouts.data()};

    context.computeDescriptorSets = device.allocateDescriptorSets(allocInfo);
  }

  void configureDescriptorSets(vk::raii::Device &device,
                               Renderer::RenderGraph::Context &context,
                               Renderer::Images::VManager &vTextureManager) {

    for (size_t i = 0; i < RenderNodeUtils::MAX_FRAMES_IN_FLIGHT; i++) {
      vk::DescriptorBufferInfo bufferInfo{
          .buffer = context.globalUniformBankBuffers[i].buffer,
          .offset = 0,
          .range =
              sizeof(Renderer::RenderGraph::Context::GlobalUniformBankBuffer)};

      vk::DescriptorImageInfo samplerInfo{
          .sampler = *vTextureManager.sampler,
          .imageView = {},
          .imageLayout = vk::ImageLayout::eUndefined,
      };

      std::array<vk::DescriptorImageInfo, SG_MAX_TEXTURES> imageInfos;

      for (size_t i = 0; i < imageInfos.size(); i++) {
        imageInfos[i] = vk::DescriptorImageInfo{
            .sampler = {},
            .imageView = *vTextureManager.textures[i]->vImage.view,
            .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
        };
      }

      vk::DescriptorBufferInfo vertexAnimationStorageBufferBankInfo{
          .buffer = context.vertAnimSBBankAllocations.buffer,
          .offset = 0,
          .range = sizeof(Renderer::RenderGraph::Context::VerAnimSSBank)};

      std::array<vk::WriteDescriptorSet, 4> writes{
          vk::WriteDescriptorSet{
              .dstSet = *context.defaultDescriptorSets[i],
              .dstBinding = 0,
              .dstArrayElement = 0,
              .descriptorCount = 1,
              .descriptorType = vk::DescriptorType::eUniformBuffer,
              .pBufferInfo = &bufferInfo,
          },
          vk::WriteDescriptorSet{
              .dstSet = *context.defaultDescriptorSets[i],
              .dstBinding = 1,
              .dstArrayElement = 0,
              .descriptorCount = 1,
              .descriptorType = vk::DescriptorType::eSampler,
              .pImageInfo = &samplerInfo,
          },
          {
              .dstSet = *context.defaultDescriptorSets[i],
              .dstBinding = 2,
              .dstArrayElement = 0,
              .descriptorCount = static_cast<uint32_t>(imageInfos.size()),
              .descriptorType = vk::DescriptorType::eSampledImage,
              .pImageInfo = imageInfos.data(),
          }};

      writes[3] = vk::WriteDescriptorSet{
          .dstSet = *context.defaultDescriptorSets[i],
          .dstBinding = 3,
          .dstArrayElement = 0,
          .descriptorCount = 1,
          .descriptorType = vk::DescriptorType::eStorageBuffer,
          .pBufferInfo = &vertexAnimationStorageBufferBankInfo,
      };

      device.updateDescriptorSets(writes, {});
    }
  }

  void configureComputeDescriptorSets(vk::raii::Device &device,
                                      Renderer::RenderGraph::Context &context) {

    for (size_t i = 0; i < RenderNodeUtils::MAX_FRAMES_IN_FLIGHT; i++) {

      vk::DescriptorBufferInfo particlesDescriptorBufferInfo{
          .buffer = context.particlesAllocations.buffer,
          .offset = 0,
          .range = sizeof(Renderer::RenderGraph::Context::ParticlesData)};

      std::array<vk::WriteDescriptorSet, 1> writes{vk::WriteDescriptorSet{
          .dstSet = *context.computeDescriptorSets[i],
          .dstBinding = 0,
          .dstArrayElement = 0,
          .descriptorCount = 1,
          .descriptorType = vk::DescriptorType::eStorageBuffer,
          .pBufferInfo = &particlesDescriptorBufferInfo,
      }};

      device.updateDescriptorSets(writes, {});
    }
  }

  void updateUniformBuffers(uint32_t frameIndex,
                            Renderer::RenderGraph::Context &context) {
    memcpy(context.globalUniformBankBuffers[frameIndex].mapped,
           &context.globalUniformBufferData,
           sizeof(context.globalUniformBufferData));
  }

  void
  initCommandBuffers(vk::raii::Device &device,
                     vk::raii::CommandPool &commandPool,
                     std::vector<vk::raii::CommandBuffer> &commandBuffers) {
    vk::CommandBufferAllocateInfo allocInfo{
        .commandPool = commandPool,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = RenderNodeUtils::MAX_FRAMES_IN_FLIGHT};

    commandBuffers = vk::raii::CommandBuffers(device, allocInfo);
  }

  void recordCommandBuffer(float width, float height,
                           Renderer::RenderNode &node,
                           vk::raii::CommandBuffer &commandBuffer,
                           Renderer::RenderGraph::Context &context,
                           uint32_t frameIndex) {

    auto &pipelineLayout = node.pipelineLayout;
    auto &descriptorSet = context.defaultDescriptorSets[frameIndex];

    commandBuffer.setViewport(
        0, vk::Viewport(0.0f, 0.0f, width, height, 0.0f, 1.0f));
    commandBuffer.setScissor(
        0, vk::Rect2D(vk::Offset2D(0, 0),
                      vk::Extent2D{.width = static_cast<uint32_t>(width),
                                   .height = static_cast<uint32_t>(height)}));

    // Bound once: every pipeline in the node shares this layout, so it stays
    // valid across bindPipeline calls.
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                     pipelineLayout, 0, *descriptorSet,
                                     nullptr);

    for (PipelineGroup &group : node.pipelineGroups) {
      if (group.renderCalls.empty()) {
        continue;
      }

      commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                 *group.pipeline);

      for (const RenderNodeUtils::RenderCall &renderCall : group.renderCalls) {
        if (node.usePushConstants) {
          renderCall.updatePushConstants();

          commandBuffer.pushConstants(
              *pipelineLayout,
              vk::ShaderStageFlagBits::eVertex |
                  vk::ShaderStageFlagBits::eFragment,
              0, sizeof(Shaders::PushConstantsBank::PushConstantData),
              &context.pushConstantBank);
        }

        commandBuffer.bindVertexBuffers(0, renderCall.vertexBuffer, {0});
        commandBuffer.bindIndexBuffer(renderCall.indexBuffer, 0,
                                      vk::IndexType::eUint32);
        commandBuffer.drawIndexed(static_cast<uint32_t>(renderCall.indexCount),
                                  1, 0, 0, 0);
      }
    }
  }

  void recordComputeCommandBuffer(Renderer::ComputeNode &node,
                                  vk::raii::CommandBuffer &commandBuffer,
                                  Renderer::RenderGraph::Context &context,
                                  uint32_t frameIndex) {

    auto &pipelineLayout = node.pipelineLayout;
    auto &graphicsSet = context.defaultDescriptorSets[frameIndex];
    auto &computesSet = context.computeDescriptorSets[frameIndex];

    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute,
                                     pipelineLayout, 0, *graphicsSet, nullptr);
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute,
                                     pipelineLayout, 1, *computesSet, nullptr);

    commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, node.pipeline);

    if (node.usePushConstants) {
      node.updatePushConstants();

      commandBuffer.pushConstants(
          *pipelineLayout, vk::ShaderStageFlagBits::eCompute, 0,
          sizeof(Shaders::PushConstantsBank::PushConstantData),
          &context.pushConstantBank);
    }
  }
};
} // namespace Renderer
