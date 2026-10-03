#pragma once

#include "images/vImageManager.h"
#include "renderGraph/renderGraphUtils.h"
#include "renderNode/renderNodeUtils.h"
#include "vDevice.h"

#include <cstdint>
#include <vulkan/vulkan_raii.hpp>
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS

namespace Renderer {
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
            .stageFlags = vk::ShaderStageFlagBits::eVertex |
                          vk::ShaderStageFlagBits::eFragment},
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
        // Just one. This initial storage buffer is for vertex animation data
        // that will be uploaded just once.
        vk::DescriptorPoolSize{.type = vk::DescriptorType::eStorageBuffer,
                               .descriptorCount =
                                   RenderNodeUtils::MAX_FRAMES_IN_FLIGHT}};

    vk::DescriptorPoolCreateInfo poolInfo{
        .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
        .maxSets = RenderNodeUtils::MAX_FRAMES_IN_FLIGHT,
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

  void updateUniformBuffers(uint32_t frameIndex,
                            Renderer::RenderGraph::Context &context) {
    memcpy(context.globalUniformBankBuffers[frameIndex].mapped,
           &context.globalUniformBufferData,
           sizeof(context.globalUniformBufferData));
  }
};
} // namespace Renderer
