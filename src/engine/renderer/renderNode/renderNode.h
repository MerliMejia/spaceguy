#pragma once

#include "../../../utils/file.h"
#include "../renderGraph/renderGraphUtils.h"
#include "../vSwapChain.h"
#include "vulkan/vulkan.hpp"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>
#define GLM_FORCE_RADIANS
#include "../images/vImage.h"
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
  bool isDepthPass = false;
  vk::Format colorFormat;
  vk::Format depthFormat;
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

  std::array<std::unique_ptr<Images::VImage>,
             RenderNodeUtils::MAX_FRAMES_IN_FLIGHT>
      outputs;
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
      vk::raii::Device &device, step2_pipelineConfigurationProps props,
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
        .depthBiasEnable = props.isDepthPass ? vk::True : vk::False,
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
            {}};

    if (props.useDepth) {
      pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>()
          .pDepthStencilState = &depthStencil;
      pipelineCreateInfoChain.get<vk::PipelineRenderingCreateInfo>()
          .depthAttachmentFormat = props.depthFormat;
    }

    if (props.isDepthPass) {
      pipelineCreateInfoChain.get<vk::PipelineRenderingCreateInfo>()
          .colorAttachmentCount = 0;
    } else {
      pipelineCreateInfoChain.get<vk::PipelineRenderingCreateInfo>()
          .colorAttachmentCount = 1;
      pipelineCreateInfoChain.get<vk::PipelineRenderingCreateInfo>()
          .pColorAttachmentFormats = &props.colorFormat;
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

  void step3_initCommandBuffer(vk::raii::Device &device,
                               vk::raii::CommandPool &commandPool) {
    vk::CommandBufferAllocateInfo allocInfo{
        .commandPool = commandPool,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = RenderNodeUtils::MAX_FRAMES_IN_FLIGHT};

    commandBuffers = vk::raii::CommandBuffers(device, allocInfo);
  }

  void recordCommandBuffer(float width, float height, uint32_t frameIndex) {

    auto &cmd = commandBuffers[frameIndex];

    cmd.setViewport(0, vk::Viewport(0.0f, 0.0f, width, height, 0.0f, 1.0f));
    cmd.setScissor(
        0, vk::Rect2D(vk::Offset2D(0, 0),
                      vk::Extent2D{.width = static_cast<uint32_t>(width),
                                   .height = static_cast<uint32_t>(height)}));

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
