#pragma once

#include "../../../utils/file.h"
#include "../images/vImage.h"
#include "../renderGraph/renderGraphUtils.h"
#include "../shaders/shaders.h"
#include "../vSwapChain.h"
#include "renderNodeUtils.h"
#include "vulkan/vulkan.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include <utility>
#include <vector>
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

enum class PipelineKind { Graphics, Compute };

struct RenderNode {
  vk::raii::ShaderModule shaderModule = nullptr;
  std::vector<RenderNodeUtils::ShaderCreateInfo> shaderCreateInfos;
  vk::raii::PipelineLayout pipelineLayout = nullptr;
  std::vector<PipelineGroup> pipelineGroups;

  std::array<std::unique_ptr<Images::VImage>,
             RenderNodeUtils::MAX_FRAMES_IN_FLIGHT>
      outputs;
  Renderer::RenderGraph::Context *renderGraphContext = nullptr;
  bool usePushConstants = false;
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
};
} // namespace Renderer
