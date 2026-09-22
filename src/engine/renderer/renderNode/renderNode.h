#pragma once

#include "../../../utils/file.h"
#include "../bufferUtils.h"
#include "../images/vImageManager.h"
#include "../renderGraphUtils.h"
#include "../vSwapChain.h"
#include "vulkan/vulkan.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>
#define GLM_FORCE_RADIANS
#include "../images/vTexture.h"
#include "../shaders/shaders.h"
#include "renderNodeUtils.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace Renderer {

struct step1_initShadersProps {
  std::string shaderFile;
};

// A pipeline plus the draws that use it.
struct PipelineGroup {
  vk::raii::Pipeline pipeline = nullptr;
  std::vector<RenderNodeUtils::RenderCall> renderCalls;
};

struct step2_pipelineConfigurationProps {
  vk::PrimitiveTopology topology = vk::PrimitiveTopology::eTriangleList;
  bool useDepth = false;
  vk::Format depthFormat = vk::Format::eD32Sfloat;
  vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1;
  bool useMultiSampling = false;
  //... More stuff when dealing with more stuff
};

struct RenderNode {
  vk::raii::ShaderModule shaderModule = nullptr;
  std::vector<RenderNodeUtils::ShaderCreateInfo> shaderCreateInfos;
  vk::raii::PipelineLayout pipelineLayout = nullptr;
  std::vector<PipelineGroup> pipelineGroups;
  std::vector<vk::raii::CommandBuffer> commandBuffers;

  std::unique_ptr<Images::VImage> input = nullptr;
  std::unique_ptr<Images::VImage> output = nullptr;

  Renderer::RenderGraph::Context *renderGraphContext = nullptr;
  bool usePushConstants = false;
  bool updateUniforms = false;
  std::function<void(Renderer::VSwapChain &vSwapChain, uint32_t imageIndex,
                     uint32_t frameIndex)>
      perFrameFunction;

  void preInit(Renderer::RenderGraph::Context &ctx) {
    renderGraphContext = &ctx;
  }

  void step1_initShaders(vk::raii::Device &device,
                         step1_initShadersProps props) {
    shaderModule =
        RenderNodeUtils::createShaderModule(readFile(props.shaderFile), device);
  }

  void step_1_2_createUniformBuffers(VDevice &vDevice) {
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

      renderGraphContext->globalUniformBankBuffers.emplace_back(
          std::move(newUniformBuffer));
    }
  }

  // I'll manually define this in a way that each node can use it however it
  // wants. Right now 0,0 -> uniform bank, 1,0 -> sampler, 2,0 0 -> texture
  // bank.
  void step_1_3_createDescriptorSetLayout(vk::raii::Device &device) {
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
    renderGraphContext->defaultDescriptorSetLayout =
        vk::raii::DescriptorSetLayout(device, layoutInfo);
  }

  void step_1_4_createDescriptorPool(vk::raii::Device &device) {
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

    renderGraphContext->defaultDescriptorPool =
        vk::raii::DescriptorPool(device, poolInfo);
  }

  void step_1_5_allocateDescriptorSets(vk::raii::Device &device) {
    std::vector<vk::DescriptorSetLayout> layouts(
        RenderNodeUtils::MAX_FRAMES_IN_FLIGHT,
        *renderGraphContext->defaultDescriptorSetLayout);

    vk::DescriptorSetAllocateInfo allocInfo{
        .descriptorPool = renderGraphContext->defaultDescriptorPool,
        .descriptorSetCount = static_cast<uint32_t>(layouts.size()),
        .pSetLayouts = layouts.data()};

    renderGraphContext->defaultDescriptorSets =
        device.allocateDescriptorSets(allocInfo);
  }

  void step_1_6_configureDescriptorSets(
      vk::raii::Device &device, Renderer::Images::VManager &vTextureManager) {

    for (size_t i = 0; i < RenderNodeUtils::MAX_FRAMES_IN_FLIGHT; i++) {
      vk::DescriptorBufferInfo bufferInfo{
          .buffer = renderGraphContext->globalUniformBankBuffers[i].buffer,
          .offset = 0,
          .range = sizeof(RenderGraph::Context::GlobalUniformBankBuffer)};

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
          .buffer = renderGraphContext->vertAnimSBBankAllocations.buffer,
          .offset = 0,
          .range = sizeof(RenderGraph::Context::VerAnimSSBank)};

      std::array<vk::WriteDescriptorSet, 4> writes{
          vk::WriteDescriptorSet{
              .dstSet = *renderGraphContext->defaultDescriptorSets[i],
              .dstBinding = 0,
              .dstArrayElement = 0,
              .descriptorCount = 1,
              .descriptorType = vk::DescriptorType::eUniformBuffer,
              .pBufferInfo = &bufferInfo,
          },
          vk::WriteDescriptorSet{
              .dstSet = *renderGraphContext->defaultDescriptorSets[i],
              .dstBinding = 1,
              .dstArrayElement = 0,
              .descriptorCount = 1,
              .descriptorType = vk::DescriptorType::eSampler,
              .pImageInfo = &samplerInfo,
          },
          {
              .dstSet = *renderGraphContext->defaultDescriptorSets[i],
              .dstBinding = 2,
              .dstArrayElement = 0,
              .descriptorCount = static_cast<uint32_t>(imageInfos.size()),
              .descriptorType = vk::DescriptorType::eSampledImage,
              .pImageInfo = imageInfos.data(),
          }};

      writes[3] = vk::WriteDescriptorSet{
          .dstSet = *renderGraphContext->defaultDescriptorSets[i],
          .dstBinding = 3,
          .dstArrayElement = 0,
          .descriptorCount = 1,
          .descriptorType = vk::DescriptorType::eStorageBuffer,
          .pBufferInfo = &vertexAnimationStorageBufferBankInfo,
      };

      device.updateDescriptorSets(writes, {});
    }
  }

  void step2_createPipelineLayout(vk::raii::Device &device) {
    vk::PushConstantRange pushConstantRange{
        .stageFlags = vk::ShaderStageFlagBits::eVertex |
                      vk::ShaderStageFlagBits::eFragment,
        .offset = 0,
        .size = sizeof(Shaders::PushConstantsBank::PushConstantData)};

    vk::PipelineLayoutCreateInfo pipelineLayoutInfo{
        .setLayoutCount = 1,
        .pSetLayouts = &*renderGraphContext->defaultDescriptorSetLayout};

    if (usePushConstants) {
      pipelineLayoutInfo.pushConstantRangeCount = 1;
      pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
    }

    pipelineLayout = vk::raii::PipelineLayout(device, pipelineLayoutInfo);
  }

  template <RenderNodeUtils::VertexType T>
  size_t step2_addPipeline(
      vk::raii::Device &device, vk::SurfaceFormatKHR &swapChainSurfaceFormat,
      step2_pipelineConfigurationProps props,
      const std::vector<RenderNodeUtils::ShaderCreateInfo> &shaderCreateInfos) {

    std::vector<vk::PipelineShaderStageCreateInfo> shaderStages;
    for (const RenderNodeUtils::ShaderCreateInfo &info : shaderCreateInfos) {
      if (info.type == RenderNodeUtils::ShaderType::Vertex) {
        shaderStages.push_back(vk::PipelineShaderStageCreateInfo{
            .stage = vk::ShaderStageFlagBits::eVertex,
            .module = *shaderModule,
            .pName = info.name.c_str()});
      } else if (info.type == RenderNodeUtils::ShaderType::Fragment) {
        shaderStages.push_back(vk::PipelineShaderStageCreateInfo{
            .stage = vk::ShaderStageFlagBits::eFragment,
            .module = *shaderModule,
            .pName = info.name.c_str()});
      }
    }

    auto bindingDescription = T::getBindingDescription();
    auto attributeDescriptions = T::getAttributeDescriptions();

    vk::PipelineVertexInputStateCreateInfo vertexInputInfo{
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &bindingDescription,
        .vertexAttributeDescriptionCount =
            static_cast<uint32_t>(attributeDescriptions.size()),
        .pVertexAttributeDescriptions = attributeDescriptions.data()};

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly{.topology =
                                                               props.topology};
    vk::PipelineViewportStateCreateInfo viewportState{.viewportCount = 1,
                                                      .scissorCount = 1};

    vk::PipelineRasterizationStateCreateInfo rasterizer{
        .depthClampEnable = vk::False,
        .rasterizerDiscardEnable = vk::False,
        .polygonMode = vk::PolygonMode::eFill,
        .cullMode = vk::CullModeFlagBits::eBack,
        .frontFace = vk::FrontFace::eCounterClockwise,
        .depthBiasEnable = vk::False,
        .lineWidth = 1.0f};

    vk::PipelineMultisampleStateCreateInfo multisampling{};

    if (props.useMultiSampling) {
      multisampling.rasterizationSamples = props.samples;
    } else {
      multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;
    }

    vk::PipelineColorBlendAttachmentState colorBlendAttachment{
        .blendEnable = vk::False,
        .colorWriteMask =
            vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
            vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA};

    vk::PipelineColorBlendStateCreateInfo colorBlending{
        .logicOpEnable = vk::False,
        .logicOp = vk::LogicOp::eCopy,
        .attachmentCount = 1,
        .pAttachments = &colorBlendAttachment};

    std::vector<vk::DynamicState> dynamicStates = {vk::DynamicState::eViewport,
                                                   vk::DynamicState::eScissor};
    vk::PipelineDynamicStateCreateInfo dynamicState{
        .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
        .pDynamicStates = dynamicStates.data()};

    vk::PipelineDepthStencilStateCreateInfo depthStencil{
        .depthTestEnable = vk::True,
        .depthWriteEnable = vk::True,
        .depthCompareOp = vk::CompareOp::eLess,
        .depthBoundsTestEnable = vk::False,
        .stencilTestEnable = vk::False};

    vk::StructureChain<vk::GraphicsPipelineCreateInfo,
                       vk::PipelineRenderingCreateInfo>
        pipelineCreateInfoChain = {
            {.stageCount = static_cast<std::uint32_t>(shaderStages.size()),
             .pStages = shaderStages.data(),
             .pVertexInputState = &vertexInputInfo,
             .pInputAssemblyState = &inputAssembly,
             .pViewportState = &viewportState,
             .pRasterizationState = &rasterizer,
             .pMultisampleState = &multisampling,
             .pColorBlendState = &colorBlending,
             .pDynamicState = &dynamicState,
             .layout = *pipelineLayout,
             .renderPass = nullptr},
            {.colorAttachmentCount = 1,
             .pColorAttachmentFormats = &swapChainSurfaceFormat.format}};

    if (props.useDepth) {
      pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>()
          .pDepthStencilState = &depthStencil;
      pipelineCreateInfoChain.get<vk::PipelineRenderingCreateInfo>()
          .depthAttachmentFormat = props.depthFormat;
    }

    PipelineGroup group;
    group.pipeline = vk::raii::Pipeline(
        device, nullptr,
        pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>());
    pipelineGroups.push_back(std::move(group));

    return pipelineGroups.size() - 1;
  }

  void clearRenderCalls() {
    for (PipelineGroup &group : pipelineGroups) {
      group.renderCalls.clear();
    }
  }
  // The initial image layout transition is from eUndefined to
  // eColorAttachmentOptimal assumming that by default we always want to draw a
  // color attachment. This should change later so I can define what's the
  // initial layout transition. Probably eUndefined to depth, stencil, other?
  void step3_initCommandBuffer(uint32_t queueIndex, vk::raii::Device &device,
                               vk::raii::CommandPool &commandPool) {
    vk::CommandBufferAllocateInfo allocInfo{
        .commandPool = commandPool,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = RenderNodeUtils::MAX_FRAMES_IN_FLIGHT};

    commandBuffers = vk::raii::CommandBuffers(device, allocInfo);
  }

  // For now, just the global uniform bank.
  // I think it depends on the node to update these.
  // One node can do this but not all of the nodes.
  void perFrame1_updateUniformBuffers(uint32_t frameIndex) {
    memcpy(renderGraphContext->globalUniformBankBuffers[frameIndex].mapped,
           &renderGraphContext->globalUniformBufferData,
           sizeof(renderGraphContext->globalUniformBufferData));
  }

  void recordCommandBuffer(Renderer::VSwapChain &vSwapChain,
                           uint32_t frameIndex) {

    auto &cmd = commandBuffers[frameIndex];

    cmd.setViewport(
        0, vk::Viewport(0.0f, 0.0f,
                        static_cast<float>(vSwapChain.swapChainExtent.width),
                        static_cast<float>(vSwapChain.swapChainExtent.height),
                        0.0f, 1.0f));
    cmd.setScissor(0,
                   vk::Rect2D(vk::Offset2D(0, 0), vSwapChain.swapChainExtent));

    // Bound once: every pipeline in the node shares this layout, so it stays
    // valid across bindPipeline calls.
    cmd.bindDescriptorSets(
        vk::PipelineBindPoint::eGraphics, pipelineLayout, 0,
        *renderGraphContext->defaultDescriptorSets[frameIndex], nullptr);

    for (PipelineGroup &group : pipelineGroups) {
      if (group.renderCalls.empty()) {
        continue;
      }

      cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *group.pipeline);

      for (const RenderNodeUtils::RenderCall &renderCall : group.renderCalls) {
        if (usePushConstants) {
          renderCall.updatePushConstants();

          cmd.pushConstants(
              *pipelineLayout,
              vk::ShaderStageFlagBits::eVertex |
                  vk::ShaderStageFlagBits::eFragment,
              0, sizeof(Shaders::PushConstantsBank::PushConstantData),
              &renderGraphContext->pushConstantBank);
        }

        cmd.bindVertexBuffers(0, renderCall.vertexBuffer, {0});
        cmd.bindIndexBuffer(renderCall.indexBuffer, 0, vk::IndexType::eUint32);
        cmd.drawIndexed(static_cast<uint32_t>(renderCall.indexCount), 1, 0, 0,
                        0);
      }
    }

    cmd.endRendering();
  }
};
} // namespace Renderer
